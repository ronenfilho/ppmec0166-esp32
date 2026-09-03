#ifndef MQTT_MAIN_H
#define MQTT_MAIN_H

/* Inicia o cliente MQTT: conecta ao broker, assina o topico de comando
   e publica o status do LED. Chamar depois que o Wi-Fi ja estiver
   conectado. */
void mqtt_app_start(void);

#endif /* MQTT_MAIN_H */
