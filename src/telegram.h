/* REGRA DE OURO CPP
    USAR SEMPRE 
    ifndef
    define
    endif

    evitar erros de compilção ao definir telegram_h mais de uma vez
    indefine 
    depois define
*/

// telegram.h declara as funções o que existe
#ifndef TELEGRAM_H
#define TELEGRAM_H

#include <Arduino.h>

// Função responsável por enviar mensagens via Telegram
void enviarMensagemTelegram(String mensagem);

//Funções OTA
// Verifica mensagens do Telegram (getUpdates)
void verificarMensagensTelegram();
// Retorna true se o comando /update foi recebido
bool comandoAtualizarRecebido();

#endif