// BANCO DE DADOS - cadastro  / NVS

#include "sensor_registry.h"
#include "../debug.h"

#include <Preferences.h>            // para acessar nvs
#include <ArduinoJson.h>            // biblioteca json, transformar lista de sensores em texto (json), salvar na memória

#include "../telegram/telegram.h"   

#define NVS_NAMESPACE_SENSORS "sensors_reg"     //espaço na nvs para salvar dados
#define NVS_KEY_CONFIG "config_json"

static Preferences prefs;                       // prefs para acessar a nvs
 // lista em RAM, esp32 iga, lê na nvs e joga para aqui, para ter acesso rápido
static std::vector<SensorConfig> sensoresCadastrados;  


// salva todos os dados do sensores em uma lista JSON
// essa lista é salva em apenas uma NVS, usando menos espaço

//JSON DE CADA SENSOR FICA ASSIM
//  {
//    "nome_amigavel": "Câmara 1",
//    "temp_max_alerta": 8.0,
//    "dia_modo":  [1, 1, 2, 1, 1, 1, 0],
//    "dia_hi":    [0, 0, 8, 0, 0, 0, 0],
//    "dia_mi":    [0, 0,30, 0, 0, 0, 0],
//    "dia_hf":    [0, 0,19, 0, 0, 0, 0],
//    "dia_mf":    [0, 0, 0, 0, 0, 0, 0]
//  }

static void gravarNoNVS(){
    JsonDocument doc;                       //cria documento Json em Branco
    JsonArray array = doc.to<JsonArray>();  //transforma doc em um lista []

    //para cada sensor na nossa lista[], criamos um objeto{} no JSON
    for (const auto& s : sensoresCadastrados) {
        JsonObject obj = array.add<JsonObject>();
        obj["id_fisico"]            = s.id_fisico;
        obj["nome_amigavel"]        = s.nome_amigavel;
        obj["temp_max_alerta"]      = s.temp_max_alerta;
        obj["tempo_espera_min"]     = s.tempo_espera_min;
        obj["temp_critica"]         = s.temp_critica;
        obj["monitoramento_ativo"]  = s.monitoramento_ativo;
        obj["horas_mudo_padrao"]    = s.horas_mudo_padrao;
        
        // ── AGENDA POR DIA ──
        // Salva os 5 arrays, de 7 posições, como listas JSON
        // Exemplo no JSON: "dia_modo:[1,1,2,1,1,0]"
        JsonArray modos     = obj["dia_modo"].to<JsonArray>();
        JsonArray hInicio   = obj["dia_hi"].to<JsonArray>();
        JsonArray mInicio   = obj["dia_mi"].to<JsonArray>();
        JsonArray hFim      = obj["dia_hf"].to<JsonArray>();
        JsonArray mFim      = obj["dia_mf"].to<JsonArray>();

        for (int i = 0; i < 7; i++) {
            modos.add(s.dia_modo[i]);
            hInicio.add(s.dia_hora_inicio[i]);
            mInicio.add(s.dia_min_inicio[i]);
            hFim.add(s.dia_hora_fim[i]);
            mFim.add(s.dia_min_fim[i]);
        }

    }

    String json;
    serializeJson(doc,json);    //transformar o objeto json em uma string de texto

    prefs.begin(NVS_NAMESPACE_SENSORS, false);      //abre gaveta para escrever
    prefs.putString(NVS_KEY_CONFIG, json);          // salva texto JSON na chave "config_json"
    prefs.end();                                      // fecha a gaveta   

    LOG("Configuração de sensores salva no NVS");
}

//  salvar dados sensores da NVS para ram SENSORESCADASTRADOS > NVS
void registry_iniciar () {
    prefs.begin(NVS_NAMESPACE_SENSORS, true);               // abre gaveta para escrever
    String json = prefs.getString(NVS_KEY_CONFIG,"[]");     // lê o testo, se não existir traz lista vazia "[]"
    prefs.end();

    JsonDocument doc;                                       //ESP32 nao sava array na NVS, só consegue texto
                                                            // por isso converte para JSON

    DeserializationError error = deserializeJson(doc,json); // Transforma o texto de volta em objeto JSON
                                                            // verifica erro

    sensoresCadastrados.clear();            //limpa a lista atual na RAM

    if(!error) {
        JsonArray array = doc.as<JsonArray>();
        for (JsonObject obj : array) {
            SensorConfig s;
            s.id_fisico = obj["id_fisico"].as<String>();
            s.nome_amigavel = obj["nome_amigavel"].as<String>();
            s.temp_max_alerta = obj["temp_max_alerta"].as<float>();
            s.tempo_espera_min = obj["tempo_espera_min"].as<uint16_t>();
            s.temp_critica = obj["temp_critica"].as<float>();
            s.monitoramento_ativo = obj["monitoramento_ativo"].as<bool>();
            s.horas_mudo_padrao = obj["horas_mudo_padrao"].as<uint8_t>();
            
    
            // ── AGENDA POR DIA ──
              // Carrega os arrays — se não existir no NVS, usa padrão seguro (DIA_24H = 1)
              JsonArray modos   = obj["dia_modo"].as<JsonArray>();
              JsonArray hInicio = obj["dia_hi"].as<JsonArray>();
              JsonArray mInicio = obj["dia_mi"].as<JsonArray>();
              JsonArray hFim    = obj["dia_hf"].as<JsonArray>();
              JsonArray mFim    = obj["dia_mf"].as<JsonArray>();

              for (int i = 0; i < 7; i++) {
                  // modos[i] | DIA_24H → se não existir no JSON, usa 24h como padrão
                  // fallback, caso leia sensore antes de ele estar cadastrado
                  // entra como padrão 1, para ler 24h
                  s.dia_modo[i]        = modos[i]   | DIA_24H;
                  s.dia_hora_inicio[i] = hInicio[i] | 0;
                  s.dia_min_inicio[i]  = mInicio[i] | 0;
                  s.dia_hora_fim[i]    = hFim[i]    | 0;
                  s.dia_min_fim[i]     = mFim[i]    | 0;
              }
            

            s.mudo_ate = 0; // Reset do modo mudo ao reiniciar
            sensoresCadastrados.push_back(s);
        } 
        LOG("Registry: " + String(sensoresCadastrados.size()) + " sensores.");

    } else {
        LOG("ERRO: Falha ao carregar sensores do NVS: " + String(error.c_str()));
        enviarMensagemTelegram("ERRO CRÍTICO: Falha ao carregar sensores da memória. Nenhum sensor monitorado\n"
                               "Tente reiniciar a placa. Caso o erro persista entre em contato com o técnico");  
    }
}


//adiciona um novo sensor, ou atualiza um que já existe
// auto = descubra sozinho que tipo de variável é essa
    //percorre lista, procrando se ID físico já existe
    //se existir atualiza o nome e o limite
    //se for novo empurra (push_back), para final da lista
    //no fim chama gravarSensoresNVS, atualizar na NVS
bool registry_salvar (const SensorConfig& config) {
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
    
    gravarNoNVS();    //salva a mudança na memória flash (NVS)
    return true;
}

// remover um sesor na lista pelo ID
    //usa iterador it, percorrer lista e apagar o sensore correto
bool  registry_remover(const String& id_fisico) {
    for (auto it = sensoresCadastrados.begin(); it != sensoresCadastrados.end(); ++it) {
        if (it->id_fisico == id_fisico) {
            sensoresCadastrados.erase(it);          // remove da ram
            gravarNoNVS();                    // atauliza a flash
            return true;
        }
    }
    // Se chegou aqui, não encontrou o sensor
    return false;
}

// Busca o nome amigável, se não encontrar retorna o ID próprio
    // retorna nome amigável, muito usado pelo telegram
const String& registry_getNome(const String& id_fisico) {
    for (const auto& s : sensoresCadastrados) {
        if (s.id_fisico == id_fisico){
            return s.nome_amigavel;
        }
    }
    // Se não encontrou nada retonar o próprio ID
    return id_fisico;
}

// Busca a configuração completa de um sensor específico
// buscar por um ID, quando encontra esse ID, ele copia os daodos deste sensor para o variável chamada na função config
//  config = s
bool registry_buscarPorID (const String& id_fisico, SensorConfig& config) {
    for (const auto& s : sensoresCadastrados){
        if (s.id_fisico == id_fisico) {
            config = s;
            return true;
        }
    }
    return false;
}

std::vector<SensorConfig> registry_getTodos() {
    return sensoresCadastrados;
}


