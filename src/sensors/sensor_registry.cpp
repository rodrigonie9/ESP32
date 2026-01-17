#include "sensor_registry.h"
#include "../debug.h"

#include <Preferences.h>            // para acessar nvs
#include <ArduinoJson.h>            // biblioteca json, transformar lista de sensores em texto (json), salvar na memória

#define NVS_NAMESPACE_SENSORS "sensors_reg"     //espaço na nvs para salvar dados
#define NVS_KEY_CONFIG "config_json"

static Preferences prefs;                       // prefs para acessar a nvs
 // lista em RAM, esp32 iga, lê na nvs e joga para aqui, para ter acesso rápido
static std::vector<SensorConfig> sensoresCadastrados;      


// salva todos os dados do sensores em uma lista JSON
// essa lista é salva em apenas uma NVS, usando menos espaço
static void gravarSensoresNvs(){
    JsonDocument doc;                       //cria documento Json em Branco
    JsonArray array = doc.to<JsonArray>();  //transforma doc em um lista []

    //para cada sensor na nossa lista[], criamos um objeto{} no JSON
    for (const auto& s : sensoresCadastrados) {
        JsonObject obj = array.add<JsonObject>();
        obj["id"] = s.id_fisico;
        obj["nome"] = s.nome_amigavel;
        obj["max"] = s.temp_max_alerta;
        obj["espera"] = s.tempo_espera_min;
        obj["critica"] = s.temp_critica;
        obj["ativo"] = s.monitoramento_ativo;
    }

    String json;
    serializeJson(doc,json);    //transformar o objeto json em uma string de texto

    prefs.begin(NVS_NAMESPACE_SENSORS, false);      //abre gaveta para escrever
    prefs.putString(NVS_KEY_CONFIG, json);          // salva texto JSON na chave "config_json"
    prefs.end();                                      // fecha a gaveta   

    LOG("Configuração de sensores salva no NVS");
}

//  salvar dados sensores da NVS para ram SENSORESCADASTRADOS > NVS
void iniciarSensorRegistry(){
    prefs.begin(NVS_NAMESPACE_SENSORS, true);               // abre gaveta para escrever
    String json = prefs.getString(NVS_KEY_CONFIG,"[]");     // lê o testo, se não existir traz lista vazia "[]"
    prefs.end();

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc,json); // Transforma o texto de volta em objeto JSON
                                                            // verifica erro

    sensoresCadastrados.clear();            //limpa a lista atual na RAM

    if(!error) {
        JsonArray array = doc.as<JsonArray>();
        for (JsonObject obj : array){
            SensorConfig s;
            s.mudo_ate = 0;
            s.id_fisico = obj["id"].as<String>();
            s.nome_amigavel = obj["nome"].as<String>();
            s.temp_max_alerta = obj["max"].as<float>();
            s.tempo_espera_min = obj["espera"].as<uint16_t>();
            s.temp_critica = obj["critica"].as<float>();
            s.monitoramento_ativo = obj["ativo"].as<bool>();
            sensoresCadastrados.push_back(s);       //adiciona na lista RAM
        }
        LOG("Registro de sensores carregados: " + String(sensoresCadastrados.size()) + " sensores.");
    }

}

//retornar lista completa para quem precisar ex: webportal
std::vector<SensorConfig> getSensoresCadastrados(){
    return sensoresCadastrados;
}

//adiciona um novo sensor, ou atualiza um que já existe
// auto = descubra sozinho que tipo de variável é essa
    //percorre lista, procrando se ID físico já existe
    //se existir atualiza o nome e o limite
    //se for novo empurra (push_back), para final da lista
    //no fim chama gravarSensoresNVS, atualizar na NVS
bool salvaSensor(const SensorConfig& config) {

    bool encontrado = false;
    for (auto& s : sensoresCadastrados) {
        if (s.id_fisico == config.id_fisico) {
            s = config;             //atauliza os dados se o sensor ID já exisitir
            encontrado = true; 
            break;
        }
    }

    if (!encontrado){
        sensoresCadastrados.push_back(config);      // adiciona novo se não existir
    }

    gravarSensoresNvs();    //salva a mudança na memória flash (NVS)
}

// remover um sesor na lista pelo ID
    //usa iterador it, percorrer lista e apagar o sensore correto
bool removerSensor(const String& id_fisico) {
    for (auto it = sensoresCadastrados.begin(); it != sensoresCadastrados.end(); ++it) {
        if (it->id_fisico == id_fisico) {
            sensoresCadastrados.erase(it);          // remove da ram
            gravarSensoresNvs();                    // atauliza a flash
            return true;
        }
        return false;
    }
}

// Busca o nome amigável, se não encontrar retorna o ID próprio
    // retorna nome amigável, muito usado pelo telegram
String getNomeAmigavel(const String& id_fisico) {
    for (const auto& s : sensoresCadastrados) {
        if (s.id_fisico == id_fisico){
            return s.nome_amigavel;
        }
    return id_fisico;
    }
}

// Busca a configuração completa de um sensor específico
bool buscarSensorPorId(const String& id_fisico, SensorConfig& config) {
    for (const auto& s : sensoresCadastrados){
        if (s.id_fisico == id_fisico) {
            config = s;
            return true;
        }
    }
    return false;
}


