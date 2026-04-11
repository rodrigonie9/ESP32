# include <WebServer.h>
# include <WiFi.h>
#include <time.h>

#include "web_portal.h"
#include "device/device_config.h"  // informações da placa esp32
#include "sensors/sensors.h"
#include "sensors/sensor_registry.h"
#include "logic/alert_manager.h"    // para botão de manutenção
#include "debug.h"
#include "../config/system_limits.h"

WebServer server(80);

// CSS salvo na flash (PROGMEM) para economizar RAM
// Layout moderno com gradiente no cabeçalho, cards com sombra suave,
// tabela clean e ID compacto que expande ao passar o mouse
const char WEB_STYLE[] PROGMEM = R"rawliteral(
<style>
body{font-family:'Segoe UI',Arial,sans-serif;margin:0;padding:16px;background:#eef2f7;color:#2d3748;}

/* ── Cabeçalho com gradiente azul → verde ── */
.topo{background:linear-gradient(135deg,#1a56db 0%,#0e9f6e 100%);color:#fff;padding:18px 24px;border-radius:12px;margin-bottom:18px;box-shadow:0 4px 16px rgba(26,86,219,0.25);}
.topo h1{margin:0 0 4px 0;font-size:1.4em;}
.topo small{opacity:0.85;font-size:0.85em;}

/* ── Cards brancos com sombra suave ── */
.card{background:#fff;padding:20px 22px;border-radius:12px;box-shadow:0 1px 3px rgba(0,0,0,0.07),0 4px 12px rgba(0,0,0,0.05);margin-bottom:18px;}
h2{color:#1a56db;font-size:0.78em;text-transform:uppercase;letter-spacing:0.08em;margin:0 0 14px 0;padding-bottom:8px;border-bottom:2px solid #e5e7eb;}

/* ── Tabela clean sem bordas grossas ── */
table{width:100%;border-collapse:collapse;}
th{background:#f7f9fc;color:#6b7280;font-size:0.72em;text-transform:uppercase;letter-spacing:0.06em;padding:10px 12px;text-align:left;border-bottom:2px solid #e5e7eb;}
td{padding:11px 12px;border-bottom:1px solid #f3f4f6;vertical-align:middle;font-size:0.9em;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:#f9fafb;}

/* ── Cores de temperatura por status ── */
.tok{color:#059669;font-weight:700;}    /* verde  = normal      */
.twarn{color:#d97706;font-weight:700;}  /* laranja = alerta     */
.tcrit{color:#dc2626;font-weight:700;}  /* vermelho = crítico   */

/* ── Badge de manutenção ── */
.badge{display:inline-block;padding:2px 8px;border-radius:999px;font-size:0.75em;font-weight:600;}
.bmaint{background:#fef3c7;color:#92400e;}

/* ── Botões ── */
.btn{padding:5px 11px;border:none;border-radius:6px;cursor:pointer;text-decoration:none;display:inline-block;margin:2px;font-size:0.8em;font-weight:500;}
.btn:hover{opacity:0.85;}
.btn-primary{background:#1a56db;color:#fff;}
.btn-danger{background:#ef4444;color:#fff;}
.btn-warning{background:#f59e0b;color:#fff;}

/* ── ID compacto: pequeno por padrão, expande ao passar o mouse ──
   max-width pequeno faz o texto ser cortado com "..."
   ao passar o mouse, max-width aumenta suavemente (transition) */
.id-chip{display:inline-block;max-width:52px;overflow:hidden;white-space:nowrap;text-overflow:ellipsis;font-family:monospace;font-size:0.9em;background:#f3f4f6;padding:2px 6px;border-radius:4px;cursor:default;transition:max-width 0.35s ease;vertical-align:middle;}
.id-chip:hover{max-width:240px;}

/* ── Linha de sensor desconectado ── */
.sensor-offline td{background:#fee2e2;}
.sensor-offline:hover td{background:#fecaca;}

/* ── Formulários ── */
input[type=text],input[type=number],select{padding:7px 10px;border:1px solid #d1d5db;border-radius:6px;font-size:0.9em;}
.campo{margin:14px 0;}
.horario-box{background:#f0f4ff;padding:12px;border-radius:6px;margin-top:8px;border:1px solid #c8d8f8;}
</style>
)rawliteral";

// JavaScript salvo na flash (PROGMEM)
// Controla a interface da agenda no portal
const char WEB_SCRIPT[] PROGMEM = R"rawliteral(
<script>
// Mostra ou esconde os selects de horário de um dia específico
// Chamado quando o usuário clica num radio (Off / 24h / Hor)
// i = índice do dia: 0=Dom, 1=Seg, 2=Ter, 3=Qua, 4=Qui, 5=Sex, 6=Sab
function toggleDia(i) {
    // Descobre qual radio está marcado para este dia
    var sel = document.querySelector('input[name="dia_modo_' + i + '"]:checked');
    var janela = document.getElementById('janela_' + i);
    if (sel && janela) {
        // Só exibe os campos de horário se o modo escolhido for 2 (DIA_HORARIO)
        janela.style.display = (sel.value == '2') ? 'inline' : 'none';
    }
}
</script>
)rawliteral";


// ======================== CONSTRUÇÃO DA PÁGINA PRINCIPAL ==============================
// Desenha o site no navegador do usuário
// Usa sendContent para enviar o site em pedaços — protege a RAM da ESP32
void handleRoot() {
    // Avisa o navegador que vamos enviar o site em partes (chunks)
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    // refresh automático a cada 2 minutos | viewport para funcionar bem no celular
    server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                                  "<meta http-equiv='refresh' content='120'>"
                                  "<meta name='viewport' content='width=device-width,initial-scale=1'>");
    server.sendContent(WEB_STYLE);
    server.sendContent("</head><body>");

    // --- CABEÇALHO com gradiente — mostra o nome da placa e hora atual em destaque ---

    // Tenta pegar a hora atual do NTP
    // struct tm é uma "ficha" com campos: hora, minuto, dia, mês, ano
    // getLocalTime() preenche essa ficha — retorna false se NTP ainda não sincronizou
    char horaAtual[24] = "hora nao sincronizada";
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        // strftime formata a ficha tm em texto:
        // %H:%M:%S = 14:32:05  |  %d/%m/%Y = 31/03/2026
        strftime(horaAtual, sizeof(horaAtual), "%H:%M:%S  %d/%m/%Y", &timeinfo);
    }

    // Formata hora do boot (quando a placa iniciou)
    char horaBoot[24] = "desconhecida";
    time_t tBoot = sysinfo_getBootTime();
    if (tBoot != 0) {
        struct tm tmBoot;
        localtime_r(&tBoot, &tmBoot);
        strftime(horaBoot, sizeof(horaBoot), "%H:%M  %d/%m/%Y", &tmBoot);
    }

    // Formata hora do último reboot programado
    char horaReboot[24] = "nunca";
    time_t tReboot = sysinfo_getUltimoRebootProgramado();
    if (tReboot != 0) {
        struct tm tmReboot;
        localtime_r(&tReboot, &tmReboot);
        strftime(horaReboot, sizeof(horaReboot), "%H:%M  %d/%m/%Y", &tmReboot);
    }

    server.sendContent("<div class='topo'><h1>" + getNomePlaca() + "</h1>"
                       "<small>Monitor de Temperatura &bull; ESP32</small>"
                       "<small style='display:block;margin-top:6px;opacity:0.85'>"
                       "Hora atual: " + String(horaAtual) + "</small>"
                       "<small style='display:block;opacity:0.75'>"
                       "Iniciou em: " + String(horaBoot) + " &bull; "
                       "Reboot programado: " + String(horaReboot) + "</small></div>");

    // --- SEÇÃO 1: CONFIGURAÇÃO DO NOME DA PLACA ---
    String cardPlaca = "<div class='card'><h2>Configuração da Placa</h2>";
    cardPlaca += "<form action='/config/placa' method='POST'>";
    cardPlaca += "Nome: <input type='text' name='nome' value='" + getNomePlaca() + "' maxlength='32' required> ";
    cardPlaca += "<input type='submit' value='Salvar' class='btn btn-primary'></form></div>";
    server.sendContent(cardPlaca);

    // --- SEÇÃO 2: TABELA DE SENSORES CADASTRADOS ---
    // Ordem das colunas: Nome | Temperatura | Hora Leitura | Alerta | Ações | ID
    server.sendContent("<div class='card'><h2>Sensores Cadastrados</h2><table>");
    server.sendContent("<tr><th>Nome</th><th>Temperatura</th><th>Hora Leitura</th><th>Alerta</th><th>Ações</th><th>ID</th></tr>");

    auto cadastrados = registry_getTodos();
    for (const auto& s : cadastrados) {
        float temp     = hw_getTemp(s.id_fisico);
        SensorStats st = hw_getStats(s.id_fisico); // contém ultimo_timestamp

        // ── Coluna NOME + badge de manutenção com hora de término ──
        String nomeCell = s.nome_amigavel;
        time_t agora; time(&agora);
        bool emManutencao = (s.mudo_ate > 0 && agora < s.mudo_ate);
        if (emManutencao) {
            struct tm tmMudo;
            localtime_r(&s.mudo_ate, &tmMudo);
            char ate[6];  // "HH:MM\0"
            strftime(ate, sizeof(ate), "%H:%M", &tmMudo);
            nomeCell += " <span class='badge bmaint'>Manutenção até " + String(ate) + "</span>";
        }

        // ── Coluna TEMPERATURA com cor por status ──
        // Verde = normal | Laranja = acima do alerta | Vermelho = crítico ou sensor OFF
        String tempCell;
        if (temp == SENSOR_ERRO) {
            tempCell = "<span class='tcrit'>DESCONECTADO</span>";
        } else if (temp >= s.temp_critica) {
            // Temperatura acima do limite crítico — aviso imediato
            tempCell = "<span class='tcrit'>" + String(temp, 1) + "&deg;C</span>";
        } else if (temp >= s.temp_max_alerta) {
            // Temperatura acima do alerta normal
            tempCell = "<span class='twarn'>" + String(temp, 1) + "&deg;C</span>";
        } else {
            // Temperatura normal
            tempCell = "<span class='tok'>" + String(temp, 1) + "&deg;C</span>";
        }

        // ── Coluna HORA DA ÚLTIMA LEITURA ──
        // st.ultimo_timestamp == 0 → nunca leu (NTP não sincronizou ainda ou placa acabou de ligar)
        // localtime() converte o timestamp (segundos desde 1970) para hora/data legível
        // strftime() formata como "HH:MM:SS"
        String horaCell;
        if (st.ultimo_timestamp == 0) {
            horaCell = "<span style='color:#9ca3af'>&mdash;</span>"; // traço cinza
        } else {
            struct tm* tm_info = localtime(&st.ultimo_timestamp);
            char buf[9]; // "HH:MM:SS\0" = 9 caracteres
            strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
            horaCell = String(buf);
        }

        // ── Envia as células na nova ordem ──
        bool desconectado = (temp == SENSOR_ERRO);
        server.sendContent(desconectado ? "<tr class='sensor-offline'>" : "<tr>");
        server.sendContent("<td>" + nomeCell + "</td>");
        server.sendContent("<td>" + tempCell + "</td>");
        server.sendContent("<td>" + horaCell + "</td>");
        server.sendContent("<td>" + String(s.temp_max_alerta, 1) + "&deg;C</td>");

        // ── Coluna AÇÕES: Editar + Manutenção + Remover ──
        server.sendContent("<td>");
        server.sendContent("<a href='/editar?id=" + s.id_fisico + "' class='btn btn-primary'>Editar</a> ");

        // Botão manutenção — vira "Cancelar" se já estiver em manutenção
        if (emManutencao) {
            server.sendContent("<form action='/cancelar_manutencao' method='POST' style='display:inline'>");
            server.sendContent("<input type='hidden' name='id' value='" + s.id_fisico + "'>");
            server.sendContent("<button type='submit' class='btn btn-warning'>Cancelar Manutenção</button></form> ");
        } else {
            server.sendContent("<form action='/manutencao' method='POST' style='display:inline'>");
            server.sendContent("<input type='hidden' name='id' value='" + s.id_fisico + "'>");
            server.sendContent("<select name='horas' style='padding:3px;font-size:0.8em'>");
            for (int h = 1; h <= 24; h++) {
                server.sendContent("<option value='" + String(h) + "'>" + String(h) + "h</option>");
            }
            server.sendContent("</select> ");
            server.sendContent("<button type='submit' class='btn btn-warning'>Manutenção</button></form> ");
        }
        server.sendContent("<a href='/remover?id=" + s.id_fisico + "' class='btn btn-danger'>Remover</a>");
        server.sendContent("</td>");

        // ── Coluna ID — exibe id_sensor (0-29), tooltip mostra id_fisico completo ──
        server.sendContent("<td><span class='id-chip' title='" + s.id_fisico + "'>"
                           + String(s.id_sensor) + "</span></td>");
        server.sendContent("</tr>");
    }
    server.sendContent("</table></div>");

    // --- SEÇÃO 3: SENSORES DETECTADOS NO FIO MAS AINDA NÃO CADASTRADOS ---
    server.sendContent("<div class='card'><h2>Detectados no Barramento</h2><table>");
    server.sendContent("<tr><th>ID Físico</th><th>Ação</th></tr>");

    uint8_t total = hw_getContagem();
    for (uint8_t i = 0; i < total; i++) {
        String id = hw_getID(i);
        SensorConfig dummy;
        if (!registry_buscarPorID(id, dummy)) { // só mostra se ainda não cadastrado
            server.sendContent("<tr><td><code>" + id + "</code></td><td>");
            server.sendContent("<form action='/config/sensor' method='POST' style='display:inline'>");
            server.sendContent("<input type='hidden' name='id' value='" + id + "'>");
            server.sendContent("<input type='text' name='nome' placeholder='Nome' maxlength='32' required> ");
            server.sendContent("<input type='number' step='0.1' name='max' value='8.0' min='-50' max='100' style='width:70px' required> &deg;C ");
            server.sendContent("<input type='submit' value='Cadastrar' class='btn btn-primary'></form>");
            server.sendContent("</td></tr>");
        }
    }
    server.sendContent("</table></div></body></html>");
    server.sendContent(""); // sinaliza fim do envio em chunks
}


// ======================== PÁGINA DE EDIÇÃO ============================================
// Abre uma página completa para configurar todos os campos de um sensor já cadastrado
void handleEditarSensor() {
    if (!server.hasArg("id")) {
        server.sendHeader("Location", "/");
        server.send(303);
        return;
    }

    SensorConfig s;
    if (!registry_buscarPorID(server.arg("id"), s)) {
        server.sendHeader("Location", "/");
        server.send(303);
        return;
    }

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                                  "<meta name='viewport' content='width=device-width,initial-scale=1'>");
    server.sendContent(WEB_STYLE);
    server.sendContent(WEB_SCRIPT);
    server.sendContent("</head><body><h1>Editar Sensor</h1><div class='card'>");
    server.sendContent("<form action='/salvar/sensor' method='POST'>");
    server.sendContent("<input type='hidden' name='id' value='" + s.id_fisico + "'>");

    // ── Nome
    server.sendContent("<div class='campo'>Nome: <input type='text' name='nome' value='" + s.nome_amigavel + "' maxlength='32' required></div>");

    // ── Temperaturas
    // ── Alerta suave: temperatura + tempo de degelo ──
    // O "tempo de degelo" é a paciência do sistema antes de avisar
    // Se a temperatura voltar ao normal dentro desse tempo, nenhum aviso é enviado
    server.sendContent("<div class='campo'>Temp. alerta: <input type='number' name='max' value='" + String(s.temp_max_alerta, 1) + "' step='0.1' min='-50' max='100' style='width:80px'> °C"
                       "&nbsp;&nbsp; Tempo de degelo: <input type='number' name='degelo' value='" + String(s.tempo_degelo_min) + "' min='0' max='999' style='width:60px'> min"
                       " <small style='color:#888'>Aguarda este tempo acima do limite antes de avisar. Cobre ciclos de degelo normais.</small></div>");

    // ── Alerta crítico: temperatura + tempo de espera ──
    // Tempo menor que o suave — crítico é mais urgente, mas ainda filtra picos passageiros
    server.sendContent("<div class='campo'>Temp. crítica: <input type='number' name='critica' value='" + String(s.temp_critica, 1) + "' step='0.1' min='-50' max='100' style='width:80px'> °C"
                       "&nbsp;&nbsp; Tempo de espera: <input type='number' name='espera_critico' value='" + String(s.tempo_espera_critico_min) + "' min='0' max='999' style='width:60px'> min"
                       " <small style='color:#888'>Temperatura grave. Avisa após este tempo. Repete enquanto persistir.</small></div>");


    // ── Agenda por dia ──
    // Tabela com 7 linhas — uma por dia da semana
    // Cada linha: nome do dia | radio(Off/24h/Hor) | campos de horário (só aparecem no modo Hor)
    const char* nomesDias[] = {"Dom","Seg","Ter","Qua","Qui","Sex","Sab"};

    server.sendContent("<div class='campo'><b>Agenda por dia:</b>");
    server.sendContent("<table style='width:auto;margin-top:8px'>");
    server.sendContent("<tr><th>Dia</th><th>Off</th><th>24h</th><th>Hor</th><th colspan='2'>Janela de horário</th></tr>");

    for (int i = 0; i < 7; i++) {
        uint8_t modo = s.dia_modo[i]; // modo salvo para este dia

        // Converte hora+minuto para minutos totais (para pré-selecionar os selects)
        // Ex: 08:30 → 8*60+30 = 510
        int minIni = s.dia_hora_inicio[i] * 60 + s.dia_min_inicio[i];
        int minFim = s.dia_hora_fim[i]    * 60 + s.dia_min_fim[i];

        // A janela de horário só é exibida se o modo atual for DIA_HORARIO (2)
        String visJanela = (modo == DIA_HORARIO) ? "inline" : "none";

        server.sendContent("<tr>");
        server.sendContent("<td><b>" + String(nomesDias[i]) + "</b></td>");

        // Radio Off (valor 0 = DIA_DESLIGADO)
        server.sendContent("<td style='text-align:center'><input type='radio' name='dia_modo_" + String(i) +
                           "' value='0'" + String(modo == DIA_DESLIGADO ? " checked" : "") +
                           " onclick='toggleDia(" + String(i) + ")'></td>");

        // Radio 24h (valor 1 = DIA_24H)
        server.sendContent("<td style='text-align:center'><input type='radio' name='dia_modo_" + String(i) +
                           "' value='1'" + String(modo == DIA_24H ? " checked" : "") +
                           " onclick='toggleDia(" + String(i) + ")'></td>");

        // Radio Hor (valor 2 = DIA_HORARIO)
        server.sendContent("<td style='text-align:center'><input type='radio' name='dia_modo_" + String(i) +
                           "' value='2'" + String(modo == DIA_HORARIO ? " checked" : "") +
                           " onclick='toggleDia(" + String(i) + ")'></td>");

        // Célula com os selects de horário — span controlado pelo JavaScript toggleDia()
        server.sendContent("<td colspan='2'><span id='janela_" + String(i) +
                           "' style='display:" + visJanela + "'>");

        // Select de início — envia minutos desde 00:00 (ex: 510 = 08:30)
        server.sendContent("Das <select name='dia_ini_" + String(i) + "'>");
        for (int h = 0; h < 24; h++) {
            for (int m = 0; m <= 30; m += 30) {
                int val = h * 60 + m;
                char buf[6];
                snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
                server.sendContent("<option value='" + String(val) + "'" +
                                   (val == minIni ? " selected" : "") + ">" + buf + "</option>");
            }
        }
        server.sendContent("</select>");

        // Select de fim
        server.sendContent(" &agrave;s <select name='dia_fim_" + String(i) + "'>");
        for (int h = 0; h < 24; h++) {
            for (int m = 0; m <= 30; m += 30) {
                int val = h * 60 + m;
                char buf[6];
                snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
                server.sendContent("<option value='" + String(val) + "'" +
                                   (val == minFim ? " selected" : "") + ">" + buf + "</option>");
            }
        }
        server.sendContent("</select></span></td>");
        server.sendContent("</tr>");
    }

    server.sendContent("</table>");
    server.sendContent("<small style='color:#888'>Hor = horário específico. Se fim &lt; início, atravessa a meia-noite.</small>");
    server.sendContent("</div>"); // fecha campo

    // Botões salvar / cancelar
    server.sendContent("<div class='campo'><input type='submit' value='Salvar' class='btn btn-primary'> ");
    server.sendContent("<a href='/' class='btn'>Cancelar</a></div>");
    server.sendContent("</form></div></body></html>");
    server.sendContent("");
}


// ======================== SALVAR EDIÇÃO ===============================================
// Recebe os dados do formulário de edição e salva no NVS
void handleSalvarEdicao() {
    if (!server.hasArg("id")) {
        server.sendHeader("Location", "/");
        server.send(303);
        return;
    }

    SensorConfig s;
    // Carrega configuração existente — preserva campos não editados (mudo_ate, etc.)
    if (!registry_buscarPorID(server.arg("id"), s)) {
        server.sendHeader("Location", "/");
        server.send(303);
        return;
    }

    // ── Nome
    if (server.hasArg("nome")) {
        String nome = server.arg("nome");
        if (nome.length() > 0 && nome.length() <= 32) s.nome_amigavel = nome;
    }

    // ── Temperaturas
    // Valida primeiro caractere antes de converter (toFloat() retorna 0.0 para strings inválidas)
    String maxStr = server.arg("max");
    if (maxStr.length() > 0 && (isdigit(maxStr[0]) || maxStr[0] == '-' || maxStr[0] == '.'))
        s.temp_max_alerta = maxStr.toFloat();

    String criticaStr = server.arg("critica");
    if (criticaStr.length() > 0 && (isdigit(criticaStr[0]) || criticaStr[0] == '-' || criticaStr[0] == '.'))
        s.temp_critica = criticaStr.toFloat();

    // ── Tempos de alerta e mudo padrão
    if (server.hasArg("degelo"))        s.tempo_degelo_min         = (uint16_t)server.arg("degelo").toInt();
    if (server.hasArg("espera_critico")) s.tempo_espera_critico_min = (uint16_t)server.arg("espera_critico").toInt();

    // ── Agenda por dia ──
    // Para cada dia lê o radio (modo) e os selects de horário (se modo = DIA_HORARIO)
    for (int i = 0; i < 7; i++) {
        String modoKey = "dia_modo_" + String(i); // ex: "dia_modo_3" = Quarta
        String iniKey  = "dia_ini_"  + String(i); // ex: "dia_ini_3"
        String fimKey  = "dia_fim_"  + String(i); // ex: "dia_fim_3"

        if (server.hasArg(modoKey)) {
            s.dia_modo[i] = (uint8_t)server.arg(modoKey).toInt();
        }

        // Só salva horário se o modo deste dia for DIA_HORARIO (2)
        // Os selects enviam minutos desde 00:00 — convertemos de volta para hora+minuto
        // Ex: 510 minutos → hora = 510/60 = 8, minuto = 510%60 = 30 → 08:30
        if (s.dia_modo[i] == DIA_HORARIO) {
            if (server.hasArg(iniKey)) {
                int minIni = server.arg(iniKey).toInt();
                s.dia_hora_inicio[i] = minIni / 60; // divisão inteira: 510/60 = 8
                s.dia_min_inicio[i]  = minIni % 60; // resto:           510%60 = 30
            }
            if (server.hasArg(fimKey)) {
                int minFim = server.arg(fimKey).toInt();
                s.dia_hora_fim[i] = minFim / 60;
                s.dia_min_fim[i]  = minFim % 60;
            }
        }
    }

    // ── Verifica se outro sensor já usa este nome (permite manter o mesmo nome ao editar) ──
    // Compara contra todos os sensores, mas ignora o próprio sensor (mesmo id_fisico)
    String idAtual = server.arg("id");
    auto todos = registry_getTodos();
    for (const auto& existente : todos) {
        if (existente.id_fisico != idAtual &&
            existente.nome_amigavel.equalsIgnoreCase(s.nome_amigavel)) {
            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200, "text/html",
                "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>");
            server.sendContent(WEB_STYLE);
            server.sendContent("</head><body><div class='card'>"
                "<p style='color:#dc2626;font-weight:bold'>&#10060; Nome j&aacute; em uso</p>"
                "<p>J&aacute; existe um sensor chamado <b>" + s.nome_amigavel + "</b>. Escolha outro nome.</p>"
                "<a href='/editar?id=" + idAtual + "' class='btn btn-primary'>Voltar</a>"
                "</div></body></html>");
            server.sendContent("");
            return;
        }
    }

    registry_salvar(s);
    server.sendHeader("Location", "/");
    server.send(303);
}


// ======================== AÇÕES E INICIALIZAÇÃO ===============================================

// Salva o nome da placa
void handleConfigPlaca() {
    if (server.hasArg("nome")) {
        String nome = server.arg("nome");
        // rejeita nomes maiores de 32 caracteres - já bloqueado no HTML
        if (nome.length() > 0 && nome.length() <= 32) {
            setNomePlaca(nome);
        }
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

// Cadastra um sensor novo detectado no barramento
void handleConfigSensor() {
    if (server.hasArg("id") && server.hasArg("nome")) {
        String nome = server.arg("nome");
        // rejeita nome vazio ou acima de 32 caracteres
        if (nome.length() == 0 || nome.length() > 32) {
            server.sendHeader("Location", "/");
            server.send(303);
            return;
        }

        String maxStr = server.arg("max");
        // Verifica se o campo tem conteúdo numérico, sinal ou ponto
        // toFloat() retorna 0.0 tanto para "0" legítimo quanto para "abc" inválido
        // checar o primeiro caractere é a única forma segura de distinguir os dois
        bool tempValida = maxStr.length() > 0 &&
                          (isdigit(maxStr[0]) || maxStr[0] == '-' || maxStr[0] == '.');

        // ── Verifica se o nome já está em uso por outro sensor ──
        // Comparação sem diferenciar maiúsculas/minúsculas (equalsIgnoreCase)
        // Ex: "Câmara 1" e "câmara 1" são considerados iguais
        auto todos = registry_getTodos();
        for (const auto& existente : todos) {
            if (existente.nome_amigavel.equalsIgnoreCase(nome)) {
                server.setContentLength(CONTENT_LENGTH_UNKNOWN);
                server.send(200, "text/html",
                    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                    "<meta name='viewport' content='width=device-width,initial-scale=1'>");
                server.sendContent(WEB_STYLE);
                server.sendContent("</head><body><div class='card'>"
                    "<p style='color:#dc2626;font-weight:bold'>&#10060; Nome j&aacute; em uso</p>"
                    "<p>J&aacute; existe um sensor chamado <b>" + nome + "</b>. Escolha outro nome.</p>"
                    "<a href='/' class='btn btn-primary'>Voltar</a>"
                    "</div></body></html>");
                server.sendContent("");
                return;
            }
        }

        SensorConfig s;
        s.id_fisico           = server.arg("id");
        s.nome_amigavel       = nome;
        s.temp_max_alerta     = tempValida ? maxStr.toFloat() : TEMP_ALERTA_PADRAO;
        s.monitoramento_ativo = true;

        // ── Valores padrão para o novo sensor
        // Podem ser ajustados depois na página de edição (botão Editar)
        s.temp_critica              = s.temp_max_alerta + 5.0; // 5°C acima do alerta
        s.tempo_degelo_min          = 40;  // 40 min — cobre ciclos de degelo normais (~30 min + margem)
        s.tempo_espera_critico_min  = 15;  // 15 min — crítico é urgente, paciência menor
        // ── Agenda por dia: padrão = todos os dias em 24h ──
        // O usuário pode ajustar depois na página de edição
        for (int i = 0; i < 7; i++) {
            s.dia_modo[i]        = DIA_24H; // começa monitorando 24h
            s.dia_hora_inicio[i] = 0;       // zerado — não usado no modo 24h
            s.dia_min_inicio[i]  = 0;
            s.dia_hora_fim[i]    = 0;       // zerado — não usado no modo 24h
            s.dia_min_fim[i]     = 0;
        }
        s.mudo_ate             = 0; // não está em manutenção

        registry_salvar(s);
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

// Remove um sensor pelo ID
void handleRemoverSensor() {
    if (server.hasArg("id")) registry_remover(server.arg("id"));
    server.sendHeader("Location", "/");
    server.send(303);
}

// Cancela o modo manutenção — zera mudo_ate e salva na NVS
void handleCancelarManutencao() {
    if (server.hasArg("id")) {
        SensorConfig s;
        if (registry_buscarPorID(server.arg("id"), s)) {
            s.mudo_ate = 0;
            registry_salvar(s);
        }
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

// Botão de manutenção — silencia alertas por X horas sem alterar a agenda
void handleManutencao() {
    if (server.hasArg("id") && server.hasArg("horas")) {
        uint8_t horas = (uint8_t)server.arg("horas").toInt();
        if (horas >= 1 && horas <= 24) {
            alert_setModoManutencao(server.arg("id"), horas);
        }
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

// Registra todas as rotas e inicia o servidor web
void iniciarWebPortal() {
    server.on("/", handleRoot);
    server.on("/config/placa",  HTTP_POST, handleConfigPlaca);
    server.on("/config/sensor", HTTP_POST, handleConfigSensor);
    server.on("/editar",        HTTP_GET,  handleEditarSensor);  // página de edição
    server.on("/salvar/sensor", HTTP_POST, handleSalvarEdicao);  // salva edição
    server.on("/manutencao",           HTTP_POST, handleManutencao);          // ativa manutenção
    server.on("/cancelar_manutencao",  HTTP_POST, handleCancelarManutencao);  // cancela manutenção
    server.on("/remover",                  handleRemoverSensor);
    server.begin();
    LOG("Servidor Web iniciado.");
}

void webPortalLoop() {
    server.handleClient();
}
