#include "ota_http.h"
#include "wifi_config.h"

#include <WiFi.h>        // Controle do Wi-Fi
#include <HTTPClient.h> // Cliente HTTP
#include <Update.h>     // Biblioteca de atualização OTA

// Função que executa a atualização OTA via HTTP
bool atualizarFirmwareOTA(const char* urlFirmware) {

  // Verifica se tem internet
  if (!internetDisponivel()) {
    // ! = negação de lógica > !true > false , !false > true
    // se não tem internet, cai dentro do If
    return false; // Sai da função, 
  }

  HTTPClient http;
  // Cria o cliente HTTP

  http.setTimeout(20000);
  // Define timeout de 20 segundos para download

  http.begin(urlFirmware);
  // Inicia a conexão com a URL do firmware (.bin)

  int httpCode = http.GET();
  // Faz a requisição HTTP

  if (httpCode != HTTP_CODE_OK) {
    // Se o servidor não respondeu corretamente
    http.end();
    return false;
  }

  int tamanhoFirmware = http.getSize();
  // Obtém o tamanho total do firmware

  if (tamanhoFirmware <= 0) {
    // Tamanho inválido do arquivo
    http.end();
    return false;
  }

  // Inicializa o processo de atualização
  // Verifica se há espaço suficiente na partição OTA
  if (!Update.begin(tamanhoFirmware)) {
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  // Obtém o fluxo de dados do firmware

  size_t bytesEscritos = Update.writeStream(*stream);
  // Escreve o firmware recebido diretamente na flash

  if (bytesEscritos != tamanhoFirmware) {
    // Nem todos os bytes foram gravados corretamente
    Update.end();
    http.end();
    return false;
  }

  // Finaliza a atualização e valida o firmware
  if (!Update.end(true)) {
    http.end();
    return false;
  }

  http.end();
  // Fecha a conexão HTTP

  // Reinicia a ESP32 para rodar o novo firmware
  ESP.restart();

  return true;
}
