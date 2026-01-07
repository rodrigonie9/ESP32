#include <Arduino.h>            // Biblioteca base do Arduino

// Bibliotecas dos sensores
#include <OneWire.h>            // Protocolo 1-Wire
#include <DallasTemperature.h>  // Leitura do DS18B20

#include <WiFi.h>               // só no main.cpp para debug

// Módulos do Projeto
#include "debug.h"
#include "wifi_config/wifi_config.h"
#include "telegram/telegram.h"
#include "ota/ota_http.h"
#include "web/web_portal.h"
#include "device/device_config.h"  // Configuração nome da placa
#include <ESPmDNS.h>
#include "sensors/sensors.h"

// ============================================================================
// ======================== ATUALIZAÇÃO OTA (HTTP) ============================
//
// COMO FUNCIONA O OTA:
//
// 1. SALVAR > depois Compile o projeto normalmente (PlatformIO -> Build)
//    → será gerado o arquivo:
//      .pio/build/esp32dev/firmware.bin
//
//   → colar o firmware.bin para a pasta firmware dentro da pasta main
//     → renomear para firmware.bin (nome correto do raw no github)
//       → esse nome é que esta neste link do github para placa buscar e atalizar o firmare  
//  
// 2. Crie uma nova Release no GitHub:
//    - Tag: vX.Y.Z  (ex: v1.1.0)
//    - Anexe o arquivo firmware.bin
//
// 3. A URL abaixo deve apontar para o arquivo da Release:
//    https://raw.githubusercontent.com/<usuario>/<github_path>/<nome_do_firmware>"
// 4. Para iniciar a atualização:
//    - Envie o comando /update no Telegram
//    - Apenas o CHAT_AUTORIZADO pode solicitar
//
// OBSERVAÇÕES IMPORTANTES:
// - A pasta .pio NÃO deve ser enviada ao GitHub
// - O OTA só ocorre quando solicitado (não é automático)
// - Em caso de falha, o firmware atual continua rodando
//
// ============================================================================

// URL do firmware no GitHub
const char* URL_FIRMWARE =
"https://raw.githubusercontent.com/rodrigonie9/ESP32/main/firmware/firmware.bin";

// Flag que indica pedido de atualização
bool solicitarOTA = false;


// ====================== DS18B20 ======================
#define PINO_DATA 18   // GPIO onde os sensores estão ligados

// Cria o barramento 1-Wire no pino definido
OneWire oneWire(PINO_DATA);

// Cria o objeto que controla os sensores
DallasTemperature sensores(&oneWire);



// ====================== CONTROLE DE TEMPO ======================
unsigned long ultimoEnvio = 0;              // Guarda o tempo do último envio
const unsigned long INTERVALO = 120000;      //  



void setup() {
  // Inicia comunicação serial
  Serial.begin(115200);
  delay(1000);

  LOG("Iniciando sistema...");

  // ====================== INICIA CONFIG PLACA ======================
  iniciarDeviceConfig();

  // ====================== CONEXÃO WI-FI ======================
  LOG("Conectando ao Wi-Fi");
  conectarWiFi();
  LOG("WiFi Conectado");

  // ====================== DEFINE HOSTNAME USADO PELA PLACA ======================
  WiFi.setHostname(getNomePlaca().c_str());
  
  // ====================== INICIA MDNS ======================
  // Inicia mDNS (permite acessar a placa pelo nome, sem precisar saber o IP)
  if (MDNS.begin(getNomePlaca().c_str())) {
    LOG("mDNS ativo em:");
    LOG("http://" + getNomePlaca() + ".local");
  }

  // ====================== INICIAR WEB PORTAL ======================
  iniciarWebPortal();

  // ====================== INICIA TELEGRAM =====================
  // pega na NVS úlitmo ID de mensagem do telegram, com a mensagem /update
  iniciarTelegramNVS();

  // ====================== INICIA SENSORES ======================
  iniciarSensores();

  LOG("Sensores detectados:");
  LOG(getQuantidadeSensores());

  sensores.begin();

  detectarSensores();

  // Conta quantos sensores existem no barramento
  int qtd = sensores.getDeviceCount();
  LOG("Sensores encontrados: ");
  LOG(qtd);
  

  LOG("WiFi status:");
  LOG(WiFi.status());

  LOG("IP local:");
  LOG(WiFi.localIP().toString());

  // Envia mensagem inicial
  char mensagem_inicial[64];
  snprintf(
    mensagem_inicial,
    sizeof(mensagem_inicial),
    "%d de %d sensores de temperaturas ativos RODRIGO",
    qtd,    // número de sensores ativos
    2       // total de sensores instalados
  );
  enviarMensagemTelegram(mensagem_inicial);
}

void loop() {

  // ===================== ATUALIZAÇÃO FIRMWARE =====================
  // Verifica mensagens do Telegram
  verificarMensagensTelegram();
  // Essa função é rápida e não bloqueia

  // Verifica se foi solicitado OTA
  if (comandoAtualizarRecebido()) {
    solicitarOTA = true;
  }

  if (solicitarOTA) {
    LOG("Iniciando atualização OTA...");
    enviarMensagemTelegram("Atualização de OTA iniciando agora");
    delay(500); //tempo enviar msg, liberar sockets https, estabiizar wifi
    atualizarFirmwareOTA(URL_FIRMWARE);
    solicitarOTA = false;
    // Se OTA iniciar com sucesso, a ESP32 reinicia
    // Se falhar, continua rodando firmware atual
  }

  // ===================== LEITURA TEMPERATURA ======================
  // Verifica se já passou o intervalo definido
  if (!solicitarOTA && millis() - ultimoEnvio >= INTERVALO) {
    // confere se não está atualizando firmware (solicitarOTA = true)
    // confere o tempo de 15minutos

    // Solicita leitura de todos os sensores
    atualizarSensores();

    // Monta a mensagem
    String mensagem = "Leitura de temperatura:\n";

    uint8_t quantidadeSensores = getQuantidadeSensores();

    for (uint8_t i = 0; i < quantidadeSensores;i++) {

      float temp = getUltimaTemperatura(i);

        if (temp == DEVICE_DISCONNECTED_C) {
        mensagem += "Sensor ";
        mensagem += i;
        mensagem += ": ERRO\n";
        } else {
        mensagem += "Sensor ";
        mensagem += i;
        mensagem += ": ";
        mensagem += String(temp, 1);
        mensagem += " °C\n";
        }
      }

      // Envia para o Telegram
      enviarMensagemTelegram(mensagem);

      // Atualiza o tempo do último envio
      ultimoEnvio = millis();
    }

  // ====================== PORTAL WEB ======================
  // atualiza webportal
  webPortalLoop();

  // pequeno delay para aliviar o cpu
  delay(50);

  // termina loop
  }