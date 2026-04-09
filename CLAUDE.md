# CLAUDE.md — Projeto controle_temperatura

Este arquivo é lido automaticamente pelo Claude Code no início de cada conversa.
Contém o contexto completo do projeto para não precisar repetir a cada sessão.

---

## Quem é o usuário

- Está **aprendendo C++** — usa os comentários do código para aprender
- **Digita o código manualmente** — nunca assumir copia/cola
- Quer **entender o porquê** de cada decisão, não só o que fazer
- Gosta de ser guiado pelo raciocínio (método socrático): fazer perguntas antes de dar a solução

**Como escrever código:**
- Blocos pequenos e claros, um arquivo por vez
- Sempre muitos comentários explicativos
- Explicar o "porquê" de cada decisão, não só o "o quê"
- Analogias simples ajudam muito (ex: comparar com conceitos que ele já conhece)
- Antes de dar a solução, fazer perguntas para guiar o raciocínio

---

## O projeto

Sistema embarcado de monitoramento de múltiplas temperaturas com ESP32.

**Hardware:** ESP32 com dois barramentos 1-Wire (GPIO 18 e GPIO 21), até 30 sensores DS18B20.

**Módulos:**
- `sensors` — lê DS18B20 via Dallas Temperature, cacheia leituras
- `sensor_registry` — configuração persistente no NVS (nome amigável, thresholds, agenda)
- `alert_manager` — lógica de alertas com agendas e modo manutenção
- `telegram` — bot Telegram para notificações e OTA via `/update`
- `web_portal` — interface web porta 80 (configuração + visualização)
- `wifi_config` — provisionamento WiFi via WiFiManager + watchdog de internet
- `ota_http` — atualização de firmware via HTTP (GitHub Releases)
- `device_config` — nome amigável do dispositivo (mDNS)

**Fluxo principal (main.cpp):**
- A cada 2 minutos: lê sensores + processa alertas + envia log ao Google Sheets
- Reboot programado às 12h (salvo no NVS)

**Alertas:**
- Dois níveis: suave e crítico
- Timers independentes por sensor, persistidos no NVS (sobrevivem ao reboot)
- Agendas por dia da semana: Off / 24h / Horário customizado
- Modo manutenção por duração (1h/2h/.../24h), salvo no NVS
- Crítico suprime o suave enquanto ativo
- Delay anti-falso-positivo configurável por sensor

**Persistência:** NVS com namespaces separados — "device", "sensors_reg", "telegram", "alert_state"

---

## Restrições de hardware — SEMPRE considerar

Antes de escrever qualquer código, verificar mentalmente:

- **RAM limitada (~320KB heap):** evitar alocações dinâmicas desnecessárias, strings grandes, cópias de objetos. Preferir `const String&`, buffers fixos, PROGMEM para dados estáticos
- **Flash/Stack:** sem recursão profunda, sem variáveis locais grandes
- **Não bloquear o loop:** usar `millis()`, nunca `delay()` em operações longas
- **NVS:** limite de tamanho por chave — cuidado com JSONs grandes
- **WiFi e 1-Wire:** operações de rede podem causar jitter nos timings
- **Watchdog timer:** loops longos ou bloqueantes causam reset por WDT
- **ArduinoJson:** sempre usar tamanho de documento adequado
- **HTTPClient:** sempre chamar `.end()` após requisições

---

## Estado atual dos testes (retomar aqui)

### Grupo 1 — Alertas de temperatura
- ✅ 1.1 Timer suave — fluxo completo
- ✅ 1.2 Degelo normal (sem aviso)
- ✅ 1.3 Timer crítico — fluxo completo
- ✅ 1.4 Crítico suprime o suave
- ✅ 1.5 Timer suave continua quando sai do crítico
- ✅ 1.6 Agenda OFF durante alerta ativo (timers zerados na RAM e NVS)
- ✅ 1.7 Manutenção silencia sem resetar timers
- ✅ 1.8 NTP falha → monitora por padrão (timers do NVS descartados, agendas ignoradas)
- **⏳ PRÓXIMO: 1.9** — Boundary exato no limiar suave
- ⬜ 1.10 Boundary exato no limiar crítico
- ⬜ 1.11 Múltiplos sensores em alerta simultâneo
- ⬜ 1.12 Repetição suave a cada 30 min
- ⬜ 1.13 Repetição crítico a cada 10 min

### Grupos 2–12 — todos pendentes
- Grupo 12 (Watchdog de internet): aguardando acesso ao roteador (senha TP-Link) para desligar cabo

---

## Melhorias planejadas (não urgentes)

### Pendentes de implementação
- **Segurança — autenticação web portal** (HTTP Basic Auth ou formulário)
- **Apps Script Google Sheets** — arquivamento mensal, histórico por abas
- **Token pelo WiFiManager** — distribuir .bin sem recompilar (token + chatID no portal captivo)
- **Portal central** — múltiplas placas (curto prazo: Telegram já centraliza; longo prazo: MQTT/HTTP)
- **Avisos de boot/OTA no Telegram** — ainda em discussão (restart inesperado pode ser útil avisar)

### Já implementado (histórico)
- ✅ Watchdog de internet (08/04/2026)
- ✅ Alerta sensor offline — bugs corrigidos (06/04/2026)
- ✅ Nome amigável único — validação web portal (06/04/2026)
- ✅ Timers de alerta no NVS — sobrevivem ao reboot (05/04/2026)
- ✅ Modo manutenção — timer visível + cancelar (05/04/2026)
- ✅ Reboot programado — sem loop duplo, timestamp no NVS (05/04/2026)
- ✅ Sistema de alertas — reescrita completa (01/04/2026)
- ✅ Log de temperaturas — Google Sheets funcionando
- ✅ Otimização de flash — reparticionamento + `-Os`
- ✅ Web portal — layout modernizado
- ✅ OTA — fix HTTP 302, feedback no Telegram
