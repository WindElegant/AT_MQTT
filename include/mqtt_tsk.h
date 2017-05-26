#ifndef __MQTT_TSK__
#define __MQTT_TSK__

#define MQTT_TOPIC_MAX_LEN 1024

#include "event.h"

#include "MQTTClient.h"

#define EV_CFW_MQTT_CONNECT    (EV_CFW_XX_IND_END + 1)
#define EV_CFW_MQTT_DISCONNECT (EV_CFW_XX_IND_END + 2)
#define EV_CFW_MQTT_PINGREQ     (EV_CFW_XX_IND_END + 3)

void mqtt_init(MQTT_CFG_T *);
void mqtt_exit(void);

void set_mqtt_cfg(MQTT_CFG_T *);
void get_mqtt_cfg(MQTT_CFG_T *);

#endif
