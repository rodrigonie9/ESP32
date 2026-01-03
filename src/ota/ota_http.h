/* REGRA DE OURO CPP
    USAR SEMPRE 
    ifndef
    define
    endif

    evitar erros de compilção ao definir telegram_h mais de uma vez
    indefine 
    depois define
*/

#ifndef OTA_HTTP_H
#define OTA_HTTP_H

// Função responsável por atualizar o firmware via OTA HTTP
// Retorna true se a atualização for iniciada com sucesso
bool atualizarFirmwareOTA(const char* urlFirmware);

#endif
