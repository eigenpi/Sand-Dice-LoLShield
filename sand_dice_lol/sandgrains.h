// this implements the accelerated sand grains application
// running on the 14 x 9 Charliplexed LoL Shield;

#ifndef _SAND_GRAINS_H_
#define _SAND_GRAINS_H_

#include "Arduino.h"

// you can change these defines as desired; for example, if you want
// just four grains change N_GRAINS to 4;
#define N_GRAINS     16 // number of grains of sand
#define WIDTH        14 // display width in pixels
#define HEIGHT        9 // display height in pixels
#define MAX_FPS      12 // maximum redraw rate, frames/second

#define CHANGE_GUARD_BAND 10

// the 'sand' grains exist in an integer coordinate space that's 256X
// the scale of the pixel grid, allowing them to move and interact at
// less than whole-pixel increments;
// that is the LED array of (14 x 9) is mapped into a space of 
// (3584 x 2304) coordinates;
#define MAX_X (WIDTH  * 256 - 1) // Maximum X coordinate in "grain space"
#define MAX_Y (HEIGHT * 256 - 1) // Maximum Y coordinate

// structure to model each sand grain with its coordinates and velocities;
struct GRAIN {
  int16_t  x,  y; // Position
  int16_t vx, vy; // Velocity
};

////////////////////////////////////////////////////////////////////////////////
//
// SAND_GRAINS
//
////////////////////////////////////////////////////////////////////////////////

class SAND_GRAINS {
	private:
    uint16_t _led_brightness;
    // this will be used to stop rolling the dice once they are rolled;
    bool _dice_rolled;
    bool _grains_are_initialized;

	public:
    struct GRAIN grain[N_GRAINS];
    uint8_t img[WIDTH * HEIGHT]; // internal 'map' of pixels;
    // grain accelerations from the accelerometer; will be continuously updated;
    int16_t ax; 
    int16_t ay;    
    int16_t az;

	public:
		SAND_GRAINS() {
      _led_brightness = 24;
      _dice_rolled = false;
      _grains_are_initialized = false;
      ax = 0; 
      ay = 0; 
      az = 0;
    }
		~SAND_GRAINS() {}

    // setting functions;
    void set_dice_rolled() { _dice_rolled = true; }
    void reset_dice_rolled() { _dice_rolled = false; }
    void set_grains_are_initialized() { _grains_are_initialized = true; }
    void reset_grains_are_initialized() { _grains_are_initialized = false; }
    void set_led_brightness(uint16_t led_brightness_new) { _led_brightness = led_brightness_new; }
    void set_new_accel_values(uint16_t ax_new, uint16_t ay_new, uint16_t az_new) { 
      ax = ax_new; ay = ay_new; az = az_new;
    }
    // getting functions;
    uint16_t led_brightness() { return _led_brightness; }
    bool dice_rolled() { return _dice_rolled; }
    bool grains_are_initialized() { return _grains_are_initialized; }
    uint16_t grain_x(uint8_t i) { return grain[i].x; }
    uint16_t grain_y(uint8_t i) { return grain[i].y; }
    uint8_t get_img(uint8_t i) { return img[i]; }
    
    void initialization();   
    void apply_2D_accel_to_grain_velocities();
    void update_position_of_grains();
    void roll_the_dice();
    bool are_grains_at_rest();
};

#endif // _SAND_GRAINS_H_
