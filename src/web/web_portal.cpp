# include <WebServer.h>
# include <WiFi.h>

#include "web_portal.h"
#include "device/device_config.h"  // infrmações da placa esp32
#include "sensors/sensors.h"
#include "sensors/sensor_registry.h"
#include "debug.h"
#include "../config/system_limits.h"

WebServer server(80);

//Guardamos o CSS (eswtilo do webportal) na memória flash (PROGMEN) para economizar RAM
const char WEB_STYLE[] PROGMEM = R"rawliteral(
<style>
    body{font-family:sans-serif;margin:20px;background:#f4f4f9;color:#333;}
    .card{background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin-bottom:20px;}
    h1,h2{color:#444;}
    table{width:100%;border-collapse:collapse;margin-top:10px;}
    th,td{border:1px solid #ddd;padding:12px;text-align:left;}
    th{background:#f8f8f8;}
    .btn{padding:8px 15px;border:none;border-radius:4px;cursor:pointer;text-decoration:none;display:inline-block;margin:5px 0;}
    .btn-primary{background:#007bff;color:#fff;}
    .btn-danger{background:#dc3545;color:#fff;}
    input[type=text],input[type=number]{padding:8px;border:1px solid #ccc;border-radius:4px;}
</style>
)rawliteral";


// ======================== CONSTRUÇÃO DA PÁGINA ===============================================
// desenha o site no navegador do usuário
// usa sendContent, enviar site por pedaçoes
// protegendo memória RAM
void handleRoot() {
    // Avisa o navegador que vamos enviar o site em partes (chunks)
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='utf-8'>");
    server.sendContent(WEB_STYLE);
    server.sendContent("</head><body><h1>Monitor de Temperatura</h1>");

    // --- SEÇÃO 1: NOME DA PLACA ---
    String cardPlaca = "<div class='card'><h2>Configuração da Placa</h2>";
    cardPlaca += "<form action='/config/placa' method='POST'>";
    //Limita nome a 32 chars
    cardPlaca += "Nome: <input type='text' name='nome' value='" + getNomePlaca() + "' maxlength='32' required> ";
    cardPlaca += "<input type='submit' value='Salvar' class='btn btn-primary'></form></div>";
    server.sendContent(cardPlaca);

    // --- SEÇÃO 2: SENSORES CADASTRADOS ---
    server.sendContent("<div class='card'><h2>Sensores Cadastrados</h2><table>");
    server.sendContent("<tr><th>Nome</th><th>ID Físico</th><th>Limite</th><th>Temp</th><th>Ações</th></tr>");

    auto cadastrados = registry_getTodos();
    for (const auto& s : cadastrados) {
        float temp = hw_getTemp(s.id_fisico);
        String row = "<tr><td>" + s.nome_amigavel + "</td>";
        row += "<td><code>" + s.id_fisico + "</code></td>";
        row += "<td>" + String(s.temp_max_alerta, 1) + "°C</td>";
        row += "<td>" + (temp == SENSOR_ERRO ? "<span style='color:red'>OFF</span>" : String(temp, 1) + "°C") + "</td>";
        row += "<td><a href='/remover?id=" + s.id_fisico + "' class='btn btn-danger'>Remover</a></td></tr>";
        server.sendContent(row);
    }
    server.sendContent("</table></div>");

    // --- SEÇÃO 3: SENSORES NOVOS (DETECTADOS NO FIO) ---
    server.sendContent("<div class='card'><h2>Detectados no Barramento</h2><table>");
    server.sendContent("<tr><th>ID Físico</th><th>Ação</th></tr>");

    uint8_t total = hw_getContagem();
    for (uint8_t i = 0; i < total; i++) {
        String id = hw_getID(i);
        SensorConfig dummy;
        if (!registry_buscarPorID(id, dummy)) { // Só mostra se NÃO estiver cadastrado ainda
            String row = "<tr><td><code>" + id + "</code></td>";
            row += "<td><form action='/config/sensor' method='POST' style='display:inline;'>";
            row += "<input type='hidden' name='id' value='" + id + "'>";
            //Máximo 32 caracteres
            row += "<input type='text' name='nome' placeholder='Nome' maxlength='32' required> ";
            // min= -50   max=30   impede valores vazios
            row += "<input type='number' step='0.1' name='max' value='30.0' min='-50' max='100' style='width:60px;' required> ";
            row += "<input type='submit' value='Cadastrar' class='btn btn-primary'></form></td></tr>";
            server.sendContent(row);
        }
    }
    server.sendContent("</table></div></body></html>");
    server.sendContent(""); // Finaliza o envio
}



// ======================== AÇÕES E INICIALIZAÇÃO ===============================================
//Funções que recebem o que o usário digitou e salvam na memória
void handleConfigPlaca() {
  if (server.hasArg("nome")) {
    String nome = server.arg("nome");
    // rejeita nomes maiores de 32 caracteres - já bloqueado no HTML (web_portal)
    if (nome.length() > 0 && nome.length() <=32) {
        setNomePlaca(nome);
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleConfigSensor(){
  if (server.hasArg("id") && server.hasArg("nome")) {
    String nome = server.arg("nome");
    //rejeita nome vazio ou acima de 32 caracteres
    if (nome.length() == 0 || nome.length() > 32){
       server.sendHeader("Location","/");
       server.send(303); 
       return;
    }
    
    String maxStr = server.arg("max");
    // Verifica se o campo tem contéudo digital, sinal ou ponto
    // toFloat() retorna 0.0 tanto para "0" legítico quando para "abc" inválido
    // chechar o primeiro caractere é a única forma segura de distribuir os dois
    bool tempValida = maxStr.length() > 0 &&
                      (isdigit(maxStr[0]) || maxStr[0] == '-' || maxStr[0] == '.');
    
    SensorConfig s;
    s.id_fisico = server.arg("id");
    s.nome_amigavel = nome;  
    //condição ? valor_se_verdadeiro : valor_se_falso
    // if / else em uma só linha
    //       condição              ? valor_se_true    : valor_se_falso
    s.temp_max_alerta = tempValida ? maxStr.toFloat() : TEMP_ALERTA_PADRAO;
    s.monitoramento_ativo = true;
    registry_salvar(s);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleRemoverSensor() {
    if (server.hasArg("id")) registry_remover(server.arg("id"));
    server.sendHeader("Location", "/");
    server.send(303);
}

void iniciarWebPortal() {
    server.on("/", handleRoot);
    server.on("/config/placa", HTTP_POST, handleConfigPlaca);
    server.on("/config/sensor", HTTP_POST, handleConfigSensor);
    server.on("/remover", handleRemoverSensor);
    server.begin();
    LOG("Servidor Web iniciado.");
}

void webPortalLoop() {
    server.handleClient();
}