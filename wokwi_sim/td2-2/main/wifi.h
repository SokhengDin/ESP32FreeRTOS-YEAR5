#ifndef WIFI_H
#define WIFI_H

/* define callback function type for wifi connection */
typedef void (*wifi_connected_callback_t)(void);
/* function used to register the callback */
void wifi_register_callback(wifi_connected_callback_t callback);

/* function used to start wifi from main */
void wifi_start(void);

#endif // WIFI_H