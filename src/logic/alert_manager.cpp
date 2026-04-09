// ═══════════════════════════════════════════════════════════════════════════════
// ALERT MANAGER — LÓGICA DE ALERTAS COM DOIS TIMERS INDEPENDENTES
// ═══════════════════════════════════════════════════════════════════════════════
//
// PROBLEMA QUE ESTE SISTEMA RESOLVE:
//   Câmaras frigoríficas fazem ciclos de degelo automático (~30 min) onde a
//   temperatura sobe bastante. Sem filtro, isso geraria falsos alertas toda noite.
//   Com os dois timers, o sistema aguarda pacientemente antes de avisar —
//   se a temperatura voltar ao normal dentro do tempo configurado, silêncio total.
//
// ───────────────────────────────────────────────────────────────────────────────
// DOIS TIMERS INDEPENDENTES (um por limiar, por sensor)
// ───────────────────────────────────────────────────────────────────────────────
//
//   TIMER SUAVE (temp > temp_max_alerta)
//   ┌─────────────────────────────────────────────────────────────────────────┐
//   │  temp normal                                                            │
//   │     │                                                                   │
//   │     ▼ cruza temp_max_alerta                                             │
//   │  AGUARDANDO ──── temp voltou ao normal antes do tempo de degelo ──────► IDLE
//   │     │            (era degelo normal — silêncio total)                   │
//   │     │ tempo de degelo expirou (padrão: 40 min)                          │
//   │     ▼                                                                   │
//   │  1º AVISO: "⚠️ ALERTA: Câmara X está em Y°C há 40 min!"                │
//   │     │                                                                   │
//   │     ▼ problema persiste                                                 │
//   │  REPETIÇÃO a cada 30 min: "⚠️ AINDA EM ALERTA: Câmara X em Y°C"        │
//   │     │                                                                   │
//   │     ▼ temperatura volta ao normal                                       │
//   │  NORMALIZADO: "✅ Normalizado: Câmara X voltou a Y°C"  ───────────────► IDLE
//   └─────────────────────────────────────────────────────────────────────────┘
//
//   TIMER CRÍTICO (temp >= temp_critica)  ← corre em paralelo, independente
//   ┌─────────────────────────────────────────────────────────────────────────┐
//   │  temp abaixo do crítico                                                 │
//   │     │                                                                   │
//   │     ▼ cruza temp_critica                                                │
//   │  AGUARDANDO ──── temp voltou abaixo do crítico antes do tempo ────────► IDLE
//   │     │            (era pico passageiro — silêncio total)                 │
//   │     │ tempo de espera expirou (padrão: 15 min)                          │
//   │     ▼                                                                   │
//   │  1º AVISO: "🚨 CRÍTICO: Câmara X está em Y°C!"                         │
//   │     │                                                                   │
//   │     ▼ problema persiste                                                 │
//   │  REPETIÇÃO a cada 10 min: "🚨 AINDA CRÍTICO: Câmara X em Y°C"          │
//   │     │                                                                   │
//   │     ▼ temperatura cai abaixo do crítico                                 │
//   │  SAIU DO CRÍTICO: "✅ Saiu do crítico: Câmara X agora em Y°C"           │
//   │     │  (se ainda > temp_max_alerta: "ainda em alerta suave")            │
//   │     ▼                                                                   │
//   │  Timer crítico reseta — timer suave continua rodando normalmente ──────► IDLE
//   └─────────────────────────────────────────────────────────────────────────┘
//
// ───────────────────────────────────────────────────────────────────────────────
// REGRA DE PRIORIDADE — crítico suprime o suave
// ───────────────────────────────────────────────────────────────────────────────
//
//   Quando temp >= temp_critica:
//     → apenas o timer crítico envia mensagens
//     → o timer suave continua rodando (contando o tempo), mas não avisa
//     → isso evita receber dois avisos ao mesmo tempo para a mesma câmara
//
//   Quando temp cai para a faixa suave (entre alerta e crítico):
//     → timer crítico reseta e envia "saiu do crítico"
//     → timer suave assume — já sabe quanto tempo está acima do limite
//
// ───────────────────────────────────────────────────────────────────────────────
// EXEMPLO REAL — câmara em degelo normal (sem nenhum aviso)
// ───────────────────────────────────────────────────────────────────────────────
//
//   00:00  3°C   ambos os timers IDLE
//   00:05  5°C   timer suave inicia (cruzou temp_max_alerta)
//   00:20  9°C   timer crítico inicia (cruzou temp_critica) — suave suprimido
//   00:35  7°C   temperatura caiu abaixo do crítico — timer crítico reseta (silêncio)
//   00:45  3°C   temperatura normal — timer suave reseta (silêncio)
//   resultado:   NENHUM AVISO ENVIADO — era degelo normal
//
// ───────────────────────────────────────────────────────────────────────────────
// EXEMPLO REAL — problema real não resolvido
// ───────────────────────────────────────────────────────────────────────────────
//
//   00:00  3°C   ambos os timers IDLE
//   00:05  5°C   timer suave inicia
//   00:45  5°C   timer suave expirou (40 min) → "⚠️ ALERTA"
//   01:15  5°C   30 min depois → "⚠️ AINDA EM ALERTA"
//   01:45  5°C   30 min depois → "⚠️ AINDA EM ALERTA"
//   02:00  3°C   temperatura normal → "✅ Normalizado"
//
// ───────────────────────────────────────────────────────────────────────────────
// CAMPOS CONFIGURÁVEIS (por sensor, via web portal)
// ───────────────────────────────────────────────────────────────────────────────
//
//   temp_max_alerta          → limiar do alerta suave (ex: 4°C)
//   tempo_degelo_min         → paciência antes do 1º aviso suave (padrão: 40 min)
//   temp_critica             → limiar do alerta crítico (ex: 8°C)
//   tempo_espera_critico_min → paciência antes do 1º aviso crítico (padrão: 15 min)
//
// INTERVALOS DE REPETIÇÃO (fixos no código, linha ~70)
//   REPETICAO_SUAVE_MS   → a cada 30 min enquanto persistir em alerta
//   REPETICAO_CRITICO_MS → a cada 10 min enquanto persistir em crítico
//
// ═══════════════════════════════════════════════════════════════════════════════

#include "alert_manager.h"
#include "../sensors/sensors.h"
#include "../sensors/sensor_registry.h"
#include "../telegram/telegram.h"
#include "../device/device_config.h"
#include "../debug.h"
#include "../config/system_limits.h"
#include <time.h>
#include <Preferences.h>

#define NVS_NAMESPACE_ALERTAS "alert_state"

static Preferences prefsAlerta;


// ── INTERVALOS DE REPETIÇÃO ───────────────────────────────────────────────────
// Enquanto o problema não for resolvido, o sistema continua avisando nestes intervalos
// UL = unsigned long — necessário para multiplicações grandes sem overflow
// 60000UL = 1 minuto em milissegundos
const unsigned long REPETICAO_SUAVE_S   = 30UL * 60UL;  // avisa a cada 30 min se ainda em alerta
const unsigned long REPETICAO_CRITICO_S = 1UL * 60UL;  // avisa a cada 10 min se ainda crítico


// ── ESTADO DE ALERTA POR SENSOR ───────────────────────────────────────────────
// Cada sensor tem dois timers independentes: um para o limiar suave, outro para o crítico
// Ficam na RAM — são zerados ao reiniciar a placa (isso é esperado e correto)
struct EstadoAlerta {

    // Timer do limiar suave (temp > temp_max_alerta)
    time_t        inicioSuave      = 0;     // Unix timestamp de quando cruzou o limiar suave
                                            // 0 = temperatura está normal
    bool          suaveAvisado     = false; // true = primeiro aviso já foi enviado
    unsigned long ultimoAvisoSuave = 0;     // millis() do último aviso — controla repetição

    // Timer do limiar crítico (temp >= temp_critica)
    time_t        inicioCritico      = 0;   // Unix timestamp de quando cruzou o limiar crítico
    bool          criticoAvisado     = false;
    unsigned long ultimoAvisoCritico = 0;

    // Sensor desconectado — conta ciclos consecutivos de erro
    uint8_t ciclosErro    = 0;     // ciclos consecutivos com SENSOR_ERRO
    bool    erroAvisado   = false; // true = já avisou sobre desconexão
};

// Um estado para cada sensor possível — alocado em tempo de compilação
// static = só existe neste arquivo, não vaza para o restante do projeto
static EstadoAlerta estados[MAX_SENSORES];

// ── ESTADO DE ERRO PARA SENSORES SEM HARDWARE ─────────────────────────────────
// Usado quando o sensor está cadastrado no NVS mas NÃO foi detectado no barramento
// no boot (por exemplo: cabo queimado antes do reboot, sensor trocado sem reboot).
// Indexado por id_sensor (0-29) — independente do índice de hardware (hw_idx).
// NÃO pode usar estados[hw_idx] porque o sensor não tem posição no mapa de hardware
// e usar estados[id_sensor] causaria conflito com outro sensor que esteja naquela posição.
struct EstadoSemHardware {
    uint8_t ciclosErro  = 0;    // quantos ciclos consecutivos retornaram DEVICE_DISCONNECTED_C
    bool    erroAvisado = false; // true = Telegram já foi avisado, não repete
};
static EstadoSemHardware semHw[MAX_SENSORES];


// ── FUNÇÕES DE PERSISTÊNCIA NO NVS ────────────────────────────────────────────
// Salva os 4 campos do estado de alerta de um sensor na NVS
// Chamada sempre que o estado muda (timer inicia, aviso enviado, timer zera)
static void salvarEstadoNVS(uint8_t id_sensor, const EstadoAlerta& e) {
    if (id_sensor == 255) return;  // sensor sem id atribuído — não salva
    // Monta as chaves como char[] — Preferences só aceita const char*, não String
    char kIs[6], kSa[6], kIc[6], kCa[6];
    snprintf(kIs, sizeof(kIs), "is_%d", id_sensor);
    snprintf(kSa, sizeof(kSa), "sa_%d", id_sensor);
    snprintf(kIc, sizeof(kIc), "ic_%d", id_sensor);
    snprintf(kCa, sizeof(kCa), "ca_%d", id_sensor);
    prefsAlerta.begin(NVS_NAMESPACE_ALERTAS, false);
    prefsAlerta.putLong(kIs, (long)e.inicioSuave);
    prefsAlerta.putBool(kSa, e.suaveAvisado);
    prefsAlerta.putLong(kIc, (long)e.inicioCritico);
    prefsAlerta.putBool(kCa, e.criticoAvisado);
    prefsAlerta.end();
}

// Carrega do NVS os estados salvos antes do restart — chamada uma vez no boot
void alert_carregarEstados() {
    prefsAlerta.begin(NVS_NAMESPACE_ALERTAS, true);  // true = somente leitura
    auto cadastrados = registry_getTodos();

    for (const auto& s : cadastrados) {
        if (s.id_sensor == 255) continue;  // sem id — pula

        // Localiza o índice de hardware deste sensor
        int idx = -1;
        for (int i = 0; i < hw_getContagem(); i++) {
            if (hw_getID(i) == s.id_fisico) { idx = i; break; }
        }
        if (idx == -1) continue;  // sensor não encontrado no hardware

        EstadoAlerta& e = estados[idx];
        char kIs[6], kSa[6], kIc[6], kCa[6];
        snprintf(kIs, sizeof(kIs), "is_%d", s.id_sensor);
        snprintf(kSa, sizeof(kSa), "sa_%d", s.id_sensor);
        snprintf(kIc, sizeof(kIc), "ic_%d", s.id_sensor);
        snprintf(kCa, sizeof(kCa), "ca_%d", s.id_sensor);
        e.inicioSuave    = (time_t)prefsAlerta.getLong(kIs, 0);
        e.suaveAvisado   =         prefsAlerta.getBool(kSa, false);
        e.inicioCritico  = (time_t)prefsAlerta.getLong(kIc, 0);
        e.criticoAvisado =         prefsAlerta.getBool(kCa, false);

        if (e.inicioSuave > 0 || e.inicioCritico > 0) {
            LOG("Estado restaurado: " + s.nome_amigavel +
                " | suave=" + String((long)e.inicioSuave) +
                " | critico=" + String((long)e.inicioCritico));
        }
    }
    prefsAlerta.end();
}

// ── FUNÇÕES AUXILIARES DE RESET ───────────────────────────────────────────────
// Centraliza o reset para não repetir as mesmas linhas em vários lugares
// EstadoAlerta& e = referência — altera o estado original, não uma cópia

static void resetarSuave(EstadoAlerta& e) {
    e.inicioSuave      = 0;
    e.suaveAvisado     = false;
    e.ultimoAvisoSuave = 0;
}

static void resetarCritico(EstadoAlerta& e) {
    e.inicioCritico      = 0;
    e.criticoAvisado     = false;
    e.ultimoAvisoCritico = 0;
}


// ── FUNÇÃO AUXILIAR: VERIFICA AGENDA ─────────────────────────────────────────
// Retorna true = pode monitorar / false = fora da agenda, ignorar
bool estaNoHorarioDeMonitoramento(const SensorConfig& s) {

    struct tm timeinfo;
    // Se o relógio não estiver sincronizado, monitora por segurança
    if (!getLocalTime(&timeinfo)) return true;

    // Descobre qual é o dia de hoje (0=Dom, 1=Seg, ... 6=Sab)
    uint8_t diaAtual = timeinfo.tm_wday;
    uint8_t modo     = s.dia_modo[diaAtual];

    if (modo == DIA_DESLIGADO) return false;
    if (modo == DIA_24H)       return true;

    // Modo horário — converte tudo para minutos desde 00:00 para facilitar comparação
    // Ex.: 08:30 = (8 * 60) + 30 = 510 minutos do dia
    int minAtual  = (timeinfo.tm_hour             * 60) + timeinfo.tm_min;
    int minInicio = (s.dia_hora_inicio[diaAtual]  * 60) + s.dia_min_inicio[diaAtual];
    int minFim    = (s.dia_hora_fim[diaAtual]     * 60) + s.dia_min_fim[diaAtual];

    // Janela normal (ex: 08:00 → 20:00) — não vira meia-noite
    if (minInicio <= minFim)
        return (minAtual >= minInicio && minAtual < minFim);

    // Janela que vira meia-noite (ex: 22:00 → 06:00)
    // Monitora das 22h até as 23:59 OU das 00:00 até as 06:00
    return (minAtual >= minInicio || minAtual < minFim);
}


// ── FUNÇÃO PARA O BOTÃO MUDO ──────────────────────────────────────────────────
void alert_setModoManutencao(const String& id_fisico, uint8_t horas) {
    SensorConfig s;
    if (registry_buscarPorID(id_fisico, s)) {
        // Unix timestamp de quando o silêncio termina
        time_t agora;
        time(&agora);
        s.mudo_ate = agora + ((time_t)horas * 3600);
        registry_salvar(s);
        LOG("Alerta: " + s.nome_amigavel + " silenciado por " + String(horas) + "h");
    }
}


// ── FUNÇÃO PRINCIPAL DE LÓGICA ────────────────────────────────────────────────
//
// Fluxo para cada sensor:
//  ├─ Monitoramento desligado? → pula
//  ├─ Em manutenção (mudo)? → pula (ou libera se o tempo acabou)
//  ├─ Fora da agenda? → zera estados e pula
//  ├─ Sensor desconectado? → pula
//  ├─ Temp >= crítica? → timer crítico (paciência curta) → avisa + repete
//  ├─ Temp > alerta (mas < crítica)? → timer suave (tempo de degelo) → avisa + repete
//  └─ Temp normal? → zera tudo, avisa normalização se necessário
//
void processarLogicaAlertas() {
    auto cadastrados = registry_getTodos();

    for (const auto& s : cadastrados) {

        // ── 1. Monitoramento desligado ────────────────────────────────────
        if (!s.monitoramento_ativo) continue;

        // ── 2. Modo manutenção (mudo) ─────────────────────────────────────
        // mudo_ate > 0 significa que está silenciado até aquele timestamp
        if (s.mudo_ate > 0) {
            time_t agora;
            time(&agora);
            if (agora < s.mudo_ate) continue;  // ainda em manutenção
            else {
                // Tempo de manutenção acabou — libera o sensor
                SensorConfig s_upd = s;
                s_upd.mudo_ate = 0;
                registry_salvar(s_upd);
            }
        }

        // ── 3. Fora da agenda ─────────────────────────────────────────────
        if (!estaNoHorarioDeMonitoramento(s)) {

            // Por que zerar os timers aqui?
            // Os timers ficam em estados[idx] — indexados pela posição do sensor
            // no barramento físico (hardware), não pelo cadastro (registry).
            // Se não zerarmos, o timer continua "congelado" no tempo em que saiu
            // da agenda. Quando o monitoramento voltar, o sistema acharia que o
            // sensor já está em alerta há horas — e mandaria aviso errado.
            // Zerando aqui, ele começa do zero quando a agenda reativar.
            //
            // Precisamos do idx aqui para acessar estados[idx].
            // O passo 5 também busca o idx, mas ele nunca chega a rodar para
            // este sensor neste ciclo — o continue abaixo pula tudo.
            // Por isso buscamos o idx aqui de forma local, só para o reset.
            int idx = -1;
            for (int i = 0; i < hw_getContagem(); i++) {
                if (hw_getID(i) == s.id_fisico) { idx = i; break; }
            }
            if (idx != -1) {
                                
                //So salva na NVS se havia algo para zerar — evita escrita desnecessária
                EstadoAlerta& e = estados[idx];
                bool timerAtivo = (e.inicioSuave > 0 || e.inicioCritico > 0);

                if (timerAtivo) {
                    LOG("[Agenda OFF] " + s.nome_amigavel +
                        " | suave=" + String((long)e.inicioSuave) +
                        " | critico=" + String((long)e.inicioCritico));
                }

                // Reseta timers na RAM — isso é o mais importante para evitar avisos errados quando a agenda voltar
                resetarSuave(estados[idx]);
                resetarCritico(estados[idx]);
                    
                if (timerAtivo) {
                    salvarEstadoNVS(s.id_sensor, e);
                    LOG("[Agenda OFF] Timers zerados na RAM e NVS: " + s.nome_amigavel);
                }
            }

            continue;  // pula para o próximo sensor — fora do horário de monitoramento
        }

        // ── 4. Lê temperatura ─────────────────────────────────────────────
        float temp = hw_getTemp(s.id_fisico);

        if (temp == DEVICE_DISCONNECTED_C) {
            // Busca o índice de hardware do sensor (posição no mapa do barramento físico)
            int idx = -1;
            for (int i = 0; i < hw_getContagem(); i++) {
                if (hw_getID(i) == s.id_fisico) { idx = i; break; }
            }

            if (idx != -1) {
                // ── Caso normal: sensor estava no barramento no boot, agora está falhando ──
                // A cada ciclo de erro incrementa o contador.
                // Depois de 5 ciclos consecutivos (= 10 min), avisa uma única vez.
                EstadoAlerta& e = estados[idx];
                e.ciclosErro++;
                if (e.ciclosErro >= 5 && !e.erroAvisado) {
                    enviarMensagemTelegram("🔌❌ SENSOR OFFLINE\n"
                                          "Placa: " + getNomePlaca() + "\n"
                                          "Sensor: " + s.nome_amigavel + "\n"
                                          "Verifique a conexão do cabo.");
                    e.erroAvisado = true;
                }
            } else if (s.id_sensor < MAX_SENSORES) {
                // ── Caso especial: sensor cadastrado mas NÃO estava no barramento no boot ──
                // (ex: cabo queimado antes do reboot — nunca entrou no mapa de hardware)
                // Não pode usar estados[hw_idx] pois não existe hw_idx para este sensor.
                // Usa semHw[id_sensor], array separado, para evitar conflito com outros sensores
                // que ocupam aquela posição no mapa de hardware.
                EstadoSemHardware& e = semHw[s.id_sensor];
                e.ciclosErro++;
                if (e.ciclosErro >= 5 && !e.erroAvisado) {
                    enviarMensagemTelegram("🔌❌ SENSOR OFFLINE\n"
                                          "Placa: " + getNomePlaca() + "\n"
                                          "Sensor: " + s.nome_amigavel + "\n"
                                          "Verifique a conexão do cabo.");
                    e.erroAvisado = true;
                }
            }

            continue;  // sensor offline — não processa alertas de temperatura
        }

        // Sensor voltou a responder — avisa recuperação se estava com erro
        {
            int idx = -1;
            for (int i = 0; i < hw_getContagem(); i++) {
                if (hw_getID(i) == s.id_fisico) { idx = i; break; }
            }
            if (idx != -1 && estados[idx].erroAvisado) {
                enviarMensagemTelegram("🔌✅ SENSOR RECONECTADO\n"
                                      "Placa: " + getNomePlaca() + "\n"
                                      "Sensor: " + s.nome_amigavel + "\n"
                                      "Voltou a responder normalmente.");
                estados[idx].ciclosErro  = 0;
                estados[idx].erroAvisado = false;
            } else if (idx != -1) {
                estados[idx].ciclosErro = 0;  // reseta contador mesmo sem ter avisado
            }
        }

        // ── 5. Busca o índice do sensor na lista de hardware ──────────────
        // Precisamos do índice para acessar estados[idx]
        int idx = -1;
        for (int i = 0; i < hw_getContagem(); i++) {
            if (hw_getID(i) == s.id_fisico) { idx = i; break; }
        }
        if (idx == -1) continue;

        // EstadoAlerta& e = referência direta ao estado deste sensor na lista
        // Com &, qualquer alteração em "e" altera diretamente estados[idx] — sem cópia
        EstadoAlerta& e = estados[idx];


        // ══════════════════════════════════════════════════════════════════
        // ── 6. LIMIAR CRÍTICO (temp >= temp_critica) ──────────────────────
        // Timer independente com paciência curta — crítico é urgente
        // Mas ainda dá um tempo para picos passageiros (porta aberta, degelo intenso)
        // ══════════════════════════════════════════════════════════════════
        if (temp >= s.temp_critica) {

            // Primeira vez acima do crítico — anota o horário
            if (e.inicioCritico == 0) {
                time(&e.inicioCritico);
                salvarEstadoNVS(s.id_sensor, e);
                LOG("Timer crítico iniciado: " + s.nome_amigavel + " " + String(temp, 1) + "°C");
            }

            // Converte tempo_espera_critico_min (minutos) para segundos
            unsigned long esperaCriticoS = (unsigned long)s.tempo_espera_critico_min * 60UL;

            // Ainda dentro do tempo de paciência? Aguarda silenciosamente
            // (se cair antes de expirar, o timer será zerado na seção 7)
            time_t agora;
            time(&agora);
            if ((agora - e.inicioCritico) < (time_t)esperaCriticoS) continue;

            // Tempo expirou — é um crítico real, não um pico passageiro
            if (!e.criticoAvisado) {
                // Primeiro aviso crítico
                enviarMensagemTelegram("🚨 CRÍTICO: " + s.nome_amigavel +
                                       " está em " + String(temp, 1) + "°C!");
                e.criticoAvisado     = true;
                e.ultimoAvisoCritico = millis();
                salvarEstadoNVS(s.id_sensor, e);
            }
            else if ((millis() - e.ultimoAvisoCritico) >= REPETICAO_CRITICO_S * 1000UL) {
                // Repetição — problema não foi resolvido, insiste no aviso
                enviarMensagemTelegram("🚨 AINDA CRÍTICO: " + s.nome_amigavel +
                                       " continua em " + String(temp, 1) + "°C!");
                e.ultimoAvisoCritico = millis();
            }

            continue;  // crítico ativo — não processa suave neste ciclo (prioridade)
        }


        // ══════════════════════════════════════════════════════════════════
        // ── 7. SAIU DA ZONA CRÍTICA ───────────────────────────────────────
        // Se chegou aqui: temp < temp_critica
        // Se o timer crítico estava rodando, reseta (era passageiro ou foi resolvido)
        // ══════════════════════════════════════════════════════════════════
        if (e.inicioCritico > 0) {
            if (e.criticoAvisado) {
                // Informa a saída do crítico — inclui contexto se ainda estiver em alerta suave
                String msg = "✅ Saiu do crítico: " + s.nome_amigavel +
                             " agora em " + String(temp, 1) + "°C";
                if (temp > s.temp_max_alerta) {
                    msg += " (ainda em alerta suave)";
                }
                enviarMensagemTelegram(msg);
            }
            resetarCritico(e);
            salvarEstadoNVS(s.id_sensor, e);
            // Nota: o timer suave continua rodando normalmente
            // Se estava acima do suave antes de entrar no crítico, o tempo já está contando
        }


        // ══════════════════════════════════════════════════════════════════
        // ── 8. LIMIAR SUAVE (temp > temp_max_alerta) ──────────────────────
        // Timer independente com paciência longa — cobre ciclos de degelo normais
        // Só avisa se a temperatura não voltar ao normal dentro do tempo de degelo
        // ══════════════════════════════════════════════════════════════════
        if (temp > s.temp_max_alerta) {

            // Primeira vez acima do limiar suave — anota o horário
            if (e.inicioSuave == 0) {
                time(&e.inicioSuave);
                salvarEstadoNVS(s.id_sensor, e);
                LOG("Timer suave iniciado: " + s.nome_amigavel + " " + String(temp, 1) + "°C");
            }

            // Converte tempo_degelo_min (minutos) para segundos
            unsigned long degelo_s = (unsigned long)s.tempo_degelo_min * 60UL;

            // Ainda dentro do tempo de degelo? Aguarda silenciosamente
            // Degelo normal resolve dentro desse tempo — nenhum aviso necessário
            time_t agora;
            time(&agora);
            if ((agora - e.inicioSuave) < (time_t)degelo_s) continue;

            // Tempo de degelo expirou — temperatura não voltou, é um problema real
            if (!e.suaveAvisado) {
                // Primeiro aviso suave
                enviarMensagemTelegram("⚠️ ALERTA: " + s.nome_amigavel +
                                       " está em " + String(temp, 1) + "°C há " +
                                       String(s.tempo_degelo_min) + " min!");
                e.suaveAvisado     = true;
                e.ultimoAvisoSuave = millis();
                salvarEstadoNVS(s.id_sensor, e);
            }
            else if ((millis() - e.ultimoAvisoSuave) >= REPETICAO_SUAVE_S * 1000UL) {
                // Repetição — problema persiste, insiste no aviso
                enviarMensagemTelegram("⚠️ AINDA EM ALERTA: " + s.nome_amigavel +
                                       " continua em " + String(temp, 1) + "°C!");
                e.ultimoAvisoSuave = millis();
            }

            continue;
        }


        // ══════════════════════════════════════════════════════════════════
        // ── 9. TEMPERATURA NORMAL ─────────────────────────────────────────
        // Se chegou aqui: temp <= temp_max_alerta — câmara está OK
        // ══════════════════════════════════════════════════════════════════

        // Se tinha avisado antes, informa a normalização
        if (e.suaveAvisado) {
            enviarMensagemTelegram("✅ Normalizado: " + s.nome_amigavel +
                                   " voltou a " + String(temp, 1) + "°C");
        }

        // Zera tudo — pronto para o próximo ciclo de monitoramento
        resetarSuave(e);
        resetarCritico(e);  // reseta também o crítico, caso tenha sido passageiro sem aviso
        salvarEstadoNVS(s.id_sensor, e);
    }
}
