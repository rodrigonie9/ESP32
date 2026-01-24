#include <Arduino.h>            // Biblioteca base do Arduino

#include <WiFi.h>               // só no main.cpp para debug

#include <time.h>

// Módulos do Projeto
#include "debug.h"
#include "wifi_config/wifi_config.h"
#include "telegram/telegram.h"
#include "ota/ota_http.h"
#include "web/web_portal.h"
#include "device/device_config.h"  // Configuração nome da placa
#include "sensors/sensors.h"
#include "sensors/sensor_registry.h"

#include <ESPmDNS.h>              // identidade do dispositivo placa

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


// ====================== CONTROLE DE TEMPO ======================
unsigned long ultimoEnvio = 0;              // Guarda o tempo do último envio
const unsigned long INTERVALO = 120000;      //  

unsigned long ultimoRelatorio = 0;
unsigned long ultimaLeitura = 0;



void setup() {
  // ====================== SERIAL ==============================================
  Serial.begin(115200);
  delay(1000);
  LOG("Iniciando sistema...");

  // ====================== INICIA CONFIG PLACA ==================================
  iniciarDeviceConfig();
  LOG("Nome da placa: ");
  LOG(getNomePlaca());
  
  // ====================== CONEXÃO WI-FI ========================================
  LOG("Conectando ao Wi-Fi");
  conectarWiFi();
  LOG("WiFi Conectado");

  // ====================== CONEXÃO WI-FI ========================================
  // O valor -10800 é: -3 horas * 3600 segundos // ajustar fuso
  configTime(-10800, 0, "pool.ntp.org", "time.nist.gov");
  LOG("Sincronizando hora via NTP...");


  // ====================== DEFINE HOSTNAME USADO PELA PLACA ======================
  WiFi.setHostname(getNomePlaca().c_str());
  

  // ====================== INICIA MDNS ======================
  // Inicia mDNS (permite acessar a placa pelo nome, sem precisar saber o IP)
  if (MDNS.begin(getNomePlaca().c_str())) {
    LOG("mDNS ativo em:");
    LOG("http://" + getNomePlaca() + ".local");
  } else {
    LOG("Falha ao iniciar mDNS");
  }
  
  // ====================== SENSORES ======================
  // Iniciar sensores Hardwatre
  registry_iniciar(); // memória e configurações

  hw_iniciar();      //hardware
  LOG("Sensore detectados: ");
  LOG(hw_getContagem());




  // ====================== INICIAR WEB PORTAL ======================
  iniciarWebPortal();
  LOG("Servido Web iniciado");


  // ====================== INICIA TELEGRAM =====================
  // pega na NVS úlitmo ID de mensagem do telegram, com a mensagem /update
  iniciarTelegramNVS();

  // ====================== INFO FINAL ======================
  LOG("IP local:");
  LOG(WiFi.localIP().toString());

  // Envia mensagem inicial
  char mensagem[128];
  snprintf(
    mensagem,
    sizeof(mensagem),
    "Placa %s online\nSensores detectados: %d",
    getNomePlaca().c_str(),
    hw_getContagem()
  );
  enviarMensagemTelegram(mensagem);
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

    ultimoEnvio = millis(); 

    // Solicita leitura de todos os sensores
    hw_lerTodos();

    // Monta a mensagem
    String mensagem = "📊 *Relatório de Rotina*\n";
    mensagem += "Placa: " + getNomePlaca() + "\n\n";

    // Total Sensores ESP32, que está enxergando no fio, conectado
    uint8_t totalSensoresNoFio = hw_getContagem();

    for (uint8_t i = 0; i < totalSensoresNoFio; i++) {

      // 1 Pega o ID do sensor pela posição (evita trabalhar por indice
      //   se perder um sensor no meio do loop, erra leitura por camera
      String id_fisico = hw_getID(i);

      // 2 Busca temperatura usando ID do sensore
      float temp = hw_getTemp(id_fisico);

      // 3 Pega nome amigável do sensor
      String nome_amigavel = registry_getNome (id_fisico);

      // Escreve informação sensor na mensagem
      mensagem += "🔹 " + nome_amigavel + ": ";
      if (temp == SENSOR_ERRO) {
        mensagem += "ERRO\n";
      } else {
        mensagem += String (temp, 1) + "°C\n";
      }

    }

      // Envia para o Telegram
      enviarMensagemTelegram(mensagem);

  }

  // ====================== PORTAL WEB ======================
  // atualiza webportal
  if (!solicitarOTA){
    webPortalLoop();      // só executa se não estiver atualizando (OTA)
  }
    

  // pequeno delay para aliviar o cpu
  delay(50);

  // termina loop
  }