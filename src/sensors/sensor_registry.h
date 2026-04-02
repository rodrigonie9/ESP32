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

// ── MODOS DE MONITORAMENTO ──────────────────────────────────────────
#define DIA_DESLIGADO 0     // dia desligado - não monitora
#define DIA_24H       1     // monitora dia inteiro
#define DIA_HORARIO   2     // monitora só na janela configurada

// ================= CONFIGURAÇÃO =================
//Estrutura do cadastro do sensor
// Ficha de cada sensor
struct SensorConfig {
    String id_fisico;               //  ID Fisico sensor DS18B20
    String nome_amigavel;           // nome definido pelo usuário
    float temp_max_alerta;              // Limite para o alerta suave
    uint16_t tempo_degelo_min;          // Tempo de paciência antes de avisar alerta suave (cobre ciclos de degelo)
    float temp_critica;                 // Limite grave
    uint16_t tempo_espera_critico_min;  // Tempo de paciência antes de avisar crítico
    bool monitoramento_ativo;       // Monitoramento ligado / desligado
    unsigned long mudo_ate;         // Timestamp para modo manutenção (0 = normal)
    uint8_t horas_mudo_padrao;      // Duração botão de silencio (1h, 2h,....24h)

// ── AGENDA POR DIA// ──────────────────────────────────────────
// Índice: 0=Domingo, 1=Segunda, 2=Terça, 3=Quarta, 4=Quinta, 5=Sexta, 6=Sábado
// Cada dia teu seu prórpio modo de janeça independente
    uint8_t dia_modo[7];                // modo do dia: DIA_DESLIGADO, DIA_24H ou DIA_HORARIO
    uint8_t dia_hora_inicio[7];         // hora que o monitoramento LIGA naquele dia (0-23)         
    uint8_t dia_min_inicio[7];          // minuto que LIGA (0 ou 30)
    uint8_t dia_hora_fim[7];            // hora que o monitoramento DESLIGA naquele dia (0-23)                     
    uint8_t dia_min_fim[7];             // minuto que DESLIGA (0 ou 30)            

}; // fecha struct sensor config

// ================= FUNÇÕES =================
void registry_iniciar();
bool registry_salvar(const SensorConfig& config);
bool registry_remover(const String& id_fisico);
const String& registry_getNome(const String& id_fisico);
bool registry_buscarPorID(const String& id_fisico, SensorConfig& config);
std::vector<SensorConfig> registry_getTodos();



