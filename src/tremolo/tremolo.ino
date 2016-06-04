// Licensed under a Creative Commons Attribution 3.0 Unported License.
// Based on rcarduino.blogspot.com previous work.
// www.electrosmash.com/pedalshield
 
// sinewave.ino program creates a sinusoidal waveform adjustable in amplitude and frequency.
// potentiometer 0 controls the frequency.
// potentiometer 2 controls the amplitude.
 
int in_ADC0, in_ADC1;  //variables for 2 ADCs values (ADC0, ADC1)
int POT0, POT1, POT2, out_DAC0, out_DAC1; //variables for 3 pots (ADC8, ADC9, ADC10)
const int LED = 3;
const int FOOTSWITCH = 7; 
const int TOGGLE = 2;
int toggle_value = 0;
int effect = 0;
int accumulator, sample, min_ampl, max_ampl;
uint16_t sine_ampl;
uint16_t ampl;
uint32_t max_sample;
 
// Create a table to hold pre computed sinewave, the table has a resolution of 600 samples
#define no_samples 44100         // 44100 samples at 44.1KHz takes 1 second to be read. 
uint16_t nSineTable[no_samples]; // storing 12 bit samples in 16 bit variable.
 
void createSineTable()
{
  int sine_max = 0;
  for(uint32_t nIndex=0; nIndex<no_samples; nIndex++)
  {
    // normalised to 12 bit range 0-4095
    sine_ampl = (uint16_t)  (((1+sin(((2.0*PI)/no_samples)*nIndex))*4095.0)/2);
    nSineTable[nIndex] = sine_ampl;
    if (sine_ampl>sine_max) sine_max = sine_ampl;
  }
  Serial.print("Sine Max: ");
  Serial.println(sine_max);
}
 
void setup()
{
  Serial.begin(250000);
  createSineTable();

  min_ampl = max_ampl = 0;
 
  //turn on the timer clock in the power management controller
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk(ID_TC4);
 
  //we want wavesel 01 with RC 
  TC_Configure(TC1,1, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | TC_CMR_TCCLKS_TIMER_CLOCK2);
  TC_SetRC(TC1, 1, 238); // sets <> 44.1 Khz interrupt rate
  TC_Start(TC1, 1);
 
  //enable timer interrupts on the timer
  TC1->TC_CHANNEL[1].TC_IER=TC_IER_CPCS;
  TC1->TC_CHANNEL[1].TC_IDR=~TC_IER_CPCS;
 
  //Enable the interrupt in the nested vector interrupt controller 
  //TC4_IRQn where 4 is the timer number * timer channels (3) + the channel number 
  //(=(1*3)+1) for timer1 channel1 
  NVIC_EnableIRQ(TC4_IRQn);
 
  //ADC Configuration
  ADC->ADC_MR |= 0x80;   // DAC in free running mode.
  ADC->ADC_CR=2;         // Starts ADC conversion.
  ADC->ADC_CHER=0x1CC0;  // Enable ADC channels 0,1,8,9 and 10  
 
  //DAC Configuration
  analogWrite(DAC0,0);  // Enables DAC0
  analogWrite(DAC1,0);  // Enables DAC1

  pinMode(TOGGLE, INPUT_PULLUP);
  attachInterrupt(TOGGLE, switch_handler, CHANGE);

  Serial.println("Setup Complete");
}
 
void loop()
{
  //Read the ADCs.
  while((ADC->ADC_ISR & 0x1CC0)!=0x1CC0); // wait for ADC 0, 1, 8, 9, 10 conversion complete.
  in_ADC0=ADC->ADC_CDR[7];            // read data from ADC0
  in_ADC1=ADC->ADC_CDR[6];            // read data from ADC1  
  POT0=ADC->ADC_CDR[10];                  // read data from ADC8        
  POT1=ADC->ADC_CDR[11];                  // read data from ADC9   
  POT2=ADC->ADC_CDR[12];                  // read data from ADC10
  Serial.print("Effect: ");
  Serial.print(effect);
  Serial.print("  Pot0: ");
  Serial.print(POT0);
  Serial.print(" >>3: ");
  Serial.print(POT0>>3);
  Serial.print("  pot1: ");
  Serial.print(POT1);
  Serial.print("  pot2: ");
  Serial.print(POT2);
  Serial.print("  min: ");
  Serial.print(min_ampl);
  Serial.print("  max: ");
  Serial.print(max_ampl);
  Serial.print("  Max Sample: ");
  Serial.println(max_sample);
}
 
//Interrupt at 44.1KHz rate (every 22.6us)
void TC4_Handler()
{
  
  //Clear status allowing the interrupt to be fired again.
  TC_GetStatus(TC1, 1);
 
  //update the accumulator, from 0 to 511
  accumulator=POT0>>3;
     
  //calculate the sample
  sample=sample+accumulator;
  sample = sample % no_samples;
  //if(sample>=no_samples) sample=0;
  
  if (sample > max_sample) max_sample = sample; 
   
  if (effect==0) {
    // Sine Wave
    
    //Generate the DAC output
    ampl = (uint16_t) nSineTable[sample];
  }
  else if (effect==1) {
    // Square Wave
    
    if (sample>2047) {
      ampl = 0;
    }
    else {
      ampl = 4095;
    }
   
  }
  else if (effect==2) {

    if (sample <= 22049) {
      ampl = (uint16_t) (sample/22050.0 * 4095.0);
    }
    else {
      ampl = (uint16_t) (4095 - ( sample-22050 / 22050.0 * 4095.0) );
    }

  }
  else {
    // Sawtooth Wave

     ampl = (uint16_t) (sample/float(no_samples) * 4095.0);

  }

if(in_ADC0-in_ADC1 > POT1){
  out_DAC0 = ampl;
  out_DAC1 = 4095 - ampl;
}
else {
  out_DAC0 = 2048;
  out_DAC1 = 2048;
  
 
}



  
  /*
  out_DAC0 = map(ampl*in_ADC0, 0, 16777215, 0, 4095);
  out_DAC1 = 4095 - map(ampl*in_ADC1, 0, 16777215, 0, 4095);
  */
  

  if (ampl > max_ampl) {
    max_ampl = ampl;
  }

  if (ampl < min_ampl) {
    min_ampl = ampl;
  }
  
  //Add volume feature
  out_DAC0=map(out_DAC0,0,4095,0,POT2);
  
  out_DAC1=map(out_DAC1,0,4095,0,POT2);
 
  //Write the DACs.
  dacc_set_channel_selection(DACC_INTERFACE, 0);       //select DAC channel 0
  dacc_write_conversion_data(DACC_INTERFACE, out_DAC0);//write on DAC
  dacc_set_channel_selection(DACC_INTERFACE, 1);       //select DAC channel 1
  dacc_write_conversion_data(DACC_INTERFACE, out_DAC1);//write on DAC 
}

void switch_handler()
{
  delayMicroseconds(100000); //debouncing protection
  if (toggle_value!=digitalRead(TOGGLE)) {
    max_ampl = 0;
    min_ampl = 0;
    max_sample = 0;
    effect++;
  }
  delayMicroseconds(100000); //debouncing protection
  toggle_value = digitalRead(TOGGLE);
  if (effect==4) effect=0;
}

