#pragma once
#include <Arduino.h>

// ================= ESTATÍSTICAS DOS SENSORES =================
//Estrutura para monitorar a sáude dos sensores
struct SensorStats{
    uint32_t leiturasOK;      //Número de leituras bem sucedidas
    uint32_t erros;           //Número de leituras com falha
    unsigned long ultimoErroMs; //millis() da última falha
};
//Retorna as estatísticas de um sensor
SensorStats getSensorStats(uint8_t index);


// Inicializa o barramento 1-Wire e detecta sensores
void iniciarSensores();

// Retorna quantos sensores foram detectados
uint8_t getQuantidadeSensores();

// Retorna o ID físico (ROM) do sensor pelo índice
String getSensorID(uint8_t index);

// Retorna a temperatura de um sensor pelo índice
float getTemperatura(uint8_t index);
