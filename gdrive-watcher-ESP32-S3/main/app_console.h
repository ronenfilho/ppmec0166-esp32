#ifndef APP_CONSOLE_H
#define APP_CONSOLE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Registra os comandos e sobe o REPL (USB Serial/JTAG). Chamar por
 * ultimo no boot, depois de store_init()/google_auth_init()/watcher_init(). */
esp_err_t app_console_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CONSOLE_H */
