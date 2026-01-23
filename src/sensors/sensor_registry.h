// BANCO DE DADOS - cadastro  / NVS



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
enum ModoMonitoramento {
    SEMPRE_24H = 0,
    AGENDA_SEMANAL = 1,
    AGENDA_DIARIA = 2
};


// Estrutura do cadastro do sensor
struct SensorConfig {
    String id_fisico;           // ID físico do DS18B20
    String nome_amigavel;       // nome definifo pelo usuário
    float temp_max_alerta;      // Limite para o alerta
    uint16_t tempo_espera_min;  // Tempo para ignorar temperatua alta
    float temp_critica;         // Limite grave, se atingir avisa imediatamente
    bool monitoramento_ativo;   // Monitoramento Ligado/Desligado

    unsigned long mudo_ate;     // Timestamp para modo manutenção (0 = nornmal)
    uint8_t horas_mudo_padrao;  // Usuário define se botão silencia por 1h, 2, 24h

    bool usar_agenda;           // True = segue agenda - falso = 24h por dia
    
    // Modo de Monitoramento
    ModoMonitoramento modo;

    // Para Agenda Semanal (Ex: Sábado 19:00 até Segunda 08:30)
    // DIA SEMANA 0 - 6
    uint8_t dia_desliga_semanal, hora_desliga_semanal, min_desliga_semanal;   
    uint8_t dia_religa_semanal, hora_religa_semanal, min_religa_semanal;


    // Para Agenda Diária (Ex: Todos os dias das 19:00 às 08:00)
    uint8_t hora_desliga_diaria, min_desliga_diaria;
    uint8_t hora_religa_diaria, min_religa_diaria;
};

// ================= FUNÇÕES =================
void registry_iniciar();
bool registry_salvar(const SensorConfig& config);
bool registry_remover(const String& id_fisico);
String registry_getNome(const String& id_fisico);
bool registry_buscarPorID(const String& id_fisico, SensorConfig& config);
std::vector<SensorConfig> registry_getTodos();

