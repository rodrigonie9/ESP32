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

    // Descobre qual é o dia de hoje (0=Dom, 1=Seg, ... 6=Sab)
    uint8_t diaAtual = timeinfo.tm_wday;

    // Lê o modo configurado para hoje neste sensor
    uint8_t modo = s.dia_modo[diaAtual];

     // ── MODO DESLIGADO ──────────────────────────────────────────────
     // Dia configurado como desligado - não monitora
     if (modo == DIA_DESLIGADO) return false;

      // ── MODO 24H ────────────────────────────────────────────────────
      // Dia configurado para monitorar o dia todo
      if (modo == DIA_24H) return true;

      // ── MODO HORÁRIO ─────────────────────────────────────────────────
      // Usar a janela configurada para ESTE dia especificamente
      // Converte hora atual e os limites para minutos dede 00:00
      // Ex.: 08:30 = (8 * 60) + 30 = 510 minutos do dia - facilita comparação
      int minAtual  = (timeinfo.tm_hour              *60) + timeinfo.tm_min;
      int minInicio = (s.dia_hora_inicio[diaAtual]   *60) + s.dia_min_inicio[diaAtual];
      int minFim    = (s.dia_hora_fim[diaAtual]      *60) + s.dia_min_fim[diaAtual];

      // Caso 1: janela normal, não vira meia-noite (ex: 08:00 → 20:00)
      if (minInicio <= minFim){                                 // JANELA NORMAL - não vira meia noite
        return (minAtual >= minInicio && minAtual < minFim);    // confere se está em horário de monitoramento
                                                                // ja retorna TRUE ou FALSE
      }

      // Caso 2: janela vira meia-noite (ex: 22:00 → 06:00)
      // Monitora das 22h até as 23:59 OU das 00:00 até as 06:00
      return (minAtual >= minInicio || minAtual < minFim);      // retorna TRUE ou FALSE
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



