#include <exa_flip_flop.h>

#include <string.h>

#include <twr_exti.h>
#include <twr_gpio.h>
#include <twr_radio_pub.h>
#include <twr_scheduler.h>
#include <twr_system.h>

#define EXA_FLIP_FLOP_CHANNEL_COUNT 3

typedef struct
{
    twr_module_sensor_channel_t channel;
    twr_scheduler_task_id_t task_id;
    int state;
    twr_tick_t state_start;
    int32_t buffer[2][EXA_FLIP_FLOP_BUFFER_SIZE];
    uint8_t active_buffer;
    uint8_t active_size;
    int8_t pending_buffer;
    uint32_t dropped_samples;
} exa_flip_flop_t;

static exa_flip_flop_t exa_flip_flop_context[EXA_FLIP_FLOP_CHANNEL_COUNT];

static const twr_gpio_channel_t exa_flip_flop_gpio[EXA_FLIP_FLOP_CHANNEL_COUNT] =
{
    TWR_GPIO_P4,
    TWR_GPIO_P5,
    TWR_GPIO_P7
};

static const twr_exti_line_t exa_flip_flop_exti[EXA_FLIP_FLOP_CHANNEL_COUNT] =
{
    TWR_EXTI_LINE_PA4,
    TWR_EXTI_LINE_PA5,
    TWR_EXTI_LINE_PA6
};

static void exa_flip_flop_exti_handler(twr_exti_line_t line, void *param);
static void exa_flip_flop_radio_task(void *param);

void exa_flip_flop_init(twr_module_sensor_channel_t channel)
{
    if (channel > TWR_MODULE_SENSOR_CHANNEL_C)
    {
        return;
    }

    exa_flip_flop_t *context = &exa_flip_flop_context[channel];

    memset(context, 0, sizeof(*context));
    context->channel = channel;
    context->pending_buffer = -1;

    twr_module_sensor_init();
    twr_module_sensor_set_mode(channel, TWR_MODULE_SENSOR_MODE_INPUT);
    twr_module_sensor_set_pull(channel, TWR_MODULE_SENSOR_PULL_UP_INTERNAL);

    context->state = twr_gpio_get_input(exa_flip_flop_gpio[channel]);
    context->state_start = twr_tick_get();
    context->task_id = twr_scheduler_register(exa_flip_flop_radio_task, context, TWR_TICK_INFINITY);

    twr_exti_register(exa_flip_flop_exti[channel], TWR_EXTI_EDGE_RISING_AND_FALLING,
            exa_flip_flop_exti_handler, context);
}

static void exa_flip_flop_exti_handler(twr_exti_line_t line, void *param)
{
    (void) line;

    exa_flip_flop_t *context = (exa_flip_flop_t *) param;
    twr_tick_t now = twr_tick_get();
    twr_tick_t duration = now - context->state_start;
    int32_t sample = (int32_t) duration;

    if (context->state == 0)
    {
        sample = -sample;
    }

    if (context->pending_buffer < 0)
    {
        context->buffer[context->active_buffer][context->active_size++] = sample;

        if (context->active_size == EXA_FLIP_FLOP_BUFFER_SIZE)
        {
            context->pending_buffer = context->active_buffer;
            context->active_buffer ^= 1;
            context->active_size = 0;
            twr_scheduler_plan_now(context->task_id);
        }
    }
    else
    {
        context->dropped_samples++;
    }

    context->state = twr_gpio_get_input(exa_flip_flop_gpio[context->channel]);
    context->state_start = now;
}

static void exa_flip_flop_radio_task(void *param)
{
    exa_flip_flop_t *context = (exa_flip_flop_t *) param;

    if (context->pending_buffer < 0)
    {
        return;
    }

    int8_t pending_buffer = context->pending_buffer;

    if (twr_radio_pub_buffer(context->buffer[pending_buffer], sizeof(context->buffer[pending_buffer])))
    {
        context->pending_buffer = -1;
    }
    else
    {
        twr_scheduler_plan_from_now(context->task_id, 100);
    }
}