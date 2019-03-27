#include <FastLED.h>

// ADC
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))


// DSP
#define LIN_OUT 1
#define CHANNEL a0
#define FHT_N 128
#define BINS 10
static const double Fs = 14000;
#include <FHT.h>

static uint16_t bounds_by_freq[BINS-1] = {110, 219, 438, 656, 984, 1313, 1750, 2188, 2625};
                                       //{65, 131, 262, 523, 1047, 2093, 4186}
uint16_t bounds_by_index[BINS+1] = {0,1,2,4,6,9,12,16,20,24,FHT_N/2-1};
double bin_mag_avg[BINS];
double bin_size_scalar[BINS] = {1,1,1,1,1,1,1,1,1,1};

unsigned int sampling_period_us;
unsigned long microseconds;

// HPF with EMA to remove DC component and HF noise
// cutoff = Fs/(2pi) * arccos(1 - a^2/2(1-a))
// We want to cutoff < 10Hz
#define PHI 2*PI/Fs
static const double alpha = sqrt(sq(cos(PHI*10)) - 4*cos(PHI*10) + 3) + cos(PHI*10) - 1;
int16_t k_raw, k_filtered, k_max=0;


// LED
#define LED_PIN  3
#define COLOR_ORDER GRB
#define CHIPSET     WS2811
#define BRIGHTNESS 255

const uint8_t kMatrixWidth = 10;
const uint8_t kMatrixHeight = 10;
#define NUM_LEDS (kMatrixWidth * kMatrixHeight)

CRGB leds_plus_safety_pixel[ NUM_LEDS + 1];
CRGB* const leds( leds_plus_safety_pixel + 1);


void setup() {

  Serial.begin(115200);

  sampling_period_us = round(1000000*(1.0/Fs));
  //bounds_by_index[0] = 0;
  for (int i=1; i<BINS+1; i++) {
  //  bounds_by_index[i] = round(bounds_by_freq[i]*(float)FHT_N/Fs);
    bin_mag_avg[i] = 0;    
  }
  //bounds_by_index[BINS] = FHT_N/2 - 1;

  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);

  for (uint8_t br = 0; br < BRIGHTNESS; br++) {
      FastLED.setBrightness( br );
      fill_solid(leds, NUM_LEDS, CRGB::White);
      FastLED.show();
      delay(1);
  }

  for (uint8_t i = 0; i < kMatrixWidth + kMatrixHeight; i > 0;i--) {
    for (uint8_t y = kMatrixHeight-1 ; y > kMatrixHeight-i ; y--) {
      for (uint8_t x = kMatrixWidth-i ; x < kMatrixWidth ; x++) {
        Serial.print(x); Serial.print(","); Serial.print(y); Serial.print(" ");
        leds[ XY(x,y) ] = CRGB::Black;
        FastLED.show();
      }
    }
    Serial.print("\n---\n");
  }

  // set the ADC clock speed to 1 Mhz
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  cbi(ADCSRA, ADPS0);

  //  TIMSK0 = 0; // turn off timer0 for lower jitter
  ADCSRA = 0xe5;  // set the adc to free running mode
  ADMUX = 0x40;   // use adc0
  DIDR0 = 0x01;   // turn off the digital input for adc0

  k_filtered = sampleADC();

}

void loop() {

    /*SAMPLING*/
    //cli();  // UDRE interrupt slows this way down on arduino1.0
    //t0 = micros();
    for (int i = 0 ; i < FHT_N ; i++) {
      k_raw = sampleADC();
      k_filtered = alpha*k_raw + ((1 - alpha)*k_filtered);
      fht_input[i] = k_raw - k_filtered;   // Apply BPF
      //Serial.print(fht_input[i]); Serial.print(" ");
      k_max = max(abs(fht_input[i]), k_max);        // pause display when music not playing
    }

    //Serial.println();

    if (k_max > 1000) {

      //_sampling = micros()-t0;
      //Serial.print("Sample time = "); Serial.println(_sampling/1000);
     
      /*FFT*/
      fht_window();             // window the data for better frequency response
      fht_reorder();            // reorder the data before doing the fht
      fht_run();                // process the data in the fht
      fht_mag_lin();            // take the output of the fht
      //sei();
      //_fht = micros()-t0;
      //Serial.print("FHT time = "); Serial.println(_fht/1000);

      /* BINNING */
      for (int i=0 ; i<BINS ; i++) {
    
        double bin_mag = 0.0;
        uint8_t bin_size;
        bin_size = bounds_by_index[i+1] - bounds_by_index[i];
        for (int j=bounds_by_index[i]; j < bounds_by_index[i+1]; j++) {
          bin_mag += fht_lin_out[j];
        }
        bin_mag_avg[i] = bin_size_scalar[i]*bin_mag/bin_size;
      
      }
      //_binning = micros()-t0;
      //Serial.print("Binning time = "); Serial.println(_binning/1000);

      /* DISPLAY */
      drawOneFrame();
      FastLED.show();
      //_display = micros()-t0;
      //Serial.print("Display time = "); Serial.println(_display/1000);

      //Serial.println();

    }

    else {
      // TODO: IDLE BEHAVIOR
    }
}

int16_t sampleADC() {
  
  microseconds = micros();

  // wait for adc to be ready
  while(!(ADCSRA & 0x10));

  // time the sample
  while(micros() < (microseconds + sampling_period_us));

  ADCSRA = 0xf5;              // restart adc
  byte m = ADCL;              // fetch adc data
  byte j = ADCH;
  int k = (j << 8) | m;       // form into an int
  k -= 0x0200;                // form into a signed int
  k <<= 6;                    // form into a 16b signed int

  return k;
  
}

void drawOneFrame() {
  static float hue = 0;
  hue += 0.2;
  uint8_t thresh;
  
  for( uint8_t y = 0; y < kMatrixHeight; y++) {
    thresh = bin_mag_avg[y]/(70-(5*y));
    for( uint8_t x = 0; x < kMatrixWidth; x++) {
      if ( x > thresh){
        leds[ XY(x,y) ] = CRGB::Black;
      } else {
        //leds[ XY(x,y) ] = CHSV( 0x53+x*20, 255, 255);
        leds[ XY(x,y) ] = CHSV( hue+y*10, 255, 255);
        //if (thresh > 7) {
        //  leds[ XY(x,y) ] = CRGB::Red;
        //} else if (thresh > 3) {
        //  leds[ XY(x,y) ] = CRGB::Purple;
        //} else {
        //  leds[ XY(x,y) ] = CRGB::Blue;
        //}
      }
    }
  }

}

uint16_t XY( uint8_t x, uint8_t y) {

  // Cartesian x,y --> led string i

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
