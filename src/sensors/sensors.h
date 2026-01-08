#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>


// Valor padrão para indicar erro de leitura
#define SENSOR_ERRO DEVICE_DISCONNECTED_C



// ================= ESTATÍSTICAS DOS SENSORES =================
//Estrutura para monitorar a sáude dos sensores
struct SensorStats{
    uint32_t leituras_ok;      //Número de leituras bem sucedidas
    uint32_t leituras_erro;       //Número de leituras com falha
    float ultima_temp;         //última temperatura válida
    bool ultimo_erro;          // últime leitura foi erro
};
//Retorna as estatísticas de um sensor
SensorStats getSensorStats(uint8_t index);


// Inicializa o barramento 1-Wire e detecta sensores
void iniciarSensores();

// Pegar temperatura dos sensores
void atualizarSensores();

// Retorna quantos sensores foram detectados
uint8_t getQuantidadeSensores();

// Retorna o ID físico (ROM) do sensor pelo índice
String getSensorID(uint8_t index);


// Funções de Leitura (sem acessar hardware)
float getUltimaTemperatura(uint8_t index);
SensorStats getSensorStats(uint8_t index);
