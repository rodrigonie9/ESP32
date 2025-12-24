#include <WiFi.h>            // Necessário para verificar conexão
#include <HTTPClient.h>      // Cliente HTTP

#include "telegram.h"        // Header do Telegram
#include "wifi_config.h"
#include "debug.h"

// ====================== DADOS DO TELEGRAM ======================

// Token do bot do Telegram
const char* botToken = "7963934643:AAEaVDm7FfoyEGXuTCVLar_5gJEvXWXhOsw";

// Lista de Chat IDs
const char* chatIDs[] = {
  "913490344",     // Rodrigo
  "8215871074",    // Gilberto
  "5713591355"     // Andrigo
};

// Quantidade total de chats cadastrados
const int TOTAL_CHATS = sizeof(chatIDs) / sizeof(chatIDs[0]);

// ====================== FUNÇÃO TELEGRAM ======================

// Função que envia mensagem para todos os chats cadastrados
void enviarMensagemTelegram(String mensagem) {

  // Verifica se tem internet
  if (!internetDisponivel()) {
    // ! = negação de lógica > !true > false , !false > true
    // se não tem internet, cai dentro do If
    return; // Sai da função
  }

  // ===== URL ENCODE SIMPLES =====
  mensagem.replace(" ", "%20");    // Espaços
  mensagem.replace("\n", "%0A");   // Quebra de linha
  mensagem.replace("°", "%C2%B0"); // Grau

  // Envia para todos os Chat IDs
  for (int i = 0; i < TOTAL_CHATS; i++) {

    HTTPClient http;               // Cria cliente HTTP

    // Monta a URL da API do Telegram
    String url = "https://api.telegram.org/bot";
    url += botToken;
    url += "/sendMessage?chat_id=";
    url += chatIDs[i];
    url += "&text=";
    url += mensagem;

    //Serial.print("Enviando para: ");
    //Serial.println(chatIDs[i]);

    http.begin(url);               // Inicia conexão


    int httpCode = http.GET();

    //Serial.print("[Telegram] HTTP Code: ");
    //Serial.println(httpCode);

    http.end();                    // Fecha conexão

    delay(500);                    // Evita bloqueio da API
  }
}
