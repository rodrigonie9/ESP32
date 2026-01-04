#include "sensors.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// ================= CONFIGURAÇÃO =================
#define PINO_DATA 4

static OneWire oneWire(PINO_DATA);
static DallasTemperature sensores(&oneWire);

// Armazena quantos sensores existem
static uint8_t totalSensores = 0;

// ================= IMPLEMENTAÇÃO =================

void iniciarSensores() {

  sensores.begin();

  totalSensores = sensores.getDeviceCount();
}

uint8_t getQuantidadeSensores() {
  return totalSensores;
}

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

float getTemperatura(uint8_t index) {

  sensores.requestTemperatures();

  float temp = sensores.getTempCByIndex(index);

  return temp;
}
