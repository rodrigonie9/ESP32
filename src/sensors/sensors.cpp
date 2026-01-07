#include "sensors.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// ================= CONFIGURAÇÃO =================
#define PINO_DATA 18  // GPIO onde os sensores estão ligados

static SensorStats stats[20];         // Suporta até 20 sensores
static float ultimaTemperatura[20];    // 


static OneWire oneWire(PINO_DATA);
static DallasTemperature sensores(&oneWire);

// Armazena quantos sensores existem
static uint8_t totalSensores = 0;

// ================= IMPLEMENTAÇÃO =================

void iniciarSensores() {

  sensores.begin();

  totalSensores = sensores.getDeviceCount();

  // Zera estatísticas dos sensores
  for (uint8_t i= 0; i < totalSensores; i++) {
    stats[i].leituras_ok = 0;
    stats[i].leituras_erro = 0; 
    stats[i].ultima_temp = 0;
    stats[i].ultimo_erro = false;  
  }

}

// Retorna quantos sensores foram detectados
uint8_t getQuantidadeSensores() {
  return totalSensores;
}


// Retorna o ID físico (ROM) do sensor pelo índice
String getSensorID(uint8_t index) {

  DeviceAddress addr;

  if (!sensores.getAddress(addr, index)) {
    return "";
  }

  char buffer[32];

  sprintf(
    buffer,
    "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
    addr[0], addr[1], addr[2], addr[3],
    addr[4], addr[5], addr[6], addr[7]
  );

  return String(buffer);
}


// Função para ler temperaturas
void atualizarSensores(){

  sensores.requestTemperatures();

  for (uint8_t i = 0; i < totalSensores; i++){

    // Primeira tentativa
    float temp = sensores.getTempCByIndex(i);

    // Segunda tentativa
    if (temp == DEVICE_DISCONNECTED_C){
      delay(10);
      
      sensores.requestTemperatures();

      temp = sensores.getTempCByIndex(i);      
    }

    // Registra estatisticas em caso de erro
    if (temp == DEVICE_DISCONNECTED_C){
      stats[i].leituras_erro++;
      stats[i].ultimo_erro = true;
      continue;   //próximo leitor do loop
    }

    // Registra estatísticas da leitura bem sucedida
    stats[i].leituras_ok++;
    stats[i].ultimo_erro = false;
    stats[i].ultima_temp = temp;
    ultimaTemperatura[i] = temp;
  }

}

// Função de Leitura, sem acessar hardware
float getUltimaTemperatura(uint8_t index){
  return ultimaTemperatura[index];
}

// Função que retorna as estatísticas de um sensor
SensorStats getSensorStats(uint8_t index) {
    return stats[index];
}


