
#include <Preferences.h>            // acessar nvs (memória placa para salva ultimo id_chat telegram)
static Preferences prefsTelegram;   // objeto responsável acessar NVS (flash interna)

#include <WiFi.h>            // Necessário para verificar conexão
#include <HTTPClient.h>      // Cliente HTTP

#include "telegram.h"        // Header do Telegram
#include "wifi_config/wifi_config.h"
#include "debug.h"
#include "secrets.h"


// ====================== DADOS DO TELEGRAM ======================
// Lista de Chat IDs
const char* chatIDs[] = {
  "913490344",     // Rodrigo
  // "8215871074",    // Gilberto
  // "5713591355"     // Andrigo
};

// Quantidade total de chats cadastrados
const int TOTAL_CHATS = sizeof(chatIDs) / sizeof(chatIDs[0]);


// ====================== VARIÁVEIS OTA =======================
// Intervalo de verificação do Telegram Status Update(em ms)
// usa intervalo para que não cheque a internet nem acesse o telegram com muita frequencia
#define INTERVALO_TELEGRAM 60000
// Guarda o último momento da verificação
static unsigned long ultimoCheckTelegram = 0;

// Guarda o último update_id processado
// cada mensagem do telegram tem um ID
static long ultimoUpdateID = 0;

// Flag interna que sinaliza pedido de OTA
static bool pedidoOTA = false;




// ====================== FUNÇÕES UPDATE OTA TELEGRAM ==========

// ============================================================================
// Verifica comandos recebidos via Telegram
//
// Comando suportado:
// - /update
//
// Regras de segurança:
// - Apenas mensagens vindas do CHAT_AUTORIZADO são aceitas
// - O comando apenas sinaliza o OTA (não executa aqui)
// - A execução real do OTA ocorre no main.cpp
//
// A verificação é feita em intervalos para evitar:
// - excesso de chamadas à API do Telegram
// - consumo desnecessário de internet
// ============================================================================

// ============================================================================
// Inicializa a NVS do Telegram
// Deve ser chamada UMA VEZ no boot
// ============================================================================
void iniciarTelegramNVS() {

  prefsTelegram.begin("telegram", false);
  // Abre a gaveta (namespace) "telegram" na NVS
  // false = leitura e escrita

  ultimoUpdateID = prefsTelegram.getLong("lastUpdate", 0);
  // Lê da NVS o último update_id salvo
  // Se não existir ainda, assume 0

  prefsTelegram.end();
  // Fecha a NVS
}


// Função que verifica se chegou comando /update no Telegram
void verificarMensagensTelegram() {
 
  //verifica se já passou o intervalo
  // não queremos API do Telegram nem consultar internet com muita frequencia
 if (millis() - ultimoCheckTelegram < INTERVALO_TELEGRAM) {
    return; // Ainda não é hora de verificar
  }
  // atualiza tempo da última verificação
  ultimoCheckTelegram = millis();
  
  // Verifica se tem internet
  if (!internetDisponivel()) {
    return;
  }

  HTTPClient http;

  // Monta a URL da API getUpdates
  String url = "https://api.telegram.org/bot";
  url += TELEGRAM_BOT_TOKEN;
  url += "/getUpdates?offset=";
  url += String(ultimoUpdateID + 1);      // pede um mais nova que a última processada

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode != 200) {
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

if (!pedidoOTA &&
      payload.indexOf(CHAT_AUTORIZADO) != -1 &&
      payload.indexOf("/update") != -1) {

  pedidoOTA = true;
  enviarMensagemTelegram("Comando /update recebido. Iniciando OTA...");
}

  // procura último "update_id" para o número
  int pos = payload.lastIndexOf("\"update_id\":");
  if (pos != -1) {
    // converte o valor do update_id para número
    ultimoUpdateID = payload.substring(pos + 12).toInt();

    //salva o update_id na NVS
    // após o reboot, esp32, lembra que já prcessou essa mensagem
    prefsTelegram.begin("telegram", false);
    prefsTelegram.putLong("lastUpdate", ultimoUpdateID);
    //fecha NVS telegra
    prefsTelegram.end(); 
  }

}

// Função que informa ao main.cpp se o comando /update foi recebido
bool comandoAtualizarRecebido() {

  // Se houve pedido de OTA
  if (pedidoOTA) {

    pedidoOTA = false;
    // Limpa a flag para não repetir a atualização

    return true;
    // Informa ao main.cpp que deve iniciar OTA
  }

  return false;
  // Nenhum pedido de OTA pendente
}

void cancelarPedidoOTA() {
  pedidoOTA = false;
}





// ====================== FUNÇÃO TELEGRAM ======================

// Função que envia mensagem para todos os chats cadastrados
void enviarMensagemTelegram(String mensagem) {

  // Verifica se tem internet
  if (!internetDisponivel()) {
    // ! = negação de lógica > !true > false , !false > true
    // se não tem internet, cai dentro do If
    return; // Sai da função
  }

  // Envia para todos os Chat IDs
  for (int i = 0; i < TOTAL_CHATS; i++) {

    HTTPClient http;               // Cria cliente HTTP

    // Monta a URL da API do Telegram
    String url = "https://api.telegram.org/bot";
    url += TELEGRAM_BOT_TOKEN;
    url += "/sendMessage";
 
    http.begin(url);               // Inicia conexão

    String body = "{\"chat_id\":\""; //Monta (JSON) mensage, chaI_id e o text
    body += chatIDs[i];
    body += "\",\"text\":\"";
    body += mensagem;
    body += "\"}";
  
    http.addHeader("Content-Type", "application/json"); //informa body é um JSON 

    int httpCode = http.POST(body); //envia post com body JSON

    http.end();                    // Fecha conexão

    delay(500);                    // Evita bloqueio da API
  }
}



