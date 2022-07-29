//important resources:
//https://tcd1304.wordpress.com/  Particularly important is the timings in page 7-8 of the datasheet in specifications.
//https://www.pjrc.com/teensy/IMXRT1060RM_rev2.pdf  Main manual to derive meaning in set_upflexpwm.h
//https://www.nxp.com/docs/en/application-note/AN4319.pdf Bonus manual that explains how PWM works in the register level, has graphical representations. 


/*
  TCD1304      - Teensy 4.0
  ------------------------
  pin 3 (ICG)  - pin 5
  pin 4 (MCLK) - pin 6
  pin 5 (SH)   - pin 4
*/

//#include <Ethernet3.h>
//#include <EthernetUdp3.h>
#include <ADC.h>
#include <ADC_util.h>
#include <SPI.h>
double Timer;
#define TimerInterval 300
#define PIXELS 3648
#define MCLK 3.2 // target TCD1304 clock rate in MHz; we will get as close as possible. Lowering this value too much from 4MHz causes the ccd to lose resolution
// frame refers to one full set of data including dummy datas(3694 pixels)
// calculate counter values such that an integer number of both 128x divided fbus cycles and MCLK cycles happen during each frame. syntax(Bus Clock per MasterClock---> CLKPMCLK)
int CLKPMCLK = ceil((float)F_BUS_ACTUAL / (MCLK * 1000000)); //38 f bus clock cycles per ccd clock cycles
int MCLKPF = ceil((float)(32 + PIXELS + 14) * 4 / 128); //116 Mclocks per PWM. note 128 scales it down by 128, multiply it and it gives Mclocks per ICG. number of ccd clock cycles per frame
int CNTPF = MCLKPF * CLKPMCLK; //4408,  the number of bus clock ticks per shift gate PWM before scaling of ES
int CLKPF = 128 * CNTPF; //564224 bus clock ticks per frame (also the clock ticks per ICG)
int USPF = CLKPF / (F_BUS_ACTUAL / 1000000); // 3761 microseconds per frame
// make sure that 6 + 1 + 0.5 > 128*116 - (32+PIXELS+14)*4 i.e. there's enough extra time between pixels to set ICG low then high. Below are all in multiples of clock periods(1/150Mhz) 
int CNT_ICG = (6.0 * (F_BUS_ACTUAL / 1000000)) / 128; //7.03, this value is scaled down by 128 times cuz it'll get scaled up in ICG.
int ES = 128; // how many times to divide framerate for electronic shutter [1 2 4 8 16 32 64 128] (ES=128 will make 128 PWM cycles per ICG/frame. 64 = 64 PWM cycles per ICG frame. can be seen has lower ES, larger the period of SH and integration time)
// The logic for ES works fine but TCD does not work properly( way lower resolution for any values lower than 128.)
int CNT_SH = (1.0 * (F_BUS_ACTUAL / 1000000)) / (128 / ES); //150 divide by whatever prescaler to set shorter integration time. t3
int off = (0.5 * (F_BUS_ACTUAL / 1000000)) / (128 / ES); // 75 delay time between ICG low and SH high. t2
int adc_off = (0.0 * (F_BUS_ACTUAL / 1000000)); // delay time between ICG high and ADC sample start t4
// for now the MCLK starts totally synchronized with ICG high edge but we could add a delay there too (datasheet says 0 delay is ok). I am not sure if I should change the polarity of the clock (do we want a rising or falling edge first?)

#include "setup_flexpwm.h"

ADC *adc; // adc object

int i = 0;
int ii = 0;

int s = 1;
uint16_t vals[1][32 + PIXELS];
byte vals2[1][32 + PIXELS];
int j = 0;
int f = 0;

uint16_t sample = 0;
uint16_t sample_prev = 0;

void flexpwm2_1_isr() { // isr = interrupt service routine. guessing this is called whenever a reset condition occurs
  // resets the pixel count before ICG goes high to signal new frame
  FLEXPWM2_SM1STS = 1;  // clear interrupt flag for sm 0.  This directly accesses status reigester bus. The syntax is PWMWave_CtrlRegister
  while (FLEXPWM2_SM1STS == 1); // wait for clear
  i = 0;
  ii = 0;
}

void flexpwm2_3_isr() { //
  FLEXPWM2_SM3STS |= 12;  // clear all set bits. |= is a bit operand. Note 12 is converted to binary 0b1100

  if ((i >= 0) && (i < (32 + PIXELS + 14))) {
    // do a fast read of adc here!
    // wait for conversion and sample ccd
    sample_prev = sample;
    while (!adc->adc0->isComplete()); // wait for conversion. isComplete() is true, then this loop is true and loop runs again.
    sample = (uint16_t)adc->adc0->analogReadContinuous(); // The symbol ->, is equivalent to derefencing a pointer followed by a . So code is a dereferenced adc.adc0.isComplete()

    if ((i >= 32) && (i < 32 + PIXELS)) {
      vals[j][i-32] = sample; // so that 1st data is stored in vals[0][0]
    }
  }
  i += 1;
}

void setup() {
  // set up adc here? for some reason ADC setup and interrupt attaching need to be last! otherwise the adc reading was super noisy...
  adc = new ADC();
  adc->adc0->setAveraging(1);
  adc->adc0->setResolution(12);
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_HIGH_SPEED);
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED);
  adc->adc0->startContinuous(A9);

  pinMode(7, OUTPUT);

  Serial.begin(115200);
  //  Serial.println(F_BUS_ACTUAL);
  //  Serial.println(CLKPMCLK);
  //  Serial.println(USPF);
  //  Serial.println(CLKPF);
  //  Serial.println(CNTPF);
  //  Serial.println(CNT_ICG);
  //  Serial.println(CNT_SH);
  //  Serial.println(off);
  //Serial.print(F_BUS_ACTUAL);
  // set up clocks
  CCM_CCGR4 |= CCM_CCGR4_PWM2(CCM_CCGR_ON);

  //FLEXPWM2_MCTRL = 0; // make sure RUN flags are off
  FLEXPWM2_FCTRL0 = FLEXPWM_FCTRL0_FLVL(15); // logic high = fault
  FLEXPWM2_FSTS0 = 0x000F; // clear fault status
  FLEXPWM2_FFILT0 = 0;
  FLEXPWM2_MCTRL |= FLEXPWM_MCTRL_CLDOK(15);

  setup_ICG();
  setup_SH();
  setup_MCLK();
  setup_ADCCLK();

  //Serial.print("FLEXPWM2_MCTRL: ");
  //Serial.println(FLEXPWM2_MCTRL);

  //FLEXPWM2_SM2CTRL2 |= 64; // force initialization ICG
  //FLEXPWM2_SM0CTRL2 |= 64; // force initialization SH
  // not actually initializing as expected..


  FLEXPWM2_MCTRL |= FLEXPWM_MCTRL_LDOK(15); // do counters initialize upon loading?
  //FLEXPWM2_MCTRL |= FLEXPWM_MCTRL_RUN(15); // why is this not necessary?

  //attachInterrupt(3, beginCCDsampler, RISING);

  

  // attach interrupts . Interrupts are good for making things happen automatically, and solves timing problems.
  // If we want a chunk of code to ignore interrupts, use noInterrupts() to disable, and then Interrupts() to re-enable.
  attachInterruptVector(IRQ_FLEXPWM2_1, flexpwm2_1_isr);
  NVIC_ENABLE_IRQ(IRQ_FLEXPWM2_1);

  attachInterruptVector(IRQ_FLEXPWM2_3, flexpwm2_3_isr);
  NVIC_ENABLE_IRQ(IRQ_FLEXPWM2_3);
  NVIC_SET_PRIORITY(IRQ_FLEXPWM2_3, 128);

}

/*
   teensy 4 seems to have ADC noise issues while a packet is being
   sent to the Ethernet board. I do not think these issues showed
   up on the 3.6. It seems like noise can be minimized by keeping
   the analog read pin far from the SPI clock pin (and presumably
   other SPI pins) as well as keeping the SPI clock slow (setting
   in W5500.h; I think 30MHz is a good speed since it takes almost
   a whole frame to transfer one frame at 4MHz CCD clock rate).
   Presumably, noise will also improve if all signal traces are kept
   as short as possible

*/

void loop() {
  if (millis() > (Timer + TimerInterval)){ 
    if (true){
      f = 0;
      for (int id2 = 1; id2 <= 10; id2++) { // 10 255 data points to differentiate between sets of 3648 data. This is useless in matlab, but helps to view trend in arduino's serial monitor  
        Serial.println(255);
      }
        Serial.println(">>");
      for (int id1 = 0; id1 < PIXELS; id1 = id1 + 1) { //first data point starts at 32. id1 are central values, 1d3 are range of plusminus values around id1
        
          float AverageValue = 0; //Averaging is used to artificially reduce noise. 
          int MeasurementsToAverage = 31; // Use odd numbers to ensure plusminus is an integer. otherwise lead to problem in averaging. Larger this value, smaller the noise. Tradeoff is lower resolution.
          int plusminus = (MeasurementsToAverage-1)/2;
          if (id1>plusminus && id1<PIXELS-plusminus){  // default case
            for(int id3 = id1-plusminus; id3 <= id1+plusminus; ++id3) // taking left and right +-numbers to average
              { 
                AverageValue += vals[0][id3];  
              }
          }
          else if (id1<= plusminus){          //case of extreme left values. Note that the logic for calculating ends is not perfect, but testing with actual data points shows no major issues.
              for(int id3 = id1; id3<id1+MeasurementsToAverage; ++id3){
                AverageValue += vals[0][id3];
              }
          } 
          else if (id1 >= PIXELS-plusminus){ // case of extreme right values
            for(int id3 = id1-MeasurementsToAverage+1; id3<=id1; ++id3){
              AverageValue += vals[0][id3];
            }         
          }
          
         AverageValue /= MeasurementsToAverage;      
         Serial.println(AverageValue);    //each average value is plotted.
         //  dataPoint = vals[0][idl];
        //   Serial.println(dataPoint);
      }
    }
  Timer = millis();
  }
}
