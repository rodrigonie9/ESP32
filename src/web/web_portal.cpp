

#include <WebServer.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DallasTemperature.h>

#include "web_portal.h"
#include "device/device_config.h"
#include "debug.h"
#include "sensors/sensors.h"

// Servidor HTTP na porta 80
WebServer server(80);

// ==========================
// Página principal
// ==========================
void handleRoot() {

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<title>ESP32 - Sensores</title>";

  // ================= ESTILO =================
  html += "<style>";
  html += "body{font-family:Arial;margin:20px;}";
  html += "h1{color:#333;}";
  html += "table{border-collapse:collapse;}";
  html += "th,td{border:1px solid #ccc;padding:6px 10px;text-align:center;}";
  html += "</style>";

  html += "</head><body>";

  // ================= CABEÇALHO =================
  html += "<h1>Monitor de Temperatura</h1>";

  html += "<p><b>Dispositivo:</b> ";
  html += getNomePlaca();
  html += "</p>";

  html += "<p><b>IP:</b> ";
  html += WiFi.localIP().toString();
  html += "</p>";

  html += "<hr>";

  // ================= SENSORES =================
  html += "<h2>Sensores detectados</h2>";

  uint8_t total = getQuantidadeSensores();

  if (total == 0) {

    html += "<p>Nenhum sensor detectado.</p>";

  } else {

    html += "<table>";

        // ====== CABEÇALHO DA TABELA ======
    html += "<tr>";
    html += "<th>#</th>";
    html += "<th>ID do Sensor</th>";
    html += "<th>Temperatura (°C)</th>";
    html += "<th>Leituras OK</th>";
    html += "<th>Leituras ERRO</th>";
    html += "</tr>";

    for (uint8_t i = 0; i < total; i++) {

      // Busca dados já medidos pelo loop principal
      float temp = getUltimaTemperatura(i);
      SensorStats s = getSensorStats(i);

      html += "<tr>";

      // Índice do sensor
      html += "<td>";
      html += String(i);
      html += "</td>";

      // ID físico
      html += "<td>";
      html += getSensorID(i);
      html += "</td>";

      // Temperatura
      html += "<td>";
      if (s.ultimo_erro) {
        html += "<span style='color:red;'>ERRO</span>";
      } else {
        html += String(temp, 1);
      }
      html += "</td>";

      // Leituras OK
      html += "<td>";
      html += String(s.leituras_ok);
      html += "</td>";

      // Leituras com erro
      html += "<td>";
      html += String(s.leituras_erro);
      html += "</td>";

      html += "</tr>";
    }

    html += "</table>";
  }

  html += "</body></html>";

  server.send(200, "text/html", html);
}

// ==========================
// Inicialização
// ==========================
void iniciarWebPortal() {

  // Rota principal
  server.on("/", handleRoot);

  // Inicia servidor
  server.begin();

  LOG("Servidor Web iniciado.");
}

// ==========================
// Loop
// ==========================
void webPortalLoop() {
  server.handleClient();
}

