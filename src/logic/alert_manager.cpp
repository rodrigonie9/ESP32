#include "alert_manager.h"
#include "../sensors/sensors.h"
#include "../sensors/sensor_registry.h"
#include "../telegram/telegram.h"
#include "../debug.h"
#include "../config/system_limits.h"
#include <time.h>


// Estrutura para controlar o tempo de cad sensora na RAM
struct EstadoAlerta {
    unsigned long inicioAlertaSuave = 0;
    unsigned long inicioAlertaCritico = 0;
    bool alertaEnviado = false;
};

static EstadoAlerta estados[MAX_SENSORES];

// --- FUNÇÃO AUXILIAR: VERIFICA AGENDA ---
// Retorna true = pode monitrar / false = fora da agenda, ignorar
bool estaNoHorarioDeMonitoramento (const SensorConfig& s) {

    // Busca a hora atual do sistema (vinda NTP)
    struct tm timeinfo;
    // Se relógio não estiver sincronizado, monitora por segurança
    if (!getLocalTime(&timeinfo)) return true;

    // ── PASSO 1: O DIA DE HOJE ESTÁ ATIVO NO BITMASK? ──────────────────      
    //
    // dias_monitoramento é um número de 8 bits — pensa como 7 lâmpadas:        
    //   bit:  6    5    4    3    2    1    0
    //   dia: Sab  Sex  Qui  Qua  Ter  Seg  Dom
    //
    // Exemplo dias úteis: 0b0111110 = 62  (bits 1 a 5 acesos)
    // Exemplo todos dias: 0b1111111 = 127 (bits 0 a 6 acesos)
    //
    // Para saber se HOJE está ativo:
    //   >> diaAtual  →  empurra os bits até o dia de hoje chegar na posição 0  
    //   & 1          →  lê só esse bit: 1 = ativo, 0 = inativo
    //     & 1 =  MÁSCARA: apaga todos os bits, deixa só o último (posição 0)  
    // Exemplo: dias úteis (0b0111110), hoje = Quarta (diaAtual=3)
    //   0b0111110 >> 3 = 0b0001111  →  & 1 = 1  →  dia ativo!
    // Exemplo: dias úteis (0b0111110), hoje = Domingo (diaAtual=0)
    //   0b0111110 >> 0 = 0b0111110  →  & 1 = 0  →  dia inativo
    uint8_t diaAtual = timeinfo.tm_wday;
    bool diaAtivo = (s.dias_monitoramento >> diaAtual) & 1;

    if (!diaAtivo) return false;    //hoje não está na agenda

    // ── PASSO 2: USA JANELA DE HORÁRIO? ────────────────────────────────      
    // Se agenda_horario_ativo for false, monitora o dia inteiro (24h)
    if (!s.agenda_horario_ativo) return true;

    
    // ── PASSO 3: ESTAMOS DENTRO DA JANELA DE HORÁRIO? ──────────────────      
    // Converte hora atual e os limites para minutos desde 00:00
    // Facilita comparar dois horários com uma subtração simples
    int minAtual  = (timeinfo.tm_hour * 60) + timeinfo.tm_min;
    int minInicio = (s.hora_inicio    * 60) + s.min_inicio;
    int minFim    = (s.hora_fim       * 60) + s.min_fim;

    // Caso 1: janela normal, não vira meia-noite (ex: 08:00 → 20:00)
    if (minInicio <= minFim) {
        //C++ pode retornar direto uma expressão if else / verdadeiro ou falso
        // Retorna TRUE, se:
        //  AGORA >= minInicio E AGORA < minFim
        return(minAtual >= minInicio && minAtual < minFim);
    }

    // Caso 2: janela vira meia-noite (ex.: 22:00 → 06:00)
    // Monitora das 22h até 23:59 OU das 00:00 até as 06:00
    // Retorna TRUE se:
    //   AGORA >= minInicio OU agora < minFim
    return(minAtual >= minInicio || minAtual < minFim);
}


// --- FUNÇÃO PARA O BOTÃO MUDO ---
void alert_setModoManutencao(const String& id_fisico, uint8_t horas) {
    SensorConfig s;
    // registry_buscardPorID (aponta para as configurações do sensor buscado)
    if (registry_buscarPorID(id_fisico,s)) {
        // converte horas em milisegundos
        // define até quando o sensor vai ficar mudo
        s.mudo_ate = millis() + ((unsigned long)horas *3600000UL);        //transforma 3600000 em Long, int é menor e pode ser negativo
        // grava na memória até quando vai ficar mudo
        registry_salvar(s);
        LOG("Alerta: " + s.nome_amigavel + " silenciado por " + String(horas) + "h");
    }
}

// --- FUNÇÃO PRINCIPAL DE LÓGICA ---
void processarLogicaAlertas() {
    auto cadastrados = registry_getTodos();

    for (const auto& s : cadastrados) {

        if (!s.monitoramento_ativo) continue;

        // 1 - Verifica Modo Manutenção (MUDO)
        //  maior que zero = está em manutenção
        if (s.mudo_ate > 0) {
            // ATENÇÃO: millis() retorna milisecondes desde o boot e transborda para 0 após ~49 dias
            // Nunca usar comparação direta(millis() < alvo), usar subtração (millis()- alvo)
            // MELHORIA:
            // millis() transbora após ~49 dias, 
            // comparando (millis() < mudo_ate), após overflow millis fica menor, que mudo_ate , sensor fica mudo para sempre
            // (millis() - mudo_ate) funciona bem com overflow, subtração do unsigned long transbor de forma previsível
            if ((long)(millis() - s.mudo_ate) < 0) continue;
            else {
                SensorConfig s_upd = s;
                s_upd.mudo_ate = 0;
                registry_salvar(s_upd);
            }
        }

        // 2 - Verifica Agenda
        if (!estaNoHorarioDeMonitoramento(s)) {
            // Se está fora do horário, "Limpamos a memória" de alertas desse sensor
            // Primeiro descobrimos qual é o índice (idx) deste sensor na nossa lista de estados
            int idx = -1;
            for (int i = 0; i < hw_getContagem(); i ++) {
                if (hw_getID(i) == s.id_fisico) {
                    idx = i;
                    break;
                }
            }
            
            // Se achamos o sensor, zeramos todos os alertas dele
            if (idx != -1) {
                estados[idx].inicioAlertaSuave = 0;
                estados[idx].inicioAlertaCritico = 0;
                estados[idx].alertaEnviado = false;
            }

            continue; // pula para o próximo sensor, pois está no horário de folga
        
        }

        // Pega temperatura
        float temp = hw_getTemp(s.id_fisico);

        if (temp == DEVICE_DISCONNECTED_C) continue;

        
        // Busca o índice deste sensor na lista de hardware
        // Precisamos do índice para acesssar estados[idx] - memória de alertas
        int idx = -1;
        for (int i = 0; i < hw_getContagem(); i++) {
            if(hw_getID(i) == s.id_fisico){
                idx = i;
                break; // achou - pode parar de procurar
            }
        }

        // Se o sensor não foi encontrado no hardware, pula
        if (idx == -1) continue;

        // ── 3. ALERTA CRÍTICO ──────────────────────────────────────────
        // Temperatura crítica = perigo imediato, avisa na hora,  sem esperar
        if (temp >= s.temp_critica) {
            if(!estados[idx].alertaEnviado){
                // String (temp,1) = temperatura com 1 casa decima. ex.: "8.3"
                enviarMensagemTelegram("🚨 CRÍTICO: " + s.nome_amigavel + 
                                       " está em " + String(temp,1) + "°C!");
                estados[idx].alertaEnviado = true;
            }
            continue;   //não precisa checar alerta suave
        }

        // ── 4. ALERTA SUAVE ─────────────────────────────────────────────
        // Temperatura acima do limite, mas espera tempo_espera_min antes de avisar
        // Evita alarmes falsos por picos rápidos (ex: porta do freezer aberta)
        if (temp > s.temp_max_alerta) {
            if(estados[idx].inicioAlertaSuave == 0){
                //Primeira vez acima do limite - anota o horário
                estados[idx].inicioAlertaSuave = millis();
            }

            // Quanto tempo já está acima do limite?
            unsigned long tempoEsperando = millis() - estados[idx].inicioAlertaSuave;

            // Converte tempo_espera_min (minutos) para milisegundos
            // 60000UL = 60.000 em unsigned long (evita overflow igual ao mudo_ate) 
            unsigned long limiteMs = (unsigned long)s.tempo_espera_min * 60000UL;

            // Se passou do tempo E ainda não avisou - manda o alerta
            if (tempoEsperando >= limiteMs && !estados[idx].alertaEnviado){
                enviarMensagemTelegram("⚠️ ALERTA: " + s.nome_amigavel +
                                      " está em " + String(temp,1) + "°C há " +
                                      String(s.tempo_espera_min) + " min!");
                estados[idx].alertaEnviado = true;

            }
            continue;
        }
    // ── 5. TEMPERATURA NORMAL ───────────────────────────────────────
    // Temperatura voltou ao normal — reseta tudo para o  próximo ciclo
    estados[idx].inicioAlertaSuave   = 0;
    estados[idx].inicioAlertaCritico = 0;
    estados[idx].alertaEnviado       = false; 

    }

}

  //Para cada sensor:
  //  ├─ Monitoramento desligado? → pula
  //  ├─ Em manutenção (mudo)? → pula
  //  ├─ Fora da agenda? → zera estados e pula
  //  ├─ Sensor desconectado? → pula
  //  ├─ Temp >= crítica? → avisa imediatamente (se ainda não avisou)
  //  ├─ Temp >= alerta? → inicia timer → avisa após tempo_espera_min
  //  └─ Temp normal? → zera tudo (pronto para próximo alerta)



