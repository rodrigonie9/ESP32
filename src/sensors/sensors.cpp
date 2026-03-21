//HARDWARE / fio

#include "sensors.h"
#include "../debug.h"

#include "../config/system_limits.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// Definição dos dois pinos de dados das sua PCB
#define PINO_A 18
#define PINO_B 21

// Criamos dois barramentos independentes
static OneWire oneWireA(PINO_A);
static DallasTemperature barramentoA(&oneWireA);

static OneWire oneWireB(PINO_B);
static DallasTemperature barramentoB(&oneWireB);

// Estrutura para saber onde cada sensor está "espetado"
struct SensorLocalizacao {
  String id;
  uint8_t barramento;     // 0 para o PINO_A, 1 para o PINO_B
  uint8_t indice;         // Posição(0,1,2....) dentro daquele pino 
};

static SensorLocalizacao mapa[MAX_SENSORES];    // tabela de tradução
static uint8_t totalDetectados = 0;
static float ultimaTemperatura[MAX_SENSORES];
static SensorStats stats[MAX_SENSORES];

// ======================= FUNÇÕES AUXILIARES =======================
// dentro da função iniciarSensores, temos a função escanearBarramento
// escaneamor barramentoA e barramentoB dentro da função

void hw_iniciar(){
  barramentoA.begin();
  barramentoB.begin();

  totalDetectados = 0;

  //Função auxiliar interna para escanear um barramento (Lambda, função dentro da função)
  auto escanear = [](DallasTemperature& bus, uint8_t barramentoID){
    uint8_t count = bus.getDeviceCount();

    for(uint8_t i = 0; i < count && totalDetectados < MAX_SENSORES; i++) {   //totalDetectados - MAXSENSORS, para não levar o loop sempre até 30
      DeviceAddress addr;
      if (bus.getAddress(addr,i)) {
        char buf[25];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", 
                         addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
        mapa[totalDetectados] = {String(buf), barramentoID, i};
        totalDetectados ++; 
      }
    }
  //final escanearBarramento
  };
  
  // Executa o escaneamento nos dois pinos
  escanear(barramentoA, 0);
  escanear(barramentoB, 1);

  //Limpa estatísticas usando o limite centralizado
  for (uint8_t i = 0; i < MAX_SENSORES; i++) {
    stats[i] = {0, 0, SENSOR_ERRO, false};
    ultimaTemperatura[i] = SENSOR_ERRO;
  }

  LOG("Busca finalizada, Total de sensores encontrados: " + String(totalDetectados));
//final void iniciar sensores
}

// referência dinâmica BUS, aponta para o barramento correto
void hw_lerTodos(){
  barramentoA.requestTemperatures();
  barramentoB.requestTemperatures();

  for (uint8_t i = 0; i < totalDetectados; i++){
    float temp;
    // cria referência chamada BUS: que vai apontar para barramentoA ou barramentoB
    //   dependendo do valor de mapa[i].barramento
    //   (mapa[i].barramento == 0) ? barramentoA : barramentoB;
    //   condicao ? valor_se_true : valor_se_false
    DallasTemperature& bus = (mapa[i].barramento == 0) ? barramentoA : barramentoB;

    // 1º tentativa
    temp = bus.getTempCByIndex(mapa[i].indice);

    // 2º tentatica em caso de erro
    if (temp == DEVICE_DISCONNECTED_C) {
      delay(10);
      bus.requestTemperatures();
      temp = bus.getTempCByIndex(mapa[i].indice);
    }

    if (temp == DEVICE_DISCONNECTED_C) {
      stats[i].leituras_erro++;
      stats[i].ultimo_erro = true;
    } else {
      stats[i].leituras_ok++;
      stats[i].ultimo_erro = false;
      stats[i].ultima_temp = temp;
      ultimaTemperatura[i] = temp;
    }
  }
}

// --- FUNÇÕES DE CONSULTA (Mantêm a mesma lógica) ---

uint8_t hw_getContagem() { 
  return totalDetectados; 
}

String hw_getID (uint8_t indice) {
  if (indice < totalDetectados) {
    return mapa[indice].id;
  } else {
    return "";
  }
}


float hw_getTemp(const String& id_fisico) {
  for (uint8_t i = 0; i < totalDetectados; i++){
    if (mapa[i].id == id_fisico) return ultimaTemperatura[i];
  }
  return DEVICE_DISCONNECTED_C;
}

SensorStats hw_getStats(const String& id_fisico)  {
    for (uint8_t i = 0; i < totalDetectados; i++) {
      if (mapa[i].id == id_fisico) return stats[i];
    }
    return {0, 0, DEVICE_DISCONNECTED_C, true};
}



