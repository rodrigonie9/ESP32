#pragma once // Limite máximo de sensores DS18B20 por placa 
#define MAX_SENSORES 30

// tempo de espera para temperatura crítica
#define TEMPO_SEGURANCA_CRITICO_MS 180000  // 3 minutos em milissegundos

// Intervalos de tempo (em milissegundos)
#define INTERVALO_LEITURA_SENSORES_MS 120000 // 2 minutos  leitura dos sensores
#define INTERVALO_RELATORIO_ROTINA_MS 900000 // 15 minutos 

#define TEMP_ALERTA_PADRAO 10.0 //falback (segurança)- apenas  se naõ for configurado ou erro no webportal