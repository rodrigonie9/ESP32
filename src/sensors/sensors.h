// USO:
// * Iniciar sensores
// * Atualizar sensores
// * Busca sensor por ID
// * Busca sensor por Indice
// * Busca temperatura

#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>


// Valor padrão para indicar erro de leitura (já definido pela biblioteca Dallas)
#define SENSOR_ERRO DEVICE_DISCONNECTED_C



// ================= ESTATÍSTICAS DOS SENSORES =================
struct SensorStats{
    uint32_t leituras_ok;      //Sucessos
    uint32_t leituras_erro;    //Falhas
    float ultima_temp;         //última temperatura válida
    bool ultimo_erro;          // se a última temperatua falhou
};

// ================= FUNÇÕES DE HARDWARE (O que mexe no fio) =================

void iniciarSensores();         // Inicializa o barramento 1-Wire e detecta sensores
void atualizarSensores();       // Faz a leitura física de todos os sensores

// ================= FUNÇÕES DE INFORMAÇÃO (O que consulta a RAM) =================
uint8_t getQuantidadeSensoresDetectados();                  // Quantos senores respondem no fio agora
String getSensorIDPorIndice(uint8_t index);                 // Pega o ID (ROM) pela posição
uint8_t getSensorIndicePorId(const String& id_fisico);      // Pega posição pelo ID



// ================= FUNÇÕES DE LEITURA (O que o resto do código vai usar) =================
float getTemperaturaSensorPorID(const String& id_fisico);
SensorStats getSensoresStatsPorId(const String& id_fisico);
