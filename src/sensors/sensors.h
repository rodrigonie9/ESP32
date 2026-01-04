#pragma once
#include <Arduino.h>

// Inicializa o barramento 1-Wire e detecta sensores
void iniciarSensores();

// Retorna quantos sensores foram detectados
uint8_t getQuantidadeSensores();

// Retorna o ID físico (ROM) do sensor pelo índice
String getSensorID(uint8_t index);

// Retorna a temperatura de um sensor pelo índice
float getTemperatura(uint8_t index);
