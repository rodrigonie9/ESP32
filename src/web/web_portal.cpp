#include "web_portal.h"

#include <WiFi.h>                   // Para obter IP e status
#include <WebServer.h>              // Servidor HTTP do esp32
#include "debug.h"                  // Sistema de Log

// Cria o servidor HTTP na porta 80
WebServer server(80);  //porta 80 padr]ao navegador 

// ======================== ROTAS ====================================

// Rota Principal: http://esp32.local
void handleRoot() {

    //HTTML simples para teste inciial
    String pagina =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<title>ESP32 Portal de Manutenção</title>"
        "</head>"
        "<body>"
        "<h1>Portal de Manutenção ESP32</h1>"
        "<p>Servidor web ativo.</p>"    
        "<p>" + WiFi.localIP().toString() + "</p>"
        "</body>"
        "</html>";
    server.send(200, "text/html", pagina);
}


// ======================== FUNÇÕES  PÚBLICAS ================================

void iniciarWebPortal() {

    LOG("Iniciando portal web...");

    // Define rota princiapal
    server.on("/", handleRoot);     //quando alguém acessar /. chama handleRoot

    // Inicia o servidor
    server.begin();
    LOG("Servidor Web iniciado.");
  
}

void webPortalLoop() {
    // Processa requisições HTTP
    // Deve ser chamado no loop principal
    // processa requisições pendentes, não bloqueia codigo
    server.handleClient();
}

