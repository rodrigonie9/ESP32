//implementa as funções, como funciona

#include <WiFiManager.h>      // Biblioteca WiFiManager
#include <HTTPClient.h>
#include <WiFi.h>

#include "wifi_config.h"     // Header deste módulo
#include "debug.h"           // Macro LOG() — liga/desliga logs pelo debug.h


// ============================================================================
// CONEXÃO INICIAL — chamada uma vez no setup()
// ============================================================================

// Função responsável por conectar o ESP32 ao WiFi
void conectarWiFi() {

  // Tenta conectar no WiFi salvo
  // Se não existir WiFi salvo, cria um Access Point

  // na primeira vez cria o access point, usuário conecta wifi pelo acces point
  // esp32 deixa salvo informações de rede salvo na placa
  // para resetar (perder informçõaes de rede)
  // 1 - botão boot
  // 2 - wm.resetSettings();   // Apaga Wi-Fi salvo

  WiFiManager wm;             // Cria o objeto WiFiManager

  //wm.resetSettings();       // reseta configução de internet()senhas salvas

  //aguarda até 90' para roteador subir após quedas de energia
  //sem isso, esp32 reinicia em loop enquanto roteador ainda está inicializando
  wm.setConnectTimeout(90);

  bool conectado = wm.autoConnect("ESP32_Config","12345678");

  // Verifica se conseguiu conectar
  if (!conectado) {
    // Caso falhe até no portal de configuração
    ESP.restart();            // Reinicia o ESP32
  }
}


// ============================================================================
// VERIFICAÇÃO DE INTERNET — testa acesso real, não só Wi-Fi
// ============================================================================

// Verifica se existe acesso real à internet.
// Não basta estar conectado ao Wi-Fi — o roteador pode estar online mas
// sem internet (ex: provedor caiu). Por isso testamos endpoints externos.
bool temInternet() {

  if (WiFi.status() != WL_CONNECTED) {
    // Se nem o Wi-Fi está conectado, com certeza não há internet
    return false;
  }

  // Testa múltiplos endpoints — se qualquer um responder, há internet.
  // Usar três fontes diferentes evita falso negativo caso um serviço
  // específico esteja fora do ar ou bloqueado na rede local.
  const char* endpoints[] {
        "http://www.google.com/generate_204",               // Google
        "http://www.msftconnecttest.com/connecttest.txt",   // Microsoft
        "http://connectivitycheck.gstatic.com/generate_204" // Android/Google
  };

  HTTPClient http;
  http.setTimeout(3000); // timeout de 3s por endpoint — evita travar o loop

  for (const char* url : endpoints) {
    http.begin(url);
    int codigo = http.GET();
    http.end();
    if (codigo > 0) return true; // qualquer resposta positiva = internet ok
  }

  return false;
  // http.GET() retorna número positivo se recebeu resposta, negativo em erro
  // (timeout, DNS falhou, sem rota, etc.)
}


// ============================================================================
// RESET DO DRIVER WI-FI — usado pelo watchdog após 6 min sem internet
// ============================================================================

// Desliga o rádio Wi-Fi completamente e reinicia do zero.
// Diferente de disconnect/reconnect, isso limpa o estado interno do driver —
// útil quando o stack ficou corrompido após uma queda de rede.
//
// WiFi.begin() sem argumentos usa as credenciais salvas pelo WiFiManager
// no flash da placa — não precisa saber SSID nem senha no código.
//
// static = função privada, só usada dentro deste arquivo
static void resetarDriverWifi() {
  LOG("[WiFi] Resetando driver Wi-Fi (WIFI_OFF -> WIFI_STA -> begin)...");

  WiFi.mode(WIFI_OFF);  // desliga completamente o rádio Wi-Fi

  // Aguarda 1 segundo em fatias para manter watchdog alimentado
  for (int t = 0; t < 100; t++) { delay(10); yield(); }

  WiFi.mode(WIFI_STA);  // liga de volta em modo estação (cliente)
  WiFi.begin();         // reconecta usando credenciais salvas pelo WiFiManager

  LOG("[WiFi] Driver resetado. Aguardando reconexão...");
}


// ============================================================================
// INTERNET DISPONÍVEL — função principal chamada antes de qualquer acesso
// ============================================================================

// Marca o momento (em ms desde o boot) em que a internet foi confirmada.
// Usado pelo watchdog para calcular quanto tempo estamos sem internet.
// static = persiste entre chamadas, mas só existe neste arquivo.
static unsigned long msUltimoInternetOk = 0;

// Flag que indica se a internet já funcionou ao menos uma vez desde o boot.
// Evita que o watchdog dispare durante o boot se a rede demorar a subir.
static bool internetConfirmadaUmaVez = false;

// Controla o log de "sem internet" para não repetir a cada chamada.
// internetDisponivel() é chamada por Telegram, Logger e OTA — sem esse
// controle, a mensagem apareceria 3+ vezes por ciclo no monitor serial.
static bool logSemInternetJaFeito = false;

// Função única que deve ser usada antes de qualquer acesso à internet.
// Retorna true se há internet agora, false caso contrário.
// Não tenta reconectar — deixamos isso para o auto-reconnect da ESP32
// e para o watchdog (verificarWatchdogInternet), que age de forma escalonada.
bool internetDisponivel() {

  if (temInternet()) {
    // Internet ok — atualiza o timestamp e marca que já funcionou
    if (!internetConfirmadaUmaVez) {
      LOG("[WiFi] Internet confirmada pela primeira vez.");
    }
    // Se estava sem internet e voltou: loga a recuperação uma vez
    if (logSemInternetJaFeito) {
      LOG("[WiFi] Internet voltou — retomando operação normal.");
      logSemInternetJaFeito = false;
    }
    msUltimoInternetOk       = millis();
    internetConfirmadaUmaVez = true;
    return true;
  }

  // Sem internet — loga apenas na primeira detecção para não poluir o monitor.
  // As próximas chamadas no mesmo ciclo (Telegram, Logger, OTA) ficam em silêncio.
  if (!logSemInternetJaFeito) {
    LOG("[WiFi] Sem internet. Aguardando auto-reconnect...");
    logSemInternetJaFeito = true;
  }

  // Sem internet — retorna false sem forçar disconnect/reconnect.
  // Forçar reconexão aqui atrapalhava o auto-reconnect nativo da ESP32
  // e podia corromper o driver Wi-Fi em quedas momentâneas.
  // O watchdog (verificarWatchdogInternet) cuida da recuperação escalada.
  return false;
}


// ============================================================================
// WATCHDOG DE INTERNET — chamado a cada ciclo no loop() do main.cpp
// ============================================================================
//
// Estratégia escalonada para recuperar a internet sem intervenção manual:
//
//   0–6 min sem internet  →  aguarda (auto-reconnect nativo da ESP32 tenta)
//   6 min                 →  reset do driver Wi-Fi (limpa estado corrompido)
//   6–12 min              →  aguarda nova reconexão após o reset
//   12 min                →  restart completo da placa (garantido resolver tudo)
//
// Após o restart, o ciclo recomeça do zero.
// Se a internet voltar em qualquer ponto, o watchdog se reseta sozinho.
//
// ============================================================================

// Quanto tempo sem internet antes de agir — 6 minutos = 3 ciclos de leitura
const unsigned long WATCHDOG_FASE1_MS = 6UL * 60UL * 1000UL;  // 6 min em ms

// Quanto tempo esperar após o reset do driver antes de reiniciar a placa
const unsigned long WATCHDOG_FASE2_MS = 6UL * 60UL * 1000UL;  // mais 6 min

// Guarda se já fizemos o reset do driver neste ciclo de falha
static bool resetDriverFeito = false;

// Guarda quando foi feito o reset do driver
static unsigned long msResetDriverFeito = 0;

void verificarWatchdogInternet() {

  // Se a internet nunca funcionou desde o boot, não age.
  // Pode ser que o roteador ainda esteja inicializando — não queremos
  // reiniciar em loop logo após ligar a placa.
  if (!internetConfirmadaUmaVez) return;

  // Se a internet está ok (foi confirmada há pouco), reseta o watchdog
  // e volta ao estado normal. Também limpa o flag de reset do driver
  // para que, na próxima falha, o ciclo comece do zero.
  unsigned long semInternetMs = millis() - msUltimoInternetOk;
  if (semInternetMs < WATCHDOG_FASE1_MS) {
    // Menos de 6 minutos — ainda dentro da tolerância normal.
    // O auto-reconnect da ESP32 está tentando em background.
    resetDriverFeito = false; // garante que o ciclo começa limpo
    return;
  }

  // ── FASE 1: 6 minutos sem internet ───────────────────────────────────────
  // Auto-reconnect não resolveu. Tenta reset do driver Wi-Fi.
  if (!resetDriverFeito) {
    LOG("[WATCHDOG] 6 min sem internet. Tentando reset do driver Wi-Fi...");
    resetarDriverWifi();
    resetDriverFeito    = true;
    msResetDriverFeito  = millis();
    return;
    // Aguardamos o próximo ciclo para ver se o reset resolveu
  }

  // ── FASE 2: 6 minutos após o reset do driver ─────────────────────────────
  // Se chegou aqui, o reset do driver não resolveu.
  // Única saída garantida: restart completo da placa.
  if (millis() - msResetDriverFeito >= WATCHDOG_FASE2_MS) {
    LOG("[WATCHDOG] 12 min sem internet. Reset do driver nao resolveu. Reiniciando placa...");
    delay(200); // tempo para a mensagem serial sair antes do restart
    ESP.restart();
  }

  // Se chegou aqui: reset foi feito mas ainda não passaram 6 min.
  // Aguardando — nada a fazer por enquanto.
}
