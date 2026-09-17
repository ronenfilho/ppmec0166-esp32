#ifndef MQTT_TEST_H
#define MQTT_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sobe o cliente MQTT e inicia o teste rapido de pub/sub:
 * - assina um topico de "eco" (ESP32 escuta/ouve)
 * - publica um heartbeat periodico (ESP32 fala/publica)
 * Chamar depois que o Wi-Fi ja estiver conectado.
 */
void mqtt_test_start(void);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_TEST_H */
