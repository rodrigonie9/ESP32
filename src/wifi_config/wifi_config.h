/* REGRA DE OURO CPP
    USAR SEMPRE 
    ifndef
    define
    endif

    evitar erros de compilção ao definir wifi_manager_h mais de uma vez
    indefine 
    depois define
*/

// wifi.manager.h declara as funções o que existe

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

// Função que conectar o ESP32 ao wifi
void conectarWiFi();

//confere wifi+internet
bool temInternet();
bool tentarRecuperarInternet(int tentativas);
bool internetDisponivel();


#endif
