#include "sensor_registry.h"
#include "../debug.h"

#include <Preferences.h>            // para acessar nvs
#include <ArduinoJson.h>            // biblioteca json, transformar lista de sensores em texto (json), salvar na memória

#define NVS_NAMESPACE_SENSORS "sensors_reg"     //espaço na nvs para salvar dados
#define NVS_KEY_CONFIG "config_json"

static Preferences prefs;                       // prefs para acessar a nvs
 // lista em RAM, esp32 iga, lê na nvs e joga para aqui, para ter acesso rápido
static std::vector<SensorConfig> sensoresCadastrados;      


