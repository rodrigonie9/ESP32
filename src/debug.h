#ifndef DEBUG_H
#define DEBUG_H

// Liga ou desliga logs do sistema
#define DEBUG true   // true = desenvolvimento | false = produção

#if DEBUG
  #define LOG(x) Serial.println(x)
#else
  #define LOG(x)
#endif

#endif