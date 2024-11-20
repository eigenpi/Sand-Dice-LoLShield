// cristinel.ababei (dejazzer); copyleft; 2024
// 
// this program implements "SAND DICE" - an electronic dice pair:
// --Arduino Uno board;
// --LoL Shield, Lots-of-LEDs show a number of sand grains that
//   move on the Shield's display as dictated by the tilting of the shield;
//   the Shield's display is an array of 14x9 LEDs;
// --MPU6050 is attached to the Arduiono board; acceleration data
//   is used to drive the movement of the grains on the display;
// --if Shield is not moved for a short time (i.e., it's let to rest), then,
//   two random numbers in interval [1..6] are generated and displayed
//   on the Shield; sand grains are replaced with the two numbers
//   displayed like two dice;
//
// Credits:
// --this source code is adapted from that of Chuck Swiger
//   his code: https://github.com/cswiger/LED_sand
//   his video: https://www.youtube.com/watch?v=egYnpTUXCXQ
// --libraries needed:
//   1) LoLShield v82.zip 
//      Downloaded from here: https://code.google.com/archive/p/lolshield/downloads
//      Installed from .ZIP inside the Arduino IDE
//   2) Adafruit MPU6050
//      Installed through the Library Manager of the Arduino IDE

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "Charliplexing.h" // initializes the LoL Sheild library;
#include "sandgrains.h"

uint32_t prev_time = 0; // used for frames-per-second throttle;
int16_t ax = 0, ax_prev = 0;
int16_t ay = 0, ay_prev = 0;   
int16_t az = 0;
int16_t change_in_ax = 0, change_in_ay = 0;
uint8_t count_consec_small_changes = 0; // used to detect short periods of no movement;
bool sand_grains_at_rest = false; // when grains stop moving, we roll the dice;

// object for MPU6050 IMU device; will poll repeatedly for acceleration data;
Adafruit_MPU6050 mpu;
// object that contains most of the functionality related to sand grains
// random generation and movement;
SAND_GRAINS sandgrains;

////////////////////////////////////////////////////////////////////////////////
//
// setup
//
////////////////////////////////////////////////////////////////////////////////

void setup()
{
  // (1) initialize the serial, connects to your PC;
  Serial.begin(115200);
  Wire.begin(); // init also I2C;
  Serial.println("-------------------------------------------------------------------------------");
  Serial.println("Welcome to SAND DICE - electronic Dice pair out of sand grains - on LoLShield");
  Serial.println("To use: Shake it, tilt it, and then rest it; wait for two dice values to show");
  Serial.println("Enjoy!");

  // (2) initialize MPU6050;
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.print("Accelerometer range set to: ");
  switch (mpu.getAccelerometerRange()) {
  case MPU6050_RANGE_2_G:
    Serial.println("+-2G");
    break;
  case MPU6050_RANGE_4_G:
    Serial.println("+-4G");
    break;
  case MPU6050_RANGE_8_G:
    Serial.println("+-8G");
    break;
  case MPU6050_RANGE_16_G:
    Serial.println("+-16G");
    break;
  }
  
  // (3) initialize also the LED driver; the one that does 
  // what is called Charliplexing of the LoL Shield;
  LedSign::Init();
  LedSign::SetBrightness(16); // LED brightness, from 0 (off) to 127 (full on)
 
  // (4) initialization of the sand grains object;
  // do one initial reading from the accelerometer for the sake of getting a random
  // number to use as seed for the random number generator;
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  int16_t random_seed =  int(a.acceleration.x * 100);
  Serial.print("Random seed: ");
  Serial.println(random_seed);
  randomSeed(random_seed); // could do also analogRead(0)?
  
  sandgrains.initialization(); // does random initial locations for sand grains;
 
  // (5) tell uer setup has finished...
  Serial.print("Setup done...\n\r");
}

////////////////////////////////////////////////////////////////////////////////
//
// loop
//
////////////////////////////////////////////////////////////////////////////////

void loop()
{
  uint8_t i = 0;
  uint32_t t = 0;

  // (1) wait if necessary so that we achieve the desired frames-per-second refresh;
  while (((t = micros()) - prev_time) < (1000000L / MAX_FPS)) {}; // do nothing;
  prev_time = t;

  // (2) read new data from MPU6050 IMU; only accelerometer data is used for now;
  //Serial.print("Reading accel...\n\r");
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  //Serial.print("Accel readings: X: ");
  //Serial.print(a.acceleration.x);
  //Serial.print(", Y: ");
  //Serial.print(a.acceleration.y);
  //Serial.print(", Z: ");
  //Serial.print(a.acceleration.z);
  //Serial.println(" m/s^2");

  // (3) transform accel values;
  ax = int(a.acceleration.x *10);
  ay = int(a.acceleration.y *10);    
  az = abs(int(a.acceleration.z *100)); 
  
  // (4) save new accel value into "sandgrains" object; 
  sandgrains.set_new_accel_values(ax, ay, az);
  
  // (5) next is a trick to detect that the shield is at rest (i.e., a rest event);
  // we identify that by monitoring acceleration in x,y dimensions;
  // if they do not change consecutively a number of this loop iterations, 
  // then, we guess that shield is resting - which will trigger rolling of the dice;
  change_in_ax = (ax > ax_prev) ? (ax - ax_prev) : (ax_prev - ax);
  change_in_ay = (ay > ay_prev) ? (ay - ay_prev) : (ay_prev - ay);
  if (change_in_ax < CHANGE_GUARD_BAND && change_in_ay < CHANGE_GUARD_BAND) {
    count_consec_small_changes++;
    if (count_consec_small_changes == 4*MAX_FPS) {
      sand_grains_at_rest = true;
    }
  } else {
    count_consec_small_changes = 0; // reset counter;
    sand_grains_at_rest = false; // grains continue to move;
  }
  
  // (6) shield is either in a rest event or still being moved;
  // (a) if we detect shield is resting (has not been moved for 
  // about 1 minute), then, roll dice and display the two die 
  // numbers until shield is moved again - when we essentially 
  // restart the game;
  if ( sand_grains_at_rest == true) {
    if (sandgrains.dice_rolled() == false) { // roll dice one time only;
      sandgrains.roll_the_dice();
      //Serial.println("Rolled dice!");
      // prepare for a new game after rest event will 
      // be interrupted; this will restart the whole thing 
      // from begining (i.e., grains will be randomly placed) 
      // when shield will be moved again;
      sandgrains.reset_grains_are_initialized();
    } else { // dice have been already rolled and shield is still in rest event;
      // keep displaying the rolled dice until shield is moved again;
    }
  }
  // (b) shield is still or again shaken, tilted, etc.
  // that is, grains continue to move or just started moving after 
  // a resting event;
  else {
    // if it is a new run right after a resting event, then,
    // we need to clear display and place randomly grains
    // at intial positions;
    if (sandgrains.grains_are_initialized() == false) { // user moves again the shield;
      sandgrains.initialization();
      //Serial.println("Restarted the game of sand dice...");      
      sandgrains.reset_dice_rolled(); // prepare this so that when in rest again, dice roll;
    } else { // case when user just continues to move shield;
      // keep displaying the moving grains until shield stops moving;
      // () apply 2D accel vector to grain velocities;
      sandgrains.apply_2D_accel_to_grain_velocities();  
      // () then update position of each grain;
      sandgrains.update_position_of_grains();
    }
  } 
  
  // (7) update pixel data in LED driver;
  //Serial.print("Now updating pixels...\n\r");
  for (i = 0; i < WIDTH*HEIGHT; i++) {
    LedSign::Set(i % WIDTH, i / WIDTH, sandgrains.get_img(i));
  }

  // (8) record accel values into "prev" variables in preparation
  // for the next iteration;
  ax_prev = ax;
  ay_prev = ay;
}
