// HARDWARE / fio

#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>


// Valor padrão para indicar erro de leitura (já definido pela biblioteca Dallas)
#define SENSOR_ERRO DEVICE_DISCONNECTED_C



// ================= ESTATÍSTICAS DOS SENSORES =================
struct SensorStats{
    uint32_t leituras_ok;       //Sucessos
    uint32_t leituras_erro;     //Falhas
    float ultima_temp;          //última temperatura válida
    bool ultimo_erro;           // se a última temperatua falhou
    time_t ultimo_timestamp;    // hora da última leitura bem-sucedida (0 = nunca leu)
                                // time_t numero inteiro, guarda segundos desde 01/01/1970
                                // função time(nullptr) - retorna hora atual do NTP
};

// ================= FUNÇÕES DE HARDWARE (O que mexe no fio) =================

void hw_iniciar();         // Inicializa o barramento 1-Wire e detecta sensores
void hw_lerTodos();             // Faz a leitura física de todos os sensores

// ================= FUNÇÕES DE INFORMAÇÃO (O que consulta a RAM) =================
uint8_t hw_getContagem();                             // Quantos senores respondem no fio agora
const String& hw_getID(uint8_t indice);               // Pega o ID (ROM) pela posição

float hw_getTemp(const String& id_fisico);

SensorStats hw_getStats(const String& id_fisico);
