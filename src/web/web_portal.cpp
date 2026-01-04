

#include <WebServer.h>
#include <WiFi.h>
#include <ESPmDNS.h>

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

  // Estilo simples
  html += "<style>";
  html += "body{font-family:Arial;margin:20px;}";
  html += "h1{color:#333;}";
  html += "table{border-collapse:collapse;}";
  html += "th,td{border:1px solid #ccc;padding:6px 10px;}";
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
    html += "<tr><th>#</th><th>ID do Sensor</th></tr>";

    for (uint8_t i = 0; i < total; i++) {

      html += "<tr>";
      html += "<td>";
      html += String(i);
      html += "</td>";

      html += "<td>";
      html += getSensorID(i);
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

