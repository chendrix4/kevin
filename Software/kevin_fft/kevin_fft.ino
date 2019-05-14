#include "arduinoFFT.h"
#include <FastLED.h>

// DSP
#define CHANNEL a0
#define N 64
#define Fs 8000 // > 2*F_max you want to visualize
#define BINS 10

uint16_t bounds_by_freq[BINS-1] = {60, 200, 400, 500, 800, 1200, 1600, 2000, 4000}; // >= bins-1 values
uint16_t bounds_by_index[BINS-1];
double bin_mag_avg[BINS-1];

double vReal[N];
double vImag[N];
double offset;

arduinoFFT FFT = arduinoFFT(vReal, vImag, N, Fs);
unsigned int sampling_period_us;
unsigned long microseconds;


// LED
#define LED_PIN  3
#define COLOR_ORDER GRB
#define CHIPSET     WS2811
#define BRIGHTNESS 64
const uint8_t kMatrixWidth = 10;
const uint8_t kMatrixHeight = 10;
#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
CRGB leds_plus_safety_pixel[ NUM_LEDS + 1];
CRGB* const leds( leds_plus_safety_pixel + 1);


void setup() {

  sampling_period_us = round(1000000*(1.0/Fs));
  for (int i=0; i<BINS-1; i++) {
    bounds_by_index[i] = round(bounds_by_freq[i]*(float)N/Fs);
  }

  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.setBrightness( BRIGHTNESS );

  Serial.begin(115200);

}

void loop() {
  /*SAMPLING*/
    for(int i=0; i<N; i++)
    {
        microseconds = micros();    //Overflows after around 70 minutes!
     
        vReal[i] = analogRead(0);
        offset += vReal[i];
        vImag[i] = 0;
     
        while(micros() < (microseconds + sampling_period_us)){
        }
    }

    offset /= N; // get the signal mean to remove DC
    for(int i=0; i<N; i++)
    {
        vReal[i] -= offset; 
    }
     
    /*FFT*/
    unsigned long t0 = millis();
    FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(FFT_FORWARD);
    FFT.ComplexToMagnitude();
    //double peak = FFT.MajorPeak();
    //Serial.println(peak);     //Print out what frequency is the most dominant.

    vReal[0] = 0.0;
    for (int i=0; i<BINS-1; i++) {

        double bin_mag = 0.0;
        uint8_t bin_size;
        if (i == 0) {
            bin_size = bounds_by_index[i];
        } else {
            bin_size = bounds_by_index[i] - bounds_by_index[i-1];
        }
        for (int j=bounds_by_index[i]-bin_size; j < bounds_by_index[i]; j++) {
            bin_mag += vReal[j];
        }
        bin_mag_avg[i] = bin_mag/bin_size;
        
    }
    //Serial.println(millis()-t0);

    DrawOneFrame();
    FastLED.show();

}

void DrawOneFrame()
{
  for( byte y = 0; y < kMatrixHeight; y++) {
    for( byte x = 0; x < kMatrixWidth; x++) {
      if ( x > bin_mag_avg[y]/(70-(5*y))){
          leds[ XY(x, y)]  = CRGB::Black;
      } else {
          leds[ XY(x, y)]  = CHSV( 0x53+x*20, 255, 255);
      }
    }
  }
}

uint16_t XY( uint8_t x, uint8_t y)
{
  uint16_t i;
  if( y & 0x01) {
    // Odd rows run backwards
    uint8_t reverseX = (kMatrixWidth - 1) - x;
    i = (y * kMatrixWidth) + reverseX;
  } else {
    // Even rows run forwards
    i = (y * kMatrixWidth) + x;
  }
  return i;
}
