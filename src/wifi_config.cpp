//implementa as funções, como funciona

#include <WiFiManager.h>      // Biblioteca WiFiManager
#include <HTTPClient.h>  

#include "wifi_config.h"     // Header deste módulo

// Função responsável por conectar o ESP32 ao WiFi
void conectarWiFi() {

  // Tenta conectar no WiFi salvo
  // Se não existir WiFi salvo, cria um Access Point

  // na primeira vez cria o access point, usuário conecta wifi pelo acces point
  // esp32 deixa salvo informações de rede salvo na placa
  // para resetar (perder informçõaes de rede)
  // 1 - botão boot
  // 2 - wm.resetSettings();   // Apaga Wi-Fi salvo

  WiFiManager wm;             // Cria o objeto WiFiManager
  //wm.resetSettings();       // reseta configução de internet()senhas salvas
  bool conectado = wm.autoConnect("ESP32_Config","12345678");

  // Verifica se conseguiu conectar
  if (!conectado) {
    // Caso falhe até no portal de configuração
    ESP.restart();            // Reinicia o ESP32
  }
}

// verifica se existe acesso real a internet
  bool temInternet(){
    if(WiFi.status() != WL_CONNECTED){
      // se não está conectado ao Wi-Fi não há internet
      return false;
    }

    HTTPClient http;
    // cria cliente http

    http.setTimeout(3000);
    // define timeout para evitar travamento

    http.begin("http://www.google.com/generate_204");
    // Endpoint usado para teste rápido de conectividade

    int codigo = http.GET();
    // executa requisição de http

    http.end();
    // libera recursos

    return (codigo>0);
    // http.GET() > retorna numero positivo se conseguiu acesso a internet , negativo em caso de erro (time out , dns falho, sem internet)
    // se recebeu resposta, considera que há internet
  }

  // Tenta recuperar conexão com a internet
  bool tentarRecuperarInternet(int tentativas){
    
    for (int i = 0;i < tentativas; i ++){

      WiFi.disconnect();
      // desconecta wifi atual

      delay(1000);
      // aguarda estabilizar

      WiFi.reconnect();
      // solicita reconexão ao roteador

      delay(3000);
      // aguarda tentativa de conexão

      if (temInternet()) {
        // se coneseguiu internet, retornar sucesso
        return true;
      }
    }

    //  se chegou aqui, não conseguiu recuperar
    return false;

  }

// função unica que deve ser usada antes de qualquer acesso à internet
bool internetDisponivel(){

  //primeiro teste direto
  if (temInternet()) {
    return true;
  }

  // se não tiver internet, tenta recuperar 3 vezes
  if(tentarRecuperarInternet(3)){
    return true;
  }

  //se ainda não tiver internet, aceita que está offline
  return false;
} 