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

  //aguarda até 90' para roteador subir após quedas de energia
  //sem isso, esp32 reinicia em loop enquanto roteador ainda está inicializando
  wm.setConnectTimeout(90);
  
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

    // testa multiplos EndPoints - se qualquer um responder, há internet
    // evita falso negativo caso um serviço esteja bloqueado ou instável
    const char* endpoints[] {
          "http://www.google.com/generate_204",               //Goole
          "http://www.msftconnecttest.com/connecttest.txt",   // Microsoft
          "http://connectivitycheck.gstatic.com/generate_204" // Android
    };
    
    HTTPClient http;
    // cria cliente http

    http.setTimeout(3000);
    // define timeout para evitar travamento

    for (const char* url : endpoints){
      http.begin(url);
      int codigo = http.GET();
      http.end();
      if (codigo > 0) return true;
    }
    
    return false;

    // http.GET() > retorna numero positivo se conseguiu acesso a internet , negativo em caso de erro (time out , dns falho, sem internet)
    // se recebeu resposta, considera que há internet
  }


  // Tenta recuperar conexão com a internet
  bool tentarRecuperarInternet(int tentativas){
    
    for (int i = 0;i < tentativas; i ++){

      WiFi.disconnect();
      // desconecta wifi atual

      // Aguarda 500ms em fatias de 10ms, chamando yield() a cada fatia
      // mantem watchdog alimentado e sistema responsivo
      for (int t = 0; t < 50; t++) {delay(10); yield();} // aguarda estabilizar

      WiFi.reconnect();                                  //solicita reconexão ao roteador
      
      // aguarda tentativa de conexão 2000ms em fatias, 2' suficietn para wifi reconectar
      for (int t = 0; t < 200; t++) { delay(10); yield(); }

      if (temInternet()) return true;
        // se coneseguiu internet, retornar sucesso
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