// logger.h DEVE vir primeiro — ele define LOGGER_ATIVO
// Só depois o #ifdef consegue verificar se está ativo
#include "logger.h"

// Todo o resto só é compilado se LOGGER_ATIVO estiver definido no logger.h
#ifdef LOGGER_ATIVO
  #include "../sensors/sensors.h"
  #include "../sensors/sensor_registry.h"
  #include "../logic/alert_manager.h"
  #include "../device/device_config.h"
  #include "../wifi_config/wifi_config.h"
  #include "../debug.h"

  #include <HTTPClient.h>   // para enviar dados pela internet
  #include <ArduinoJson.h>  // para montar o JSON que o Google espera
  #include <time.h>         // para pegar data e hora atual

  // URL gerada ao implantar o script no Google Sheets
  // Se reimplantar o script, atualizar esta URL
  const char* LOGGER_URL =
      "https://script.google.com/macros/s/AKfycbz51RHZAz4FbW7HTDjZDujaOIA-h7FaGWZhvvLsQmk25-f20XVJM5CjuN8NjEA-P26V/exec";


  // ── FUNÇÃO AUXILIAR: STATUS DO ALERTA ────────────────────────────
  // static = só existe dentro deste arquivo, não vaza para o resto do projeto
  // Compara a temperatura com os limites configurados no sensor
  // Retorna um texto fixo: "ERRO", "CRITICO", "ALERTA" ou "OK"
  static const char* getStatusAlerta(float temp, const SensorConfig& s) {
      if (temp == SENSOR_ERRO)        return "ERRO";
      if (temp >= s.temp_critica)     return "CRITICO";
      if (temp >= s.temp_max_alerta)  return "ALERTA";
      return "OK";
  }


  // ── FUNÇÃO AUXILIAR: STATUS DO MONITORAMENTO ─────────────────────
  // Verifica se o sensor está em manutenção ou fora da agenda
  // Retorna "MANUT", "ON" ou "OFF"
  static const char* getStatusMonitoramento(const SensorConfig& s) {
      // Verifica modo manutenção — mesma lógica do web portal
      // compara Unix timestamp — se agora < mudo_ate, ainda está em manutenção
      time_t agora; time(&agora);
      if (s.mudo_ate > 0 && agora < s.mudo_ate) {
          return "MANUT";
      }
      // Usa a função do alert_manager — lógica centralizada num só lugar
      return estaNoHorarioDeMonitoramento(s) ? "ON" : "OFF";
  }


  // ── INICIALIZAÇÃO ─────────────────────────────────────────────────
  void logger_iniciar() {
      LOG("Logger: modulo iniciado.");
  }


  // ── FUNÇÃO PRINCIPAL: REGISTRA UMA LEITURA ───────────────────────
  // Chamada no main.cpp após hw_lerTodos()
  // Monta um JSON com todos os sensores e envia ao Google Sheets
  void logger_registrarLeitura() {

      // Não tenta enviar se não tiver internet
      if (!internetDisponivel()) {
          LOG("Logger: sem internet, pulando envio.");
          return;
      }

      // ── Pega data e hora atual ──
      // bufData = "31/03/2026" | bufHora = "14:32:05"
      // Valores padrão caso NTP ainda não tenha sincronizado
      char bufData[12] = "sem data";
      char bufHora[10] = "sem hora";
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
          strftime(bufData, sizeof(bufData), "%d/%m/%Y", &timeinfo);
          strftime(bufHora, sizeof(bufHora), "%H:%M:%S", &timeinfo);
      }

      // ── Monta o JSON ──
      // JsonDocument é a classe do ArduinoJson v7
      // Diferente da v6, não precisa definir tamanho — cresce automaticamente
      // O JSON final ficará assim:
      // {
      //   "data": "31/03/2026",
      //   "hora": "14:32:05",
      //   "placa": "Câmara Fria",
      //   "sensores": [
      //     { "nome": "Câmara 1", "temp": 4.2, "monitoramento": "ON", "alerta": "OK" },
      //     { "nome": "Câmara 2", "temp": 9.5, "monitoramento": "ON", "alerta": "ALERTA" }
      //   ]
      // }
      JsonDocument doc;
      doc["data"]  = bufData;
      doc["hora"]  = bufHora;
      doc["placa"] = getNomePlaca();

      // to<JsonArray>() transforma o campo "sensores" num array vazio
      // Cada sensor será um objeto dentro desse array
      JsonArray arraySensores = doc["sensores"].to<JsonArray>();

      auto cadastrados = registry_getTodos();
      for (const auto& s : cadastrados) {
          float temp = hw_getTemp(s.id_fisico);

          // add<JsonObject>() adiciona um novo objeto no array
          // Cada objeto vira uma linha na planilha do Google
          JsonObject obj = arraySensores.add<JsonObject>();
          obj["nome"]          = s.nome_amigavel;
          obj["temp"]          = (temp == SENSOR_ERRO) ? -999.0 : temp;
          // -999 na planilha = sensor offline (fácil de filtrar no Sheets)
          obj["monitoramento"] = getStatusMonitoramento(s);
          obj["alerta"]        = getStatusAlerta(temp, s);
      }

      // ── Converte o JSON para texto ──
      // serializeJson transforma o objeto doc numa String
      // Ex: {"data":"31/03/2026","hora":"14:32:05",...}
      String jsonStr;
      serializeJson(doc, jsonStr);

      // ── Envia via HTTP POST ──
      HTTPClient http;

      // Google Scripts redireciona (302) igual ao GitHub — precisa seguir
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      http.setTimeout(10000); // 10 segundos de timeout

      http.begin(LOGGER_URL);
      http.addHeader("Content-Type", "application/json");

      // POST envia os dados — diferente do GET que só busca
      // Retorna o código HTTP da resposta (200 = OK, -1 = erro de conexão)
      int httpCode = http.POST(jsonStr);

      // 200 = sucesso direto | 302 = redirect do Google (dado já foi recebido antes do redirect)
      if (httpCode == HTTP_CODE_OK || httpCode == 302) {
          LOG("Logger: enviado ao Google Sheets com sucesso.");
      } else {
          LOG("Logger: falha no envio. Codigo HTTP: " + String(httpCode));
      }

      // Sempre liberar o HTTPClient após usar — evita vazamento de memória
      http.end();
  }

  #endif