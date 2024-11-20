#include "sandgrains.h"

///////////////////////////////////////////////////////////////////////////////
//
// SAND_GRAINS functions
//
///////////////////////////////////////////////////////////////////////////////

void SAND_GRAINS::initialization() 
{
  // initializations:
  // clear img array and set al lentries to 0;
  // assign initial different random locations to all sand grains;
  // NOTE: before this function call you should have seed the seed
  // of the random number generator;
  uint8_t i = 0, j = 0;

  // (1) clear all LEDs;
  for (i = 0; i < WIDTH * HEIGHT; i++) { 
    img[i] = 0;
  }

  // (2) generate random initial locations for grains;
  for (i = 0; i < N_GRAINS; i++) {
    do {
      grain[i].x = random(WIDTH  * 256); // Assign random position within
      grain[i].y = random(HEIGHT * 256); // the 'grain' coordinate space
      // Check if corresponding pixel position is already occupied...
      for (j = 0; (j < i) && 
        (((grain[i].x / 256) != (grain[j].x / 256)) || 
        ((grain[i].y / 256) != (grain[j].y / 256))); j++) {
          // do nothing;
        };
    } while (j < i); // Keep retrying until a clear spot is found
    img[(grain[i].y / 256) * WIDTH + (grain[i].x / 256)] = 1; // Mark it
    grain[i].vx = grain[i].vy = 0; // Initial velocity is zero
  }
  
  // (3) record that we done initialization;
  set_grains_are_initialized();
}

void SAND_GRAINS::apply_2D_accel_to_grain_velocities()
{
  // called after a new set of acceleration values have been read in;
  uint8_t i = 0;
  int32_t v2 = 0; // Velocity squared
  float v = 0.0; // Absolute velocity
  
  az = (az >= 3) ? 1 : 4 - az; // Clip & invert; az values: {1,2,3,4}
  ax -= az;                    // Subtract motion factor from X
  ay -= az;                    // Subtract motion factor from Y
  int16_t az2 = az*2 + 1;      // Range of random motion to add back in; can be in [2..17]
  
  //Serial.print("Accel read: ");
  //Serial.print(ax);
  //Serial.print(" ");
  //Serial.print(ay);
  //Serial.print(" ");
  //Serial.print(az);
  //Serial.print("\n\r");
  
  // apply 2D accel vector to grain velocities;
  for (i = 0; i < N_GRAINS; i++) {
    grain[i].vx += ax + random(az2); // A little randomness makes
    grain[i].vy += ay + random(az2); // tall stacks topple better!

    // Terminal velocity (in any direction) is 256 units -- equal to
    // 1 pixel -- which keeps moving grains from passing through each other
    // and other such mayhem. Though it takes some extra math, velocity is
    // clipped as a 2D vector (not separately-limited X & Y) so that
    // diagonal movement isn't faster;
    v2 = ((int32_t)grain[i].vx * grain[i].vx) + ((int32_t)grain[i].vy * grain[i].vy);
    if (v2 > 65536) { // If v^2 > 65536, then v > 256
      v = sqrt((float)v2); // Velocity vector magnitude
      grain[i].vx = (int)(256.0*(float)grain[i].vx/v); // Maintain heading
      grain[i].vy = (int)(256.0*(float)grain[i].vy/v); // Limit magnitude
    }
  }
}

void SAND_GRAINS::update_position_of_grains()
{
  // update position of each grain, one at a time, checking for
  // collisions and having them react. This really seems like it shouldn't
  // work, as only one grain is considered at a time while the rest are
  // regarded as stationary. Yet this naive algorithm, taking many not-
  // technically-quite-correct steps, and repeated quickly enough,
  // "visually integrates" into something that somewhat resembles physics.
  // (I'd initially tried implementing this as a bunch of concurrent and
  // "realistic" elastic collisions among circular grains, but the
  // calculations and volume of code quickly got out of hand for both
  // the tiny 8-bit AVR microcontroller and my tiny dinosaur brain.)
  // NOTE: this is basically a hack! 
  uint8_t i = 0, oldidx = 0, newidx = 0, delta = 0;
  int16_t newx = 0, newy = 0;

  // (1) process all grains;
  for (i = 0; i < N_GRAINS; i++) {
    newx = grain[i].x + grain[i].vx; // New position in grain space
    newy = grain[i].y + grain[i].vy;
    if (newx > MAX_X) {      // If grain would go out of bounds
      newx = MAX_X;          // keep it inside, and
      grain[i].vx /= -2;     // give a slight bounce off the wall
    } else if (newx < 0) {
      newx = 0;
      grain[i].vx /= -2;
    }
    if (newy > MAX_Y) {
      newy = MAX_Y;
      grain[i].vy /= -2;
    } else if(newy < 0) {
      newy = 0;
      grain[i].vy /= -2;
    }

    oldidx = (grain[i].y/256) * WIDTH + (grain[i].x/256); // Prior pixel #
    newidx = (newy      /256) * WIDTH + (newx      /256); // New pixel #
    if ((oldidx != newidx) && // If grain is moving to a new pixel...
        img[newidx]) {        // but if that pixel is already occupied...
      delta = abs(newidx - oldidx); // What direction when blocked?
      if (delta == 1) {             // 1 pixel left or right
        newx         = grain[i].x;  // Cancel X motion
        grain[i].vx /= -2;          // and bounce X velocity (Y is OK)
        newidx       = oldidx;      // No pixel change
      } else if (delta == WIDTH) {  // 1 pixel up or down
        newy         = grain[i].y;  // Cancel Y motion
        grain[i].vy /= -2;          // and bounce Y velocity (X is OK)
        newidx       = oldidx;      // No pixel change
      } else { // Diagonal intersection is more tricky...
        // Try skidding along just one axis of motion if possible (start w/
        // faster axis). Because we've already established that diagonal
        // (both-axis) motion is occurring, moving on either axis alone WILL
        // change the pixel index, no need to check that again.
        if ((abs(grain[i].vx) - abs(grain[i].vy)) >= 0) { // X axis is faster
          newidx = (grain[i].y / 256) * WIDTH + (newx / 256);
          if (!img[newidx]) { // That pixel's free! Take it! But...
            newy         = grain[i].y; // Cancel Y motion
            grain[i].vy /= -2;         // and bounce Y velocity
          } else { // X pixel is taken, so try Y...
            newidx = (newy / 256) * WIDTH + (grain[i].x / 256);
            if (!img[newidx]) { // Pixel is free, take it, but first...
              newx         = grain[i].x; // Cancel X motion
              grain[i].vx /= -2;         // and bounce X velocity
            } else { // Both spots are occupied
              newx         = grain[i].x; // Cancel X & Y motion
              newy         = grain[i].y;
              grain[i].vx /= -2;         // Bounce X & Y velocity
              grain[i].vy /= -2;
              newidx       = oldidx;     // Not moving
            }
          }
        } else { // Y axis is faster, start there
          newidx = (newy / 256) * WIDTH + (grain[i].x / 256);
          if (!img[newidx]) { // Pixel's free! Take it! But...
            newx         = grain[i].x; // Cancel X motion
            grain[i].vy /= -2;         // and bounce X velocity
          } else { // Y pixel is taken, so try X...
            newidx = (grain[i].y / 256) * WIDTH + (newx / 256);
            if (!img[newidx]) { // Pixel is free, take it, but first...
              newy         = grain[i].y; // Cancel Y motion
              grain[i].vy /= -2;         // and bounce Y velocity
            } else { // Both spots are occupied
              newx         = grain[i].x; // Cancel X & Y motion
              newy         = grain[i].y;
              grain[i].vx /= -2;         // Bounce X & Y velocity
              grain[i].vy /= -2;
              newidx       = oldidx;     // Not moving
            }
          }
        }
      }
    }
    
    //Serial.print("Updating grain position...\n\r");
    grain[i].x  = newx; // Update grain position
    grain[i].y  = newy;
    img[oldidx] = 0; // Clear old spot (might be same as new, that's OK)
    img[newidx] = 1; // Set new spot
  }
}

//0   1   2   3   4   5   6   7   8   9   10  11  12  13  
//14  15  16  17  18  19  20  21  22  23  24  25  26  27  
//28  --  30  --  32  --  34  35  --  37  38  39  --  41  
//42  43  44  45  46  47  48  49  50  51  52  53  54  55  
//56  57  58  --  60  61  62  63  --  65  --  67  --  69  
//70  71  72  73  74  75  76  77  78  79  80  81  82  83  
//84  --  86  --  88  --  90  91  --  93  --  95  --  97  
//98  99  100 101 102 103 104 105 106 107 108 109 110 111 
//112 113 114 115 116 117 118 119 120 121 122 123 124 125 
void SAND_GRAINS::roll_the_dice()
{
  uint8_t i = 0;
  uint8_t die1 = 0, die2 = 0; 
  
  // (1) clear all LEDs;
  for (i = 0; i < WIDTH * HEIGHT; i++) { 
    img[i] = 0;
  }
  
  // (2) generate two random numbers between 1..6;
  die1 = random(1, 7); // random numbers between 1..6;
  die2 = random(1, 7);

  // (3) set LEDs to show die1;
  switch (die1) {
  case 1:
    img[59] = 1;
    break;
  case 2:
    img[33] = 1; img[85] = 1;
    break;
  case 3:
    img[33] = 1; img[59] = 1; img[85] = 1;
    break;
  case 4:
    img[29] = 1; img[33] = 1; img[85] = 1; img[89] = 1;
    break;
  case 5:
    img[29] = 1; img[33] = 1; img[59] = 1; img[85] = 1; img[89] = 1;
    break;
  case 6:
    img[29] = 1; img[31] = 1; img[33] = 1; img[85] = 1; img[87] = 1; img[89] = 1;
    break;
  }
  
  // (4) set LEDs to show die2;
  switch (die2) {
  case 1:
    img[66] = 1;
    break;
  case 2:
    img[40] = 1; img[92] = 1;
    break;
  case 3:
    img[40] = 1; img[66] = 1; img[92] = 1;
    break;
  case 4:
    img[36] = 1; img[40] = 1; img[92] = 1; img[96] = 1;
    break;
  case 5:
    img[36] = 1; img[40] = 1; img[66] = 1; img[92] = 1; img[96] = 1;
    break;
  case 6:
    img[36] = 1; img[40] = 1; img[64] = 1; img[68] = 1; img[92] = 1; img[96] = 1;
    break;
  }

  // (5) record we rolled dice; this will prevent to roll dice again once rested;
  set_dice_rolled();
}
