#pragma once
#include <Arduino.h>

// ====================== NVS ======================

// Nome da gaveta (namespace) na NVS
#define DEVICE_NVS_NAMESPACE "device"

// Chave onde o nome da placa é salvo
#define DEVICE_KEY_NOME_PLACA "nome_placa"

// ====================== API ======================

// Inicializa config da placa (chamar no setup)
void iniciarDeviceConfig();

// Retorna o nome atual da placa
String getNomePlaca();

// Atualiza o nome da placa
void setNomePlaca(const String& novoNome);
