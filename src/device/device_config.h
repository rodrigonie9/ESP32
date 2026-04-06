#pragma once
#include <Arduino.h>

// ====================== NVS ======================

// Nome da gaveta (namespace) na NVS
#define DEVICE_NVS_NAMESPACE "device"

// Chave onde o nome da placa é salvo
#define DEVICE_KEY_NOME_PLACA    "nome_placa"

// Chave onde o timestamp do último reboot programado é salvo
#define DEVICE_KEY_ULTIMO_REBOOT "ult_reboot"

// ====================== API ======================

// Inicializa config da placa (chamar no setup)
void iniciarDeviceConfig();

// Retorna o nome atual da placa
String getNomePlaca();

// Atualiza o nome da placa
void setNomePlaca(const String& novoNome);

// Registra o momento em que a placa iniciou (chamar após NTP sincronizar)
void sysinfo_setBootTime(time_t t);

// Retorna hora do boot atual (0 = NTP não sincronizou ainda)
time_t sysinfo_getBootTime();

// Salva na NVS o timestamp do último reboot programado (chamar antes do ESP.restart)
void sysinfo_salvarRebootProgramado(time_t t);

// Retorna timestamp do último reboot programado (0 = nunca ocorreu)
time_t sysinfo_getUltimoRebootProgramado();
