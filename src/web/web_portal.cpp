

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
    html += "<th>Temp (°C)</th>";
    html += "<th>Erros</th>";
    html += "</tr>";

    for (uint8_t i = 0; i < total; i++) {

      // ======  BUSCA DADOS DO SENSOR ======
      SensorStats s = getSensorStats(i);
      float temp = getTemperatura(i);

      html += "<tr>";

      // Índice
      html += "<td>";
      html += String(i);
      html += "</td>";

      // ID físico
      html += "<td>";
      html += getSensorID(i);
      html += "</td>";

      // ======  TEMPERATURA ======
      html += "<td>";
      if (temp == DEVICE_DISCONNECTED_C) {
        html += "ERRO";
      } else {
        html += String(temp, 1);
      }
      html += "</td>";

      // ====== CONTADOR DE ERROS ======
      html += "<td>";
      html += String(s.erros);
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

