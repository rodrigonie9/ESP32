#pragma once
#include <Arduino.h>

// Função que será chamada no loop principal (main.cpp)
void processarLogicaAlertas();

// Função para colocar um sensor em modo manutenção (mudo) por X horas
void setModoManutencao(const String& id_fisico, uint8_t horas);