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
bool estaNoHorarioDeMonitoramento (const SensorConfig& s) {
    if (s.modo == SEMPRE_24H) return true;

    //estrutura timeinfo retorna timeinfo.tm_hour, _min, _sec
    struct tm timeinfo;

    //getLocalTime, função tempo esp32, tenta obter hora atual via NTP(internet)

    if (!getLocalTime(&timeinfo)) return true; // se falhara hora, monitora por segurança 

    /*_wday, retornar dia da semana. ex. 0 dom, 1 seg 
        vezes 1440 (numero de minutos em um dia) wday * 1440 = retorna dia da semana em minutos acumulados
        ex: segunda = 1 * 1440
        logo quantos minutos se passaram desde domingo 00:00
     tm_hour - converte horas do dia em minutos
     tm_min - miuntosv(0 a 59)
     depois soma tudo
     Exemplo: ex: quarta (3) - 14:25
      3 * 1440   +    14 * 60     +  25 = 5185 desde domingo 00:00  */
    int minAtualSemana = (timeinfo.tm_wday * 1440) + (timeinfo.tm_hour * 60) + timeinfo.tm_min;
    int minAtualDia = (timeinfo.tm_hour * 60) + timeinfo.tm_min;

    // MODO AGENDA SEMANAL
    if (s.modo == AGENDA_SEMANAL) {
        //calcula hora em minutos que desliga - depois compara com minutos atual
        int minDesliga = (s.dia_desliga_semanal * 1440) + (s.hora_desliga_semanal * 60) + s.min_desliga_semanal;
        //calcula hora em minutos que religa - depois compara com minutos atual
        int minReliga = (s.dia_religa_semanal * 1440) + (s.hora_religa_semanal * 60) + s.min_religa_semanal;

        
    //AGENDA SEMANAL
        // Confere se está dentro do período DESLIGADO
          // horário de Desligar e Religar está dentro da semana semana, sem virar o relógio
        if (minDesliga < minReliga) {
            // hora atual MAIOR ou IGUAL a hora desligar E menor hora de religar
            // Estamos dentro do período de monitoramento DESLIGADO?
            if (minAtualSemana >= minDesliga && minAtualSemana < minReliga) return false;
          // Período desligado atravess o final de semana (desliga sexta 7800 min) religa segunda(360 minutos)
        } else {
            // Hora atual DEPOIS do horário de desliga OU ANTES do Religar
            //  Cobre do desligar até domingo 23:59 e de domingo domingo 00:00 até religar
            if (minAtualSemana >= minDesliga || minAtualSemana < minReliga) return false;
        }
    }
    
    //AGENDA DIÁRIA
        //Confere se está dentro do período DESLIGADO
    else if (s.modo == AGENDA_DIARIA) {
        // converte tudo para minutos desde 00:00
        int minDesliga = (s.hora_desliga_diaria * 60) + s.min_desliga_diaria;
        int minReliga = (s.hora_religa_diaria * 60) + s.min_religa_diaria;
        
        //Período desligado está dentro do mesmo dia ou vira o dia?
          // 1º caso, não vira o dia
        if (minDesliga < minReliga) {
            // Se agora for DEPOIS de desligar E ANTES de religar
            if (minAtualDia >= minDesliga && minAtualDia < minReliga) return false;
          // 2º caso. vira o dia Ex: desliga 22h, religar 06h
        } else {
            // Se agora for DEPOIS de desligar OU ANTES  de desligar
            if (minAtualDia >= minDesliga || minAtualDia < minReliga) return false;
        }

    }
    return true;
}


// --- FUNÇÃO PARA O BOTÃO MUDO ---
void alert_setModoManutencao(const String& id_fisico, uint8_t horas) {
    SensorConfig s;
    // registry_buscardPorID (aponta para as configurações do sensor buscado)
    if (registry_buscarPorID(id_fisico,s)) {
        // converte horas em milisegundos
        // define até quando o sensor vai ficar mudo
        s.mudo_ate = millis() + ((unsigned long)horas *3600000);
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
            // millis() = tempo atual desde que a esp32 ligou, desligou - retorna a zero, aproximadamente após 49 dias após ser ligada
            if (millis() < s.mudo_ate) continue;
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

        // pegamos o índice interno para gerenciar o tempo
        int idx = -1;
        for (int i = 0; i < hw_getContagem(); i++) {

        }

        

    }
}



