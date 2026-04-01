#pragma once
#include <Arduino.h>
#include "sensors/sensor_registry.h"

// Função que será chamada no loop principal (main.cpp)
void processarLogicaAlertas();

// Função para colocar um sensor em modo manutenção (mudo) por X horas
void alert_setModoManutencao(const String& id_fisico, uint8_t horas);

// Verifica se o sensor está dentro do horário de monitoramento (agenda)
// Retorna true = pode monitorar | false = fora da agenda
// Exposta aqui para uso no logger — lógica fica centralizada no alert_manager
//
bool estaNoHorarioDeMonitoramento(const SensorConfig& s);