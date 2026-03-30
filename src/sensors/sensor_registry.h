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
//Estrutura do cadastro do sensor
// Ficha de cada sensor
struct SensorConfig {
    String id_fisico;               //  ID Fisico sensor DS18B20
    String nome_amigavel;           // nome definido pelo usuário
    float temp_max_alerta;          // Limite para o alerta
    uint16_t tempo_espera_min;      // Tempo para igonorar temperatura alta
    float temp_critica;             // Limite grave, avisa imediatamente
    bool monitoramento_ativo;       // Monitoramento ligado / desligado
    unsigned long mudo_ate;         // Timestamp para modo manutenção (0 = normal)
    uint8_t horas_mudo_padrao;      // Duração botão de silencio (1h, 2h,....24h)

// ── AGENDA DE MONITORAMENTO// ──────────────────────────────────────────
// Bitmask dos dias em que o monitoramento está ativo.
// Cada bit representa um dia da semana:
//   bit 0 = Domingo, bit 1 = Segunda, ..., bit 6 = Sábado     
// Exemplo: só dias úteis → 0b0111110 = 62
// Exemplo: todos os dias → 0b1111111 = 127
    uint8_t dias_monitoramento;

//Se false: nos dias ativos monitora 24h
//Se true: só monitora janela hora_inicio > hora_fim
    bool agenda_horario_ativo;

// Hora e minuto em que monitoramento LIGA(0-23 / 0 ou 30)
// só aceita em 30 em 30 minutos
    uint8_t hora_inicio;
    uint8_t min_inicio;

// Hora e minuto em que o monitoramento DESLIGA(0-23 / 0 ou 30)
// Ex.: inicio-22, fim=06, monitora das 22h até as 06h do dia seguinte
    uint8_t hora_fim;
    uint8_t min_fim;

}; // fecha struct sensor config

// ================= FUNÇÕES =================
void registry_iniciar();
bool registry_salvar(const SensorConfig& config);
bool registry_remover(const String& id_fisico);
const String& registry_getNome(const String& id_fisico);
bool registry_buscarPorID(const String& id_fisico, SensorConfig& config);
std::vector<SensorConfig> registry_getTodos();



