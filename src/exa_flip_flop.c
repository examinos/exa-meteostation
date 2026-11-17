#include <exa_flip_flop.h>

#include <string.h>

#include <twr_exti.h>
#include <twr_gpio.h>
#include <twr_log.h>
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
    volatile uint8_t active_size;
    volatile int8_t pending_buffer;
    volatile uint32_t dropped_samples;
    volatile uint32_t edge_count;
    volatile uint32_t duplicate_interrupts;
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

    twr_log_info("flip-flop %c initialized, gpio %d, exti %d", 'A' + channel,
            exa_flip_flop_gpio[channel], exa_flip_flop_exti[channel]);
    twr_scheduler_plan_relative(context->task_id, 5000);
}

static void exa_flip_flop_exti_handler(twr_exti_line_t line, void *param)
{
    (void) line;

    exa_flip_flop_t *context = (exa_flip_flop_t *) param;
    int pin_state = twr_gpio_get_input(exa_flip_flop_gpio[context->channel]);

    if (pin_state == context->state)
    {
        context->duplicate_interrupts++;
        return;
    }

    twr_tick_t now = twr_tick_get();
    context->edge_count++;
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

    context->state = pin_state;
    context->state_start = now;
}

static void exa_flip_flop_radio_task(void *param)
{
    exa_flip_flop_t *context = (exa_flip_flop_t *) param;

    if (context->pending_buffer < 0)
    {
        twr_log_debug("flip-flop %c progress: edges=%lu samples=%u/%u dropped=%lu duplicate=%lu",
            'A' + context->channel, (unsigned long) context->edge_count,
            (unsigned int) context->active_size, (unsigned int) EXA_FLIP_FLOP_BUFFER_SIZE,
            (unsigned long) context->dropped_samples, (unsigned long) context->duplicate_interrupts);
        twr_scheduler_plan_current_relative(5000);
        return;
    }

    int8_t pending_buffer = context->pending_buffer;
    char channel_tag = 'A' + context->channel;
    uint8_t radio_payload[1 + sizeof(context->buffer[pending_buffer])];

    radio_payload[0] = (uint8_t) channel_tag;
    memcpy(radio_payload + 1, context->buffer[pending_buffer], sizeof(context->buffer[pending_buffer]));

        twr_log_debug("flip-flop %c buffer ready: %u samples, %lu edges, %lu dropped, %lu duplicate",
            channel_tag, (unsigned int) EXA_FLIP_FLOP_BUFFER_SIZE,
            (unsigned long) context->edge_count, (unsigned long) context->dropped_samples,
            (unsigned long) context->duplicate_interrupts);
    twr_log_dump(radio_payload, sizeof(radio_payload), "flip-flop %c radio payload", channel_tag);

    if (twr_radio_pub_buffer(radio_payload, sizeof(radio_payload)))
    {
        twr_log_info("flip-flop %c buffer queued to radio", channel_tag);
        context->pending_buffer = -1;
        context->dropped_samples = 0;
        twr_scheduler_plan_current_relative(5000);
    }
    else
    {
        twr_log_warning("flip-flop %c buffer radio queue full; retrying", channel_tag);
        twr_scheduler_plan_from_now(context->task_id, 100);
    }
}