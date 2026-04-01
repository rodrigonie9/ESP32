// ============================================================================
// Função: atualizarFirmwareOTA
//
// Responsável por:
// - Baixar o firmware (.bin) via HTTP/HTTPS
// - Gravar o firmware na partição OTA
// - Validar a gravação
// - Reiniciar a ESP32 para rodar o novo firmware
//
// Segurança:
// - Verifica acesso à internet antes de iniciar
// - Não apaga o firmware atual se a atualização falhar
// - Usa partições OTA (sem risco de brick)
//
// Retorno:
// - true  -> OTA iniciado com sucesso (ESP reinicia)
// - false -> Falha (firmware atual continua rodando)
// ============================================================================

#include "ota_http.h"
#include "wifi_config/wifi_config.h"

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

  // Segue redirecionamentos automaticamente
  // GitHub Releases responde com HTTP 302 (redirecionamento para CDN)
  // Sem essa linha, o HTTPClient vê o 302 e desiste — OTA falha silenciosamente
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  http.begin(urlFirmware);
  // Inicia a conexão com a URL do firmware (.bin)

  int httpCode = http.GET();
  // Faz a requisição HTTP

  if (httpCode != HTTP_CODE_OK) {
    // Loga o código HTTP recebido — ajuda a entender o que falhou
    // 302 = redirecionamento não seguido | 404 = arquivo não encontrado | -1 = sem conexão
    Serial.printf("[OTA] Falha HTTP: código %d\n", httpCode);
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
