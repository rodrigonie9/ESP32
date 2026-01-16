#pragma once

#include <Arduino.h>
#include <vector>

// ================= CONFIGURAÇÃO =================


// Estrutura do cadastro do sensor
struct SensorConfig{
    String id_fisico;           // ID físico do DS18B20
    String nome_amigavel;       // nome definifo pelo usuário
    float temp_max_alerta;      // Limite para o alerta
    bool ativo;                 // Se o monitoramento está ligado para esse sensor
};

// ================= FUNÇÕES =================
void iniciarSensorRegistry();

bool salvarSensor(const SensorConfig& config);

bool removerSensor(const String& id_fisico);

String getNomeAmigavel(const String& id_fisico);

bool buscarSensorPorID(const String& id_fisico, SensorConfig& config);

std::vector<SensorConfig> getSensoresCadastrados();

