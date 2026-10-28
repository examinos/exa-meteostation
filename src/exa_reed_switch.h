#include <twr_gpio.h>
#include <twr_scheduler.h>
#include <twr_exti.h>
#include <twr_module_sensor.h>

typedef enum
{
    //! @brief Rise edge is active
    EXA_REED_SWITCH_EDGE_RISE = TWR_EXTI_EDGE_RISING,

    //! @brief Fall edge is active
    EXA_REED_SWITCH_EDGE_FALL = TWR_EXTI_EDGE_FALLING,

    //! @brief Rise and fall edges are active
    EXA_REED_SWITCH_EDGE_RISE_FALL = TWR_EXTI_EDGE_RISING_AND_FALLING

} exa_reed_switch_edge_t;


void exa_reed_switch_init(twr_module_sensor_channel_t channel, exa_reed_switch_edge_t edge);