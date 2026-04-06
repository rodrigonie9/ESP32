#include <Arduino.h>          // Necessário para ESP.getEfuseMac()
#include "device_config.h"
#include <Preferences.h>
#include <time.h>

// Objeto responsável por acessar a NVS (flash interna)
static Preferences prefsDevice;

// Variável em RAM que guarda o nome da placa
static String nomePlaca;

// Timestamps de sistema — ficam na RAM, bootTime é definido após NTP sincronizar
static time_t bootTime             = 0;  // momento em que a placa iniciou
static time_t ultimoRebootProgramado = 0; // último reboot programado (carregado da NVS no boot)

// ================= FUNÇÕES INTERNAS ===================

// Gera um nome único usando o MAC da ESP32
String gerarNomePadrao() {

    uint64_t chipid = ESP.getEfuseMac(); 
    // Obtém o MAC único gravado no chip (não muda nunca)

    char nome[32];
    // Buffer para montar o nome

    // Cria um nome baseado nos últimos bytes do MAC
    snprintf(
        nome,
        sizeof(nome),                   
        "esp32-%02X%02X%02X",
        (uint8_t)(chipid >> 16),
        (uint8_t)(chipid >> 8),
        (uint8_t)(chipid)
    );

    return String(nome);
    // Retorna o nome pronto
}

// ================= API PÚBLICA ===================

void iniciarDeviceConfig() {

    // Abre a gaveta "device" da NVS
    prefsDevice.begin(DEVICE_NVS_NAMESPACE, false);
    // false = leitura e escrita

    // Lê o nome da placa salvo
    nomePlaca = prefsDevice.getString(DEVICE_KEY_NOME_PLACA, "");
    // Se não existir, retorna string vazia

    // Se não existir nome salvo ainda
    if (nomePlaca.length() == 0) {

        // Gera nome automático único
        nomePlaca = gerarNomePadrao();

        // Salva na NVS usando a chave padronizada
        prefsDevice.putString(DEVICE_KEY_NOME_PLACA, nomePlaca);
    }

    // Carrega o último reboot programado salvo na NVS
    // getLong retorna 0 se a chave não existir — nunca reiniciou programado ainda
    ultimoRebootProgramado = (time_t)prefsDevice.getLong(DEVICE_KEY_ULTIMO_REBOOT, 0);

    // Fecha a NVS
    prefsDevice.end();
}

// Retorna o nome atual da placa
String getNomePlaca() {
    return nomePlaca;
}

// Atualiza o nome da placa (ex: via Web Portal)
void setNomePlaca(const String& novoNome) {

    // Validação mínima
    // && E> verdadeiro quando ambas condições são verdadeiras
    // || OU> verdadeiro quando pelo menos uma condição é verdadeira
    if (novoNome.length() < 3 || novoNome.length() > 32) return;

    // Abre NVS
    prefsDevice.begin(DEVICE_NVS_NAMESPACE, false);

    // Salva novo nome
    prefsDevice.putString(DEVICE_KEY_NOME_PLACA, novoNome);

    // Fecha NVS
    prefsDevice.end();

    // Atualiza variável em RAM
    nomePlaca = novoNome;
}

// ── SYSINFO ───────────────────────────────────────────────────────────────────

void sysinfo_setBootTime(time_t t) {
    bootTime = t;  // chamado uma vez no setup(), após NTP sincronizar
}

time_t sysinfo_getBootTime() {
    return bootTime;
}

void sysinfo_salvarRebootProgramado(time_t t) {
    ultimoRebootProgramado = t;
    prefsDevice.begin(DEVICE_NVS_NAMESPACE, false);
    prefsDevice.putLong(DEVICE_KEY_ULTIMO_REBOOT, (long)t);
    prefsDevice.end();
}

time_t sysinfo_getUltimoRebootProgramado() {
    return ultimoRebootProgramado;
}
