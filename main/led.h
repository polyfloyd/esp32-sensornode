#ifndef _LED_H_
#define _LED_H_


typedef enum {
    Ok,
    WiFiConnecting,
    WiFiPortal,
    Alarm,
    Warning,
} LedState;

void set_led_state(LedState state, bool toggle);
void init_led_task();


#endif
