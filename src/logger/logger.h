
// INSTRUÇÕES: Antes de compilar o código de verdade, existe uma fase chamada pré-processador 
//   — ele lê o código e executa instruções especiais que começam com #

#pragma once                                                                                                                                                                                                                                                                                          // ── ATIVAR / DESATIVAR O LOG ──────────────────────────────────────                                                                            
  // Para ativar:   manter o #define abaixo
  // Para desativar: comentar com // na frente da linha
  // Quando desativado, o compilador remove TODO o código de log
  // zero impacto em RAM e flash — igual ao debug.h
  #define LOGGER_ATIVO

  // ── O #ifdef verifica se LOGGER_ATIVO está definido ──────────────
  // Se estiver: compila as funções reais
  // Se não estiver: as funções viram "nada" (inline vazia)
  // Isso é o mesmo truque do debug.h com LOG()
  #ifdef LOGGER_ATIVO

  // Inicializa o módulo de log
  void logger_iniciar();

  // Registra uma leitura completa de todos os sensores no Google Sheets
  // Deve ser chamada no main.cpp após hw_lerTodos()
  void logger_registrarLeitura();

  #else

  // Quando desativado, as funções existem mas não fazem nada
  // O código em main.cpp não precisa mudar — simplesmente não faz nada
  inline void logger_iniciar() {}
  inline void logger_registrarLeitura() {}

  #endif

