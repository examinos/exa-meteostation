#include <exa_reed_switch.h>
#include <twr_system.h>
#include <twr_radio_pub.h>

#define EXA_REED_SWITCH_DEBOUNCE_TIME 100


typedef struct
{
    twr_module_sensor_channel_t channel;
    exa_reed_switch_edge_t edge;
    unsigned int count;

    int last_state;
    int has_edge;
    twr_tick_t last_edge_time;
    twr_tick_t closed_duration;
    twr_tick_t open_duration;
    twr_tick_t cycle_closed_duration;
    twr_tick_t cycle_open_duration;
    twr_tick_t bounce_start_time;
    twr_tick_t bounce_duration;
    int bounce_active;
    int has_falling_edge;
    int report_pending;
    uint32_t bounce_count;    

    twr_scheduler_task_id_t task_id;

} exa_reed_switch_t;

static exa_reed_switch_t _exa_reed_switch_counter[3];
static const twr_gpio_channel_t exa_reed_switch_gpio[3] =
{
    TWR_GPIO_P4,
    TWR_GPIO_P5,
    TWR_GPIO_P7
};

static void _exa_reed_switch_channel_exti(twr_exti_line_t line, void *param);
static void _exa_reed_switch_radio_task(void *param);

void exa_reed_switch_init(twr_module_sensor_channel_t channel, exa_reed_switch_edge_t edge)
{
    if (channel > TWR_MODULE_SENSOR_CHANNEL_C) return;

    static const twr_exti_line_t exa_reed_switch_exti_line[3] =
    {
        TWR_EXTI_LINE_PA4,
        TWR_EXTI_LINE_PA5,
        TWR_EXTI_LINE_PA7
    };

    memset(&_exa_reed_switch_counter[channel], 0, sizeof(_exa_reed_switch_counter[channel]));

    twr_module_sensor_init();
    twr_module_sensor_set_mode(channel, TWR_MODULE_SENSOR_MODE_INPUT);
    twr_module_sensor_set_pull(channel, TWR_MODULE_SENSOR_PULL_UP_INTERNAL); // 40kΩ

    _exa_reed_switch_counter[channel].channel = channel;
    _exa_reed_switch_counter[channel].edge = edge;
    _exa_reed_switch_counter[channel].last_state = twr_gpio_get_input(exa_reed_switch_gpio[channel]);
    _exa_reed_switch_counter[channel].last_edge_time = twr_tick_get();

    _exa_reed_switch_counter[channel].task_id = twr_scheduler_register(_exa_reed_switch_radio_task, &_exa_reed_switch_counter[channel], TWR_TICK_INFINITY);

    twr_exti_register(exa_reed_switch_exti_line[channel], (twr_exti_edge_t) edge, _exa_reed_switch_channel_exti, &_exa_reed_switch_counter[channel]);
}


static void _exa_reed_switch_channel_exti(twr_exti_line_t line, void *param)
{
    (void) line;

    exa_reed_switch_t *ctx = (exa_reed_switch_t *) param;
    
    twr_tick_t current_time = twr_tick_get();

    twr_gpio_channel_t gpio_pin = exa_reed_switch_gpio[ctx->channel];
    int pin_state = twr_gpio_get_input(gpio_pin);

    twr_tick_t state_duration = current_time - ctx->last_edge_time;

    if (ctx->has_edge && state_duration <= EXA_REED_SWITCH_DEBOUNCE_TIME)
    {
        if (!ctx->bounce_active)
        {
            ctx->bounce_start_time = ctx->last_edge_time;
            ctx->bounce_active = 1;
        }

        ctx->bounce_count++;
    }

    if (ctx->last_state == 0)
    {
        ctx->closed_duration = state_duration;
    }
    else
    {
        ctx->open_duration = state_duration;
    }

    if (pin_state == 0)
    {
        if (ctx->has_falling_edge)
        {
            ctx->cycle_closed_duration = ctx->closed_duration;
            ctx->cycle_open_duration = ctx->open_duration;
            ctx->report_pending = 1;
        }

        ctx->has_falling_edge = 1;
    }

    ctx->last_state = pin_state; 
    ctx->count++; 
    ctx->has_edge = 1;
    ctx->last_edge_time = current_time;

    twr_scheduler_plan_from_now(ctx->task_id, EXA_REED_SWITCH_DEBOUNCE_TIME);
}

static void _exa_reed_switch_radio_task(void *param)
{
    exa_reed_switch_t *ctx = (exa_reed_switch_t *) param;

    char topic[40];
    twr_tick_t current_time = twr_tick_get();
    twr_tick_t state_duration = current_time - ctx->last_edge_time;

    if (state_duration < EXA_REED_SWITCH_DEBOUNCE_TIME)
    {
        twr_scheduler_plan_from_now(ctx->task_id, EXA_REED_SWITCH_DEBOUNCE_TIME - state_duration);
        return;
    }

    if (!ctx->report_pending)
    {
        return;
    }

    uint32_t closed_duration = (uint32_t)ctx->cycle_closed_duration;
    uint32_t open_duration = (uint32_t)ctx->cycle_open_duration;
    if (ctx->bounce_active)
    {
        ctx->bounce_duration = ctx->last_edge_time - ctx->bounce_start_time;
    }

    uint32_t bounce_duration = (uint32_t)ctx->bounce_duration;
    uint32_t bounce_count = ctx->bounce_count;

    snprintf(topic, sizeof(topic), "anemometer/state/closed_ms");
    twr_radio_pub_uint32(topic, &closed_duration);

    snprintf(topic, sizeof(topic), "anemometer/state/open_ms");
    twr_radio_pub_uint32(topic, &open_duration);

    snprintf(topic, sizeof(topic), "anemometer/bounces");
    twr_radio_pub_uint32(topic, &bounce_count);

    snprintf(topic, sizeof(topic), "anemometer/bounce_duration_ms");
    twr_radio_pub_uint32(topic, &bounce_duration);

    ctx->bounce_count = 0;
    ctx->bounce_duration = 0;
    ctx->bounce_active = 0;
    ctx->report_pending = 0;
}