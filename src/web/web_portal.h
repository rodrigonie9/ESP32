#pragma once
#include <Arduino.h>

// Inicializa o servidor web
// Deve ser chamada no setup()
void iniciarWebPortal();

// Deve ser chamada no loop()
// Mantém o servidor web ativo
void webPortalLoop();
