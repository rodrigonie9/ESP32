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

// Conecta ao Wi-Fi salvo (ou abre portal de configuração na primeira vez)
void conectarWiFi();

// Testa se há acesso real à internet (não só Wi-Fi conectado)
bool temInternet();

// Retorna true se há internet agora — única função que deve ser chamada
// antes de qualquer acesso à rede no restante do projeto
bool internetDisponivel();

// Watchdog de internet — chamar a cada ciclo no loop() do main.cpp.
// Escala a recuperação: aguarda → reset do driver → restart da placa.
void verificarWatchdogInternet();


#endif
