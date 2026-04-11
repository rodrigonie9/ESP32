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
  uint8_t barramento;         // 0 para o PINO_A, 1 para o PINO_B
  uint8_t indice;             // Posição no boot — mantido para referência, não usado na leitura
  DeviceAddress addr;         // Endereço ROM de 8 bytes — gravado de fábrica, único e imutável
                              // Usado em getTempC(addr) para ler EXATAMENTE este sensor,
                              // independente de quantos outros estão no barramento agora.
                              // getTempCByIndex(indice) usava a posição ATUAL — quando um sensor
                              // desconecta, os demais são re-enumerados e os índices trocam de dono.
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
        mapa[totalDetectados].id         = String(buf);
        mapa[totalDetectados].barramento = barramentoID;
        mapa[totalDetectados].indice     = i;
        memcpy(mapa[totalDetectados].addr, addr, sizeof(DeviceAddress));
        // memcpy copia os 8 bytes do endereço ROM para dentro do mapa
        // sem isso, addr sai do escopo ao fim do loop e o dado se perde
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
    stats[i] = {0, 0, SENSOR_ERRO, false,0};    //ultimo 0 = timpestamp zerado(nunca leu)
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

    // 1º tentativa — lê pelo endereço ROM fixo, não pela posição atual no barramento
    // getTempC(addr) garante que lemos ESTE sensor específico, mesmo que outros tenham
    // desconectado e o barramento tenha re-enumerado os índices
    temp = bus.getTempC(mapa[i].addr);

    // 2º tentativa em caso de erro
    if (temp == DEVICE_DISCONNECTED_C) {
      yield();   // pequena pausa sem usar delay()
      bus.requestTemperatures();
      temp = bus.getTempC(mapa[i].addr);
    }

    if (temp == DEVICE_DISCONNECTED_C) {
      stats[i].leituras_erro++;
      stats[i].ultimo_erro = true;
      ultimaTemperatura[i] = SENSOR_ERRO;  // reseta para que hw_getTemp() reflita o erro
                                            // sem isso, retornaria a última leitura boa
    } else {
      stats[i].leituras_ok++;
      stats[i].ultimo_erro = false;
      stats[i].ultima_temp = temp;
      ultimaTemperatura[i] = temp;
      stats[i].ultimo_timestamp = time(nullptr);      //grava a hora atual
    }
  }
}

// --- FUNÇÕES DE CONSULTA (Mantêm a mesma lógica) ---

uint8_t hw_getContagem() { 
  return totalDetectados; 
}

static const String _idVazio = ""; // String permanente para retorno seguro de referência

const String& hw_getID (uint8_t indice) {
  if (indice < totalDetectados) {
    return mapa[indice].id;
  } else {
    return _idVazio; // referência para variável permanente — nunca aponta para lugar inválido
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



