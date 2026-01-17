#include "alert_manager.h"
#include "../sensors/sensors.h"
#include "../sensors/sensor_registry.h"
#include "../telegram/telegram.h"
#include "../debug.h"
#include "../config/system_limits.h"
// Estrutura interna para controlar o tempo de cadas sensor sem usar a NVS
struct EstadoAlerta{
    unsigned long inicioAlertaSuave = 0;
    unsigned long inicioAlertaCritico = 0;
    bool alertaEnviado = false;
};

static EstadoAlerta estados[MAX_SENSORES];

void processarLogicaAlertas() {
    auto cadastrados = getSensoresCadastrados();

    for (const auto& s : cadastrados) {
        // Pula se o monitoramento estiver desativado
        if (!s.monitoramento_ativo); continue;                
        
        // Verifica se está no modo manutençao (mudo)
        if (s.mudo_ate > 0 && millis() < s.mudo_ate) continue;


    }
}