#include "led.h"
#include <NeoPixelBus.h>
#include <esp_event.h>

const int brightness = 0xff;

NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> led(1, 26);
volatile static uint32_t led_state_bits = 0;

void set_led(int r, int g, int b, int w = 0) {
    led.ClearTo(RgbwColor(r, g, b, w));
    led.Show();
}

void set_led_state(LedState state, bool toggle) {
    if (toggle) {
        led_state_bits |= 1<<state;
    } else {
        led_state_bits &= ~0L ^ (1<<state);
    }
}

void init_led_task() {
    led.Begin();
    set_led(0, 0, 0, 0);

    TaskHandle_t h = NULL;
    xTaskCreate([](void*) {
        while (true) {
            if (led_state_bits & 1<<WiFiConnecting) {
                set_led(0, 0, abs(sin(.001 * millis())) * brightness);
            } else if (led_state_bits & 1<<WiFiPortal) {
                set_led(
                    abs(sin(.001 * millis()) * 2) * brightness,
                    abs(cos(.001 * millis())) * brightness,
                    0
                );
            } else if (led_state_bits & 1<<Warning) {
                set_led(abs(sin(.001 * millis())) * brightness, 0, 0);
            } else if (led_state_bits & 1<<Alarm) {
                set_led(abs(sin(.005 * millis())) * brightness, 0, 0);
            } else {
                set_led(0, 0, 0);
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }, "led", 2048, NULL, tskIDLE_PRIORITY, &h);
    assert(h);
}
