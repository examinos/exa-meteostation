#ifndef _EXA_FLIP_FLOP_H
#define _EXA_FLIP_FLOP_H

#include <stdint.h>

#include <twr_module_sensor.h>
#include <twr_radio.h>

//! @brief Number of signed millisecond samples in one radio payload
#define EXA_FLIP_FLOP_BUFFER_SIZE ((TWR_RADIO_MAX_BUFFER_SIZE - 1) / sizeof(int32_t))

//! @brief Initialize flip-flop measurement on a sensor channel
//!
//! Every rising and falling edge is measured. High-level durations are stored
//! as positive values and low-level durations as negative values.
void exa_flip_flop_init(twr_module_sensor_channel_t channel);

#endif // _EXA_FLIP_FLOP_H


/*
[
  110, -70, 760, -40, 30, -18280, 600, -490, 260, -390, 
  240, -550, 300, -460, 270, -640, 360, -530, 330, -760, 
  440, -370, 110, -80, 140, -240, 170, -330, 160, -290, 
  210, -310, 170, -370, 200, -300, 190, -420, 230, -350, 
  200, -480, 270, -390, 240, -540, 300, -220, 90, -200, 
  120, -160, 100, -230, 120, -190, 110, -250, 140, -200, 
  130, -280, 150, -230, 140, -310, 170, -260, 150, -350, 
  200, -290, 170, -390, 540, -1350, 890, -1270, 770, -13300, 
  335738, -794020, 130, -210, 120, -290, 150, -230, 140, -320, 
  170, -260, 160, -350, 200, -290, 180, -400, 220, -360, 
  200, -450, 260, -370, 230, -520, 290, -430, 260, -600, 
  340, -500, 310
]


[
25020,
0,
50,
5200,
0,
40,
490,
0,
50,
5370,
0,
50,
510
]

*/