# include <WebServer.h>
# include <WiFi.h>

#include "web_portal.h"
#include "device/device_config.h"  // informações da placa esp32
#include "sensors/sensors.h"
#include "sensors/sensor_registry.h"
#include "logic/alert_manager.h"    // para botão de manutenção
#include "debug.h"
#include "../config/system_limits.h"

WebServer server(80);

// CSS salvo na flash (PROGMEM) para economizar RAM
const char WEB_STYLE[] PROGMEM = R"rawliteral(
<style>
    body{font-family:sans-serif;margin:20px;background:#f4f4f9;color:#333;}
    .card{background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin-bottom:20px;}
    h1,h2{color:#444;}
    table{width:100%;border-collapse:collapse;margin-top:10px;}
    th,td{border:1px solid #ddd;padding:12px;text-align:left;}
    th{background:#f8f8f8;}
    .btn{padding:8px 15px;border:none;border-radius:4px;cursor:pointer;text-decoration:none;display:inline-block;margin:2px;}
    .btn-primary{background:#007bff;color:#fff;}
    .btn-danger{background:#dc3545;color:#fff;}
    .btn-warning{background:#ffc107;color:#333;}
    .btn-sm{padding:4px 10px;font-size:0.85em;}
    input[type=text],input[type=number],select{padding:8px;border:1px solid #ccc;border-radius:4px;}
    .campo{margin:12px 0;}
    .dias label{margin-right:12px;cursor:pointer;}
    .atalho{padding:4px 12px;margin-right:4px;background:#e9ecef;border:1px solid #ccc;border-radius:4px;cursor:pointer;}
    .horario-box{background:#f0f4ff;padding:12px;border-radius:4px;margin-top:8px;border:1px solid #c8d8f8;}
</style>
)rawliteral";

// JavaScript salvo na flash (PROGMEM)
// Controla a interface da agenda no portal
const char WEB_SCRIPT[] PROGMEM = R"rawliteral(
<script>
// Mostra ou esconde os selects de horário dependendo do radio escolhido
function toggleHorario() {
    var ativo = document.getElementById('r_horario').checked;
    document.getElementById('div_horario').style.display = ativo ? 'block' : 'none';
}
// Marca os checkboxes dos dias de acordo com um bitmask
// bit 0 = Domingo, bit 1 = Segunda, ..., bit 6 = Sábado
// Exemplos: setDias(127) = todos, setDias(62) = dias úteis, setDias(65) = fim de semana
function setDias(mask) {
    for (var i = 0; i < 7; i++) {
        document.getElementById('dia' + i).checked = (mask >> i) & 1;
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
    // refresh automático a cada 2 minutos
    server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                                  "<meta http-equiv='refresh' content='120'>");
    server.sendContent(WEB_STYLE);
    server.sendContent("</head><body><h1>Monitor de Temperatura</h1>");

    // --- SEÇÃO 1: NOME DA PLACA ---
    String cardPlaca = "<div class='card'><h2>Configuração da Placa</h2>";
    cardPlaca += "<form action='/config/placa' method='POST'>";
    cardPlaca += "Nome: <input type='text' name='nome' value='" + getNomePlaca() + "' maxlength='32' required> ";
    cardPlaca += "<input type='submit' value='Salvar' class='btn btn-primary'></form></div>";
    server.sendContent(cardPlaca);

    // --- SEÇÃO 2: SENSORES CADASTRADOS ---
    server.sendContent("<div class='card'><h2>Sensores Cadastrados</h2><table>");
    server.sendContent("<tr><th>Nome</th><th>ID</th><th>Alerta</th><th>Temp</th><th>Ações</th></tr>");

    auto cadastrados = registry_getTodos();
    for (const auto& s : cadastrados) {
        float temp = hw_getTemp(s.id_fisico);

        // Verifica se está em manutenção ativa
        // (long)(millis() - mudo_ate) < 0 significa que o tempo ainda não acabou
        String statusMudo = "";
        if (s.mudo_ate > 0 && (long)(millis() - s.mudo_ate) < 0) {
            statusMudo = " <span style='color:orange'>[Manutenção]</span>";
        }

        // Envia cada célula separadamente — evita String grande na RAM
        server.sendContent("<tr><td>" + s.nome_amigavel + statusMudo + "</td>");
        server.sendContent("<td><code>" + s.id_fisico + "</code></td>");
        server.sendContent("<td>" + String(s.temp_max_alerta, 1) + "°C</td>");
        String tempStr = (temp == SENSOR_ERRO)
            ? "<span style='color:red'>OFF</span>"
            : String(temp, 1) + "°C";
        server.sendContent("<td>" + tempStr + "</td>");

        // Coluna de ações: Editar + Manutenção + Remover
        server.sendContent("<td>");
        server.sendContent("<a href='/editar?id=" + s.id_fisico + "' class='btn btn-primary btn-sm'>Editar</a> ");

        // Formulário inline de manutenção — seleciona duração e envia para /manutencao
        server.sendContent("<form action='/manutencao' method='POST' style='display:inline'>");
        server.sendContent("<input type='hidden' name='id' value='" + s.id_fisico + "'>");
        server.sendContent("<select name='horas' style='padding:3px'>");
        for (int h = 1; h <= 24; h++) {
            server.sendContent("<option value='" + String(h) + "'>" + String(h) + "h</option>");
        }
        server.sendContent("</select> ");
        server.sendContent("<button type='submit' class='btn btn-warning btn-sm'>Manutenção</button></form> ");

        server.sendContent("<a href='/remover?id=" + s.id_fisico + "' class='btn btn-danger btn-sm'>Remover</a>");
        server.sendContent("</td></tr>");
    }
    server.sendContent("</table></div>");

    // --- SEÇÃO 3: SENSORES NOVOS (DETECTADOS NO FIO) ---
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
            server.sendContent("<input type='number' step='0.1' name='max' value='8.0' min='-50' max='100' style='width:70px' required> °C ");
            server.sendContent("<input type='submit' value='Cadastrar' class='btn btn-primary'></form>");
            server.sendContent("</td></tr>");
        }
    }
    server.sendContent("</table></div></body></html>");
    server.sendContent(""); // finaliza o envio em chunks
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
    server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='utf-8'>");
    server.sendContent(WEB_STYLE);
    server.sendContent(WEB_SCRIPT);
    server.sendContent("</head><body><h1>Editar Sensor</h1><div class='card'>");
    server.sendContent("<form action='/salvar/sensor' method='POST'>");
    server.sendContent("<input type='hidden' name='id' value='" + s.id_fisico + "'>");

    // ── Nome
    server.sendContent("<div class='campo'>Nome: <input type='text' name='nome' value='" + s.nome_amigavel + "' maxlength='32' required></div>");

    // ── Temperaturas
    server.sendContent("<div class='campo'>Temp. alerta: <input type='number' name='max' value='" + String(s.temp_max_alerta, 1) + "' step='0.1' min='-50' max='100' style='width:80px'> °C</div>");
    server.sendContent("<div class='campo'>Temp. crítica: <input type='number' name='critica' value='" + String(s.temp_critica, 1) + "' step='0.1' min='-50' max='100' style='width:80px'> °C <small style='color:#888'>(aviso imediato)</small></div>");
    server.sendContent("<div class='campo'>Tempo de espera: <input type='number' name='espera' value='" + String(s.tempo_espera_min) + "' min='0' max='999' style='width:70px'> min <small style='color:#888'>(0 = avisa imediatamente)</small></div>");

    // ── Duração padrão do botão Manutenção
    server.sendContent("<div class='campo'>Silenciar por: <select name='horas_mudo'>");
    for (int h = 1; h <= 24; h++) {
        // (s.horas_mudo_padrao == h ? " selected" : "") marca a opção salva
        server.sendContent("<option value='" + String(h) + "'" + (s.horas_mudo_padrao == h ? " selected" : "") + ">" + String(h) + "h</option>");
    }
    server.sendContent("</select> <small style='color:#888'>(duração padrão do botão Manutenção)</small></div>");

    // ── Dias da semana (bitmask)
    // Para cada dia, verifica se o bit correspondente está aceso no bitmask
    const char* nomesDias[] = {"Dom","Seg","Ter","Qua","Qui","Sex","Sab"};
    server.sendContent("<div class='campo'><b>Dias monitorados:</b><br><div class='dias' style='margin:8px 0'>");
    for (int i = 0; i < 7; i++) {
        bool ativo = (s.dias_monitoramento >> i) & 1; // bit i aceso = dia ativo
        server.sendContent("<label><input type='checkbox' id='dia" + String(i) +
                           "' name='dia" + String(i) + "'" +
                           String(ativo ? " checked" : "") + "> " +
                           String(nomesDias[i]) + "</label>");
    }
    server.sendContent("</div>");
    // Botões de atalho — usam JavaScript setDias(mask) para marcar os checkboxes
    server.sendContent("<button type='button' class='atalho' onclick='setDias(127)'>Todos os dias</button>"); // 0b1111111
    server.sendContent("<button type='button' class='atalho' onclick='setDias(62)'>Dias úteis</button>");    // 0b0111110
    server.sendContent("<button type='button' class='atalho' onclick='setDias(65)'>Fim de semana</button>"); // 0b1000001
    server.sendContent("</div>");

    // ── Horário de monitoramento
    // Radio: "24 horas" ou "Horário específico"
    bool horarioAtivo = s.agenda_horario_ativo;
    server.sendContent("<div class='campo'><b>Horário:</b><br style='margin:6px 0'>");
    server.sendContent("<label><input type='radio' name='agenda_ativa' value='0' onclick='toggleHorario()'" +
                       String(!horarioAtivo ? " checked" : "") + "> 24 horas</label>&nbsp;&nbsp;");
    server.sendContent("<label><input type='radio' id='r_horario' name='agenda_ativa' value='1' onclick='toggleHorario()'" +
                       String(horarioAtivo ? " checked" : "") + "> Horário específico</label>");

    // Div de horário — visível só se "Horário específico" estiver selecionado
    server.sendContent("<div id='div_horario' class='horario-box' style='display:" +
                       String(horarioAtivo ? "block" : "none") + "'>");
    server.sendContent("Das ");

    // Select de hora início — valor enviado em minutos desde 00:00 (ex: 08:30 = 510)
    // Isso simplifica o recebimento no servidor: um único número em vez de hora+minuto separados
    int minInicioAtual = s.hora_inicio * 60 + s.min_inicio;
    server.sendContent("<select name='inicio'>");
    for (int h = 0; h < 24; h++) {
        for (int m = 0; m <= 30; m += 30) {
            int val = h * 60 + m;
            char buf[8];
            snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
            server.sendContent("<option value='" + String(val) + "'" +
                               (val == minInicioAtual ? " selected" : "") + ">" + buf + "</option>");
        }
    }
    server.sendContent("</select> às ");

    // Select de hora fim
    int minFimAtual = s.hora_fim * 60 + s.min_fim;
    server.sendContent("<select name='fim'>");
    for (int h = 0; h < 24; h++) {
        for (int m = 0; m <= 30; m += 30) {
            int val = h * 60 + m;
            char buf[8];
            snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
            server.sendContent("<option value='" + String(val) + "'" +
                               (val == minFimAtual ? " selected" : "") + ">" + buf + "</option>");
        }
    }
    server.sendContent("</select>");
    server.sendContent("<br><small style='color:#888'>Se o fim for menor que o início, o monitoramento atravessa a meia-noite.</small>");
    server.sendContent("</div></div>"); // fecha div_horario e campo

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

    // ── Tempo de espera e mudo padrão
    if (server.hasArg("espera"))     s.tempo_espera_min  = (uint16_t)server.arg("espera").toInt();
    if (server.hasArg("horas_mudo")) s.horas_mudo_padrao = (uint8_t)server.arg("horas_mudo").toInt();

    // ── Bitmask dos dias
    // Checkboxes HTML só enviam o campo se estiverem marcados — ausência = desmarcado
    uint8_t dias = 0;
    for (int i = 0; i < 7; i++) {
        if (server.hasArg("dia" + String(i))) {
            dias |= (1 << i); // liga o bit desse dia no bitmask
        }
    }
    s.dias_monitoramento = dias;

    // ── Horário
    // agenda_ativa == "1" = "Horário específico" foi selecionado
    s.agenda_horario_ativo = server.hasArg("agenda_ativa") && server.arg("agenda_ativa") == "1";

    if (s.agenda_horario_ativo) {
        // Os selects enviam minutos desde 00:00 (ex: 510 = 08:30)
        // Convertemos de volta para hora e minuto separados
        int minInicio = server.arg("inicio").toInt();
        int minFim    = server.arg("fim").toInt();
        s.hora_inicio = minInicio / 60; // divisão inteira: 510 / 60 = 8
        s.min_inicio  = minInicio % 60; // resto:           510 % 60 = 30
        s.hora_fim    = minFim / 60;
        s.min_fim     = minFim % 60;
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

        SensorConfig s;
        s.id_fisico           = server.arg("id");
        s.nome_amigavel       = nome;
        s.temp_max_alerta     = tempValida ? maxStr.toFloat() : TEMP_ALERTA_PADRAO;
        s.monitoramento_ativo = true;

        // ── Valores padrão para o novo sensor
        // Podem ser ajustados depois na página de edição (botão Editar)
        s.temp_critica         = s.temp_max_alerta + 5.0; // 5°C acima do alerta
        s.tempo_espera_min     = 0;                        // avisa imediatamente
        s.horas_mudo_padrao    = 1;                        // botão manutenção = 1h
        s.dias_monitoramento   = 127;                      // todos os dias (0b1111111)
        s.agenda_horario_ativo = false;                    // 24h por padrão
        s.hora_inicio          = 0;
        s.min_inicio           = 0;
        s.hora_fim             = 0;
        s.min_fim              = 0;
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
    server.on("/manutencao",    HTTP_POST, handleManutencao);    // botão manutenção
    server.on("/remover",                  handleRemoverSensor);
    server.begin();
    LOG("Servidor Web iniciado.");
}

void webPortalLoop() {
    server.handleClient();
}
