#pragma once

#include <Arduino.h>
#include <vector>            
// vector usar memória ram da ESP32
// std::vector salva sensorescadastrados na RAM (lista dinâmica)
// usa com frequencia &, para usar referência com a memória RAM
// com &, evitar copiar, deixa RAM mais rápida, e economiza memória RAM
// segredo para a esp32 não travar por falta de memória > sem o $ iria copiar, 
//   com o &, não copia nadam só usa a ficha original que já está na RAM (crachá, endereço de onde esta a informação)
// Uso const& (const junto com &),  const SensorConfig& config
//   permite acessar original(economiza RAM)
//   const garante que a função não vai alterar o original por acidente, ex. dá a ficha para alguém ler,
//      mas em um plástico, para que a pessoa não possa escrever nela


// ================= CONFIGURAÇÃO =================


// Estrutura do cadastro do sensor
struct SensorConfig {
    String id_fisico;           // ID físico do DS18B20
    String nome_amigavel;       // nome definifo pelo usuário
    float temp_max_alerta;      // Limite para o alerta
    uint16_t tempo_espera_min;  // Tempo para ignorar temperatua alta
    float temp_critica;         // Limite grave, se atingir avisa imediatamente
    bool monitoramento_ativo;   // Monitoramento Ligado/Desligado
    unsigned long mudo_ate;     // Timestamp para modo manutenção (0 = nornmal)
};

// ================= FUNÇÕES =================
void iniciarSensorRegistry();

bool salvarSensor(const SensorConfig& config);

bool removerSensor(const String& id_fisico);

String getNomeAmigavel(const String& id_fisico);

bool buscarSensorPorID(const String& id_fisico, SensorConfig& config);

std::vector<SensorConfig> getSensoresCadastrados();

