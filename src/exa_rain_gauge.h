#include <twr_gpio.h>
#include <twr_exti.h>
#include <twr_module_sensor.h>
#include <stdint.h>

void exa_rain_gauge_init(twr_module_sensor_channel_t channel);

float exa_rain_gauge_get_interval_mm(void);
float exa_rain_gauge_get_hourly_mm(void);
float exa_rain_gauge_get_daily_mm(void);

void exa_rain_gauge_reset_hourly(void);
void exa_rain_gauge_reset_daily(void);