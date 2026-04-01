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
#include "logic/alert_manager.h"
#include "logger/logger.h"

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
"https://github.com/rodrigonie9/ESP32/releases/download/v1.0/firmware.bin";

// Flag que indica pedido de atualização
// volatile (sempre buscar real valor da memória), sem otimizar
volatile bool solicitarOTA = false;

//Controla se hora foi sincronizada via NTP
// false = sem hora, agendas desativadas
// revertida para true no loop() quando NTP responder
bool horaEstaSincronizada = false;

// ====================== CONTROLE DE TEMPO ======================
unsigned long ultimoEnvio = 0;              // Guarda o tempo do último envio
const unsigned long INTERVALO = 120000;      //  

unsigned long ultimoRelatorio = 0;
unsigned long ultimaLeitura = 0;

#define TAMANHO_MSG 512 //tamanho buffer


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
  configTime(-10800, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  LOG("Sincronizando hora via NTP...");
  
  //tenta garantir que pegou a hora certa (usa delay, até 5s em 10 tentativas)
  struct tm timeinfo;
  int tentativas = 0;
  while(!getLocalTime(&timeinfo) && tentativas < 20) {
    delay(1000);
    tentativas++;
  }
  if(tentativas < 20) {
    LOG("NTP sincronizado com sucesso");
    horaEstaSincronizada = true;
  } else {
    LOG("NTP falhou - continuando sem hora sincronizada");
    // sem NTP as agendas não funcionam - avisa o usuário via telegram
    enviarMensagemTelegram("AVISO: Falha ao sincronizar hora (NTP). Agendas desativas até o próximo reboot\n"
                            "Contatar o técnico para verificar a placa");
  }
  
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


  // ====================== INICIA TELEGRAM =====================
  // pega na NVS úlitmo ID de mensagem do telegram, com a mensagem /update
  iniciarTelegramNVS();


  
  // ====================== SENSORES ======================
  // Iniciar sensores Hardwatre
  registry_iniciar(); // memória e configurações

  hw_iniciar();      //hardware
  LOG("Sensore detectados: ");
  LOG(hw_getContagem());



  // ====================== INICIAR WEB PORTAL ======================
  iniciarWebPortal();
  LOG("Servido Web iniciado");

  logger_iniciar();


  // ====================== INFO FINAL ======================
  LOG("IP local:");
  LOG(WiFi.localIP().toString());

  // Envia mensagem inicial com IP e nome DNS
  char mensagem[256];
  snprintf(
    mensagem,
    sizeof(mensagem),
    "Placa %s online\n"
    "Sensores detectados: %d\n"
    "Acesse o portal:\n"
    "IP: http://%s\n"
    "Nome: http://%s.local",
    getNomePlaca().c_str(),
    hw_getContagem(),
    WiFi.localIP().toString().c_str(),  //IP atribuido pelo roteador
    getNomePlaca().c_str()              // endereço mDNS (funciona sem saber o IP)
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
    char bufInicio[80];
    snprintf(bufInicio, sizeof(bufInicio), "[%s] OTA iniciando agora...", getNomePlaca().c_str());
    enviarMensagemTelegram(bufInicio);
    delay(500); // tempo para enviar msg, liberar sockets, estabilizar wifi

    bool otaOk = atualizarFirmwareOTA(URL_FIRMWARE);
    // Se OTA bem-sucedido → ESP32 reinicia dentro da função (nunca chega aqui)
    // Se chegou aqui → OTA falhou, firmware atual continua rodando

    if (!otaOk) {
      // Avisa no Telegram que falhou — sem isso o usuário fica no escuro
      char bufFalha[128];
      snprintf(bufFalha, sizeof(bufFalha),
               "[%s] OTA FALHOU. Firmware atual continua rodando.\n"
               "Verifique se o arquivo .bin foi enviado ao GitHub.",
               getNomePlaca().c_str());
      enviarMensagemTelegram(bufFalha);
      LOG("OTA falhou.");
    }
    solicitarOTA = false;
  }

  // ===================== LEITURA TEMPERATURA ======================
  // Verifica se já passou o intervalo definido
  if (!solicitarOTA && millis() - ultimoEnvio >= INTERVALO) {
    // confere se não está atualizando firmware (solicitarOTA = true)
    // confere o tempo de 15minutos

    ultimoEnvio = millis(); 

    // Solicita leitura de todos os sensores
    hw_lerTodos();

    //Procesa lógica de alertas
    processarLogicaAlertas();

    // Envia leitura de todos os sensores ao Google Sheets
    logger_registrarLeitura();

    // Monta a mensagem
    char mensagem[TAMANHO_MSG]; //array de char tamanho fixo, memória liberada após sair do bloco
    char linha[64];

    //buffer fixo com snprinft, escreve direto no buffer, sem string na memória
    snprintf(mensagem, sizeof(mensagem), "Relatorio de Rotina\nPlaca: %s\n\n", getNomePlaca().c_str());

    // Pega a hora atual da placa
    // struct tm é uma "ficha" com campos separados: hora, minuto, dia, mês, ano...
    // getLocalTime() preenche essa ficha com a hora do NTP
    // Se NTP não sincronizou ainda, a função retorna false e pulamos a hora
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char horaAtual[32];
      // strftime formata a ficha tm em texto legível
      // %H = hora (00-23), %M = minuto, %d = dia, %m = mês, %Y = ano com 4 dígitos
      strftime(horaAtual, sizeof(horaAtual), "Hora: %H:%M de %d/%m/%Y\n\n", &timeinfo);
      // strncat cola horaAtual no final de mensagem, respeitando o limite do buffer
      strncat(mensagem, horaAtual, sizeof(mensagem) - strlen(mensagem) - 1);
    }

    uint8_t totalSensoresNoFio = hw_getContagem();

    for (uint8_t i = 0; i < totalSensoresNoFio; i++) {

      // & = "não copia, usa o texto original"
      // const = "promete não modificar"
      const String& id_fisico = hw_getID(i);
      float temp = hw_getTemp(id_fisico);
      const String& nome_amigavel = registry_getNome(id_fisico);

      if (temp == SENSOR_ERRO) {
          snprintf(linha, sizeof(linha), "- %s: ERRO\n", nome_amigavel.c_str());
      } else {
          snprintf(linha, sizeof(linha), "- %s: %.1f grC\n", nome_amigavel.c_str(), temp);
      }
      //strncat - concatena com segurança, respeitando limite do buffer
      strncat(mensagem, linha, sizeof(mensagem) - strlen(mensagem) - 1);
  }

    // Envia para o Telegram
    enviarMensagemTelegram(mensagem);

  }

  // ====================== PORTAL WEB ======================
  // atualiza webportal
  if (!solicitarOTA){
    webPortalLoop();      // só executa se não estiver atualizando (OTA)
  }
    

  // Se hora não foi sincronizada no boor, tenta novamente
  if (!horaEstaSincronizada) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)){
      horaEstaSincronizada = true;
      LOG("NTP sincronizado tardiamente");
      enviarMensagemTelegram("Hora sincronizada. Agendas reativadas");
    }
  }

  // Reboot diário às 03:00 - limpa heap fragmentado, reset wi-fo preso
  // grante nova sincronização NTP todos os dias
  // só executa se a hora estiver sincronizada, evita reeboot em hora errada
  if (horaEstaSincronizada){
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      if (timeinfo.tm_hour == 3 && timeinfo.tm_min == 0){
        LOG("Reboot diário programado - reiniciando");
        enviarMensagemTelegram("Reboot diário programado. Voltando em instantes.");
        delay(500);
        ESP.restart();
      }
    }
  }

  // pequeno delay para aliviar o cpu
  // permite esp32 processe tarefas internas (wifi stack, FreeRTOS, Wachtdog) sem bloquear por tempo fixo (delay(50))
  // loop roda mais rápido, webPortalLoop mais responsivo
  yield();

  // termina loop
  }