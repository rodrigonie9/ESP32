#include <Arduino.h>            // Biblioteca base do Arduino

// Bibliotecas dos sensores
#include <OneWire.h>            // Protocolo 1-Wire
#include <DallasTemperature.h>  // Leitura do DS18B20

#include <WiFi.h>               // só no main.cpp para debug

// Módulos do Projeto
#include "debug.h"
#include "wifi_config.h"
#include "telegram.h"
#include "ota_http.h"

//  ==================== OTA Git Hub ===================
// atualizar firmware via ota

// URL do firmware no GitHub
const char* URL_FIRMWARE =
"https://github.com/rodrigonie9/ESP32/raw/refs/heads/main/firmware/controle_temperatura_v1.bin";

// Flag que indica pedido de atualização
bool solicitarOTA = false;



// ====================== DS18B20 ======================
#define PINO_DATA 4   // GPIO onde os sensores estão ligados

// Cria o barramento 1-Wire no pino definido
OneWire oneWire(PINO_DATA);

// Cria o objeto que controla os sensores
DallasTemperature sensores(&oneWire);


// ====================== CONTROLE DE TEMPO ======================
unsigned long ultimoEnvio = 0;              // Guarda o tempo do último envio
const unsigned long INTERVALO = 900000;      //  



void setup() {
  // Inicia comunicação serial
  Serial.begin(115200);
  delay(1000);

  LOG("Iniciando sistema...");

  // ====================== CONEXÃO WI-FI ======================
  LOG("Conectando ao Wi-Fi");
  conectarWiFi();
  LOG("WiFi Conectado");

// ====================== INICIA SENSORES ======================
  sensores.begin();

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
    "%d de %d sensores de temperaturas ativos",
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
  if (millis() - ultimoEnvio >= INTERVALO) {

    // Solicita leitura de todos os sensores
    sensores.requestTemperatures();

    // Lê as temperaturas pelo índice
    float temp0 = sensores.getTempCByIndex(0);
    float temp1 = sensores.getTempCByIndex(1);

    // Monta a mensagem
    String mensagem = "Leitura de temperatura:\n";

    // Sensor 1
    if (temp0 == DEVICE_DISCONNECTED_C) {
      mensagem += "Resf. Bebidas: ERRO\n";
    } else {
      mensagem += "Resf. Bebidas: ";
      mensagem += String(temp0, 2);
      mensagem += " °C\n";
    }

    // Sensor 2
    if (temp1 == DEVICE_DISCONNECTED_C) {
      mensagem += "Cong. Padaria: ERRO\n";
    } else {
      mensagem += "Cong. Padaria: ";
      mensagem += String(temp1, 2);
      mensagem += " °C\n";
    }

    // Mostra no Monitor Serial
    LOG(mensagem);

    // Envia para o Telegram
    enviarMensagemTelegram(mensagem);
  
    // Atualiza o tempo do último envio
    ultimoEnvio = millis();
  }

  // pequeno delay para aliviar o cpu
  delay(50);

}