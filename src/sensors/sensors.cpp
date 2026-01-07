#include "sensors.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// ================= CONFIGURAÇÃO =================
#define PINO_DATA 18  // GPIO onde os sensores estão ligados

static SensorStats stats[20]; // Suporta até 20 sensores

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
    stats[i].leiturasOK = 0;
    stats[i].erros = 0; 
    stats[i].ultimoErroMs = 0;  
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


// Retorna a temperatura de um sensor pelo índice
float getTemperatura(uint8_t index) {

  //Primeira tentativa
  float temp = sensores.getTempCByIndex(index);

  // Se falhou, tenta uma segunda vez
  if (temp == DEVICE_DISCONNECTED_C) {
    stats[index].erros++; 
    stats[index].ultimoErroMs = millis();

    // pequeno atraso para estabilizar o barramento
    delay(50);

    //solicita nova leitura
    sensores.requestTemperatures();

    //segunda tentativa
    temp = sensores.getTempCByIndex(index);

    //Se falhou de novo, retorna erro
    if (temp == DEVICE_DISCONNECTED_C) {
      return temp;
    }
  }

  // leitura válida
  stats[index].leiturasOK++;
  return temp;
}

// Função que retorna as estatísticas de um sensor
SensorStats getSensorStats(uint8_t index) {
    return stats[index];
}


