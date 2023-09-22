#include "uop_msb.h"
#include <cstdint>
#include <cstdio>
#include <math.h>
#include "USBMouse.h"
#include "LEDMatrix.hpp"
using namespace uop_msb;

BalanceTable table;

//PROTOTYPES
double roll (float x, float y, float z);
double pitch (float x, float y, float z);
double Zoffset (float x, float y, float z);
int mapInt(double in, float inMin, float inMax, float outMin, float outMax);

//GLOBAL VARS
float x_angles[100]; 
float y_angles[100];
float z_angles[100];
float potVals[100];
//inputs to be used
double X_angle = 0;
double Y_angle  = 0;
double Z_angle = 0;
double potVal;

float reset_x = 0;
float reset_y = 0;
float resetted_x;
float resetted_y;
int LEDx;
int LEDy;
int16_t mousex;
int16_t mousey;
float XYZ_max; //can be increased up to pi/2. increasing this decreases sensitivity.
int buzz_interval = 1000;
int buzz_on = 0;


//input devices
MotionSensor motion;
DigitalIn BlueButton(USER_BUTTON);
USBMouse mouse(true, ABS_MOUSE);
AnalogIn pot(AN_POT_PIN); 
LCD_16X2_DISPLAY disp;
Buzzer buzz;

TimerCompat avgtmr; 
TimerCompat debouncetmr;
TimerCompat lcdtmr;
TimerCompat buzztmr;

int main (void)
{
    avgtmr.start();
    lcdtmr.start();
    buzztmr.start();
    uint32_t x_origin = (X_MAX_ABS - X_MIN_ABS)/2;  //Middle X-Coordinate
    uint32_t y_origin = (Y_MAX_ABS - Y_MIN_ABS)/2;  //Middle Y-Coordinate
    uint32_t radius = 5000;                         //Radius of path           

    //innitialize the first 100 values
    for (int i = 0; i < 100; i++)
    {
        Motion_t acc = motion.getAcceleration();
        x_angles[i] = roll(acc.x, acc.y, acc.z);
        y_angles[i] = pitch(acc.x, acc.y, acc.z);
        z_angles[i] = Zoffset(acc.x, acc.y, acc.z);
        potVals[i] = pot.read();
        wait_us(50000);
    }

    while (true)
    {
        
        if (avgtmr.read_ms() >= 2)
        {
            X_angle = 0;
            Y_angle = 0;
            Z_angle = 0;
            potVal = 0;
            Motion_t acc = motion.getAcceleration();   //get the input from motion sensor
            
            //shifts array of inputs
            for (int i = 0; i < 99; i++) 
            {
                x_angles[i] = x_angles[i+1];
                y_angles[i] = y_angles[i+1];
                z_angles[i] = z_angles[i+1];
                potVals[i] = potVals[i+1];
            }

            //insert new input as last value. 
            x_angles[99] = roll(acc.x, acc.y, acc.z);
            y_angles[99] = pitch(acc.x, acc.y, acc.z);
            z_angles[99] = Zoffset(acc.x, acc.y, acc.z);
            potVals[99] = pot.read();

            //calculate average
            for (int i = 0; i < 100; i++)
            {
                X_angle += x_angles[i]/100.0;
                Y_angle += y_angles[i]/100.0;
                Z_angle += z_angles[i]/100.0;
                potVal += potVals[i]/100.0;
            }
            
            XYZ_max = potVal * 1.57;    //change sensitivity
            
            buzz_interval = mapInt(abs(Z_angle), 0, 1.5, 50, 1000);

            //set the posision on led matrix
            resetted_x = X_angle - reset_x;
            resetted_y = Y_angle - reset_y;
            LEDx = round(mapInt(resetted_x, -XYZ_max, XYZ_max,  -7, 7));
            LEDy = round(mapInt(resetted_y, -XYZ_max, XYZ_max, -3, 3));
            table.setXYoffset(-LEDx, -LEDy);

            //mouse movement 
            mousex = round(mapInt(X_angle, -XYZ_max, XYZ_max, x_origin-radius, x_origin+radius));
            mousey = round(mapInt(Y_angle, -XYZ_max, XYZ_max, y_origin-radius, y_origin+radius));     
            mouse.move(mousex, mousey);    
            
            //reset the timers
            avgtmr.reset();
            avgtmr.start();                
        }

        //reset the bubble
        if (BlueButton)
        {
            debouncetmr.start();
            if (debouncetmr.read_ms() >= 20) //debounce
            {
                debouncetmr.reset();
                Motion_t acc = motion.getAcceleration();   

                reset_x = roll(acc.x, acc.y, acc.z);
                reset_y = pitch(acc.x, acc.y, acc.z);

                while(!BlueButton);
                debouncetmr.start();
                while(debouncetmr.read_ms() <= 20);  //debounce
            }
            
        }

        if (lcdtmr.read_ms() >= 1000)   //for printing angles to the output every second.
        {           
            disp.locate(0, 0);
            disp.printf("X angle: %5.4f", X_angle);
            disp.locate(1, 0);
            disp.printf("Y angle: %5.4f", Y_angle);

            lcdtmr.reset();
            lcdtmr.start();

            //debugging
            //printf("Z_angle: %f | buzz interval: %d \n", Z_angle, buzz_interval );
        }

        if (Z_angle <= 0.5){
            if (buzztmr.read_ms() >= buzz_interval)
            {
                buzztmr.reset();
                if (buzz_on == 1)
                {
                    buzz_on = 0;
                    buzz.rest();
                } else {
                    buzz_on = 1;
                    buzz.playTone("A");
                }
                buzztmr.start();
            }
        } else {
            buzz.rest();
        }
        

    }
}



//functions to calculate the angles.
double roll (float x, float y, float z)
{
    return atan(x /pow( (pow(y, 2) + pow(z, 2)) , 0.5) );
}

double pitch (float x, float y, float z)
{
    return atan(y /pow( (pow(x, 2) + pow(z, 2)) , 0.5) );
}

double Zoffset (float x, float y, float z)
{
    return atan(pow( (pow(y, 2) + pow(x, 2)) , 0.5)/z );
}

int mapInt(double in, float inMin, float inMax, float outMin, float outMax) {
  // check it's within the range
  if (inMin<inMax) { 
    if (in <= inMin) 
      return outMin;
    if (in >= inMax)
      return outMax;
  } 

  // calculate how far into the range we are
  float scale = (in-inMin)/(inMax-inMin);
  // calculate the output.
  return outMin + scale*(outMax-outMin);
}
