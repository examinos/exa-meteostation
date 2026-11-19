#include <twr_gpio.h>
#include <twr_exti.h>
#include <twr_module_sensor.h>
#include <stdint.h>

typedef void (*exa_rain_gauge_event_handler_t)(void *event_param);

void exa_rain_gauge_init(twr_module_sensor_channel_t channel);
// Called once for each batch of 3 counted pulses, from scheduler context.
void exa_rain_gauge_set_event_handler(exa_rain_gauge_event_handler_t event_handler, void *event_param);

float exa_rain_gauge_take_interval_mm(void);
float exa_rain_gauge_take_hourly_mm(void);
float exa_rain_gauge_take_daily_mm(void);
