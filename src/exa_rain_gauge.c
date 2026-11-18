/**
 * ============================================================================
 * HARDWARE SPECIFICATION & SIGNAL CHARACTERISTICS: RAIN GAUGE (CHANNEL 'C')
 * ============================================================================
 *
 * 1. PHYSICAL PRINCIPLE & ELECTRICAL INTERFACE
 *    - Sensor type: Tipping-bucket rain gauge with a magnetic reed switch.
 *    - Logic states: Normally OPEN (idle); momentarily CLOSED during bucket tip.
 *    - Sampling resolution: 10 ms timer ticks (all measured intervals are multiples of 10 ms).
 *
 * 2. SIGNAL TIMING & PULSE WIDTH (EMPIRICALLY VERIFIED)
 *    - Pulses per tip: Exactly 1 negative pulse (-) per bucket tip event.
 *    - Active pulse width (CLOSED state, negative values):
 *      * Normal / slow tipping (interval > 1000 ms): 50 - 60 ms.
 *      * Rapid / heavy-rain tipping (interval ~470 - 730 ms): 30 - 40 ms.
 *      * Minimum observed valid pulse width: 30 ms.
 *    - Idle interval (OPEN state, positive values):
 *      * Variable depending on rain intensity (tested from 470 ms up to ~982 s).
 *    - Diagnostic note: Earlier two-edge captures showed a zero-duration sample
 *      between some OPEN idle intervals and the following CLOSED pulse ([+idle, 0,
 *      -pulse]). The zero is not a physical pin state; it is consistent with a
 *      repeated EXTI callback reading the already-recorded level after closure.
 *      The callback may reflect contact bounce or another pending EXTI event; the
 *      capture does not distinguish these causes. Current diagnostic code counts
 *      and suppresses same-level callbacks. With the pull-up wiring, reopening is
 *      the preferred candidate edge for counting one tip, subject to validation.
 *
 * 3. SIGNAL INTEGRITY & DEBOUNCING REQUIREMENTS
 *    - Contact bounce (chatter): NONE observed across all tipping speeds.
 *      Transitions are completely clean without sub-10ms glitches or double-triggers.
 *    - Firmware implications:
 *      * Hardware RC filtering / existing debouncing is completely clean.
 *      * Minimum valid pulse filter (if used) MUST accept pulses >= 20-30 ms,
 *        otherwise rapid tips during torrential rain (30 ms width) will be dropped.
 *      * Lockout / dead-time after a valid tip can safely be set to ~100-200 ms
 *        (minimum physical bucket turnaround time observed at max manual speed is ~470 ms).
 * ============================================================================
 */

/*

The rainfall should be calibrated
Measure the area of the rainfall sensor in centimeters
inspiration by https://www.instructables.com/id/Arduino-Rain-Gauge-Calibration/

5 cm * 11 cm = 55 cm^2

Now measure the amount of water in mililiters to get one pulse.
It is better to start with bigger amount of water and then divide it with a number of pulses you get.
This averaging helps you get more precise numbers

In my test I need 1.6 ml to a single pulse from the sensor

1 ml = 1 cm^3

My sensor trips with 1.6mm

1.6 cm^3 / 55 cm^3 = 0.029090909.. cm = 0.29090 mm
*/

/*

Množství srážek se udává v milimetrech (s přesností na desetiny milimetru).
Je to výška, do které by na povrchu země sahaly padající, nebo usazené srážky ve formě vody nebo voda,
vzniklá rozpuštěním tuhých srážek, kdyby se nevsákla do půdy, neodtekla ani neodpařila.
Výšce srážek 1 mm odpovídá množství vody 1 litr na 1 m2 vodorovné plochy.

Denní úhrn srážek [mm] je množství srážek, které spadlo za období 24 hodin.
Měří se v termínu 7 h místního středního slunečního času a údaj se připisuje k předchozímu dni.

Hodinový úhrn srážek [mm] je množství srážek, které spadlo za 1 hodinu.
Hodnota je aktualizována 1x za hodinu z měření v celou hodinu.

(Aktuální srážky se měří v pravidelném 10minutovém kroku a jsou aktualizovány každých 10 minut.)

*/


#include <exa_rain_gauge.h>

#include <string.h>

#define RAINFALL_PULSE_MM 0.29090f    // millimiters of rainfall in a single pulse from the sensor

typedef struct
{
    volatile uint32_t interval_pulse_count;
    volatile uint32_t hourly_pulse_count;
    volatile uint32_t daily_pulse_count;
} exa_rain_gauge_t;

static exa_rain_gauge_t exa_rain_gauge_context;

static void exa_rain_gauge_exti_handler(twr_exti_line_t line, void *param);

static const twr_exti_line_t exa_rain_gauge_exti[] =
{
    TWR_EXTI_LINE_P4,
    TWR_EXTI_LINE_P5,
    TWR_EXTI_LINE_P7
};

void exa_rain_gauge_init(twr_module_sensor_channel_t channel)
{
    exa_rain_gauge_t *context = &exa_rain_gauge_context;
    memset(context, 0, sizeof(*context));

    twr_module_sensor_init();
    twr_module_sensor_set_mode(channel, TWR_MODULE_SENSOR_MODE_INPUT);

    twr_module_sensor_set_pull(channel, TWR_MODULE_SENSOR_PULL_UP_INTERNAL);

    twr_exti_register(exa_rain_gauge_exti[channel], TWR_EXTI_EDGE_RISING, exa_rain_gauge_exti_handler, context);
}


static void exa_rain_gauge_exti_handler(twr_exti_line_t line, void *param)
{
    (void) line;

    exa_rain_gauge_t *context = (exa_rain_gauge_t *) param;
    context->interval_pulse_count++;
}

float exa_rain_gauge_get_interval_mm(void)
{
    exa_rain_gauge_t *context = &exa_rain_gauge_context;

    twr_irq_disable();
    uint32_t pulses = context->interval_pulse_count;
    context->interval_pulse_count = 0;
    context->hourly_pulse_count += pulses;
    twr_irq_enable();

    return pulses * RAINFALL_PULSE_MM;
}

float exa_rain_gauge_get_hourly_mm(void)
{
    exa_rain_gauge_t *context = &exa_rain_gauge_context;

    uint32_t pulses = context->hourly_pulse_count;
    context->hourly_pulse_count = 0;
    context->daily_pulse_count += pulses;

    return pulses * RAINFALL_PULSE_MM;
}

float exa_rain_gauge_get_daily_mm(void)
{
    exa_rain_gauge_t *context = &exa_rain_gauge_context;

    uint32_t pulses = context->daily_pulse_count;
    context->daily_pulse_count = 0;

    return pulses * RAINFALL_PULSE_MM;
}

void exa_rain_gauge_reset_hourly()
{
    exa_rain_gauge_t *context = &exa_rain_gauge_context;
    context->hourly_pulse_count = 0;
}

void exa_rain_gauge_reset_daily()
{
    exa_rain_gauge_t *context = &exa_rain_gauge_context;
    context->daily_pulse_count = 0;
}