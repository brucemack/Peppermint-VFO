// BITX40 VFO
// Bruce MacKinnon KC1FSZ 22-March-2017
//
#include "si5351.h"
#include "Wire.h"
#include <SPI.h>
#include <Adafruit_SSD1306.h>

#include <ClickEncoder.h>
#include <TimerOne.h>

Adafruit_SSD1306 display(4);

Si5351 si5351;

ClickEncoder encoder(3,2,4,4);

void serviceCb() {
  encoder.service();
}

// The frequency that is displayed 
unsigned long displayFreq = 7200000L;
// The IF frequency 
unsigned long ifFreq = 12000000L;
// The adjustment
unsigned long adjFreq = 1600L;

// ----- FREQUENCY STEP MENU ---------------------------------------------------------------
const unsigned int freqStepMenuSize = 7;

const long long freqStepMenu[freqStepMenuSize] = {
  1L,
  10L,
  100L,
  500L,
  1000L,
  10000L,
  100000L
};

const char* freqStepMenuText[freqStepMenuSize] = {
  "1Hz",
  "10Hz",
  "100Hz",
  "500Hz",
  "1kHz",
  "10kHz",
  "100kHz"
};

// This is the current step that is being used
unsigned int freqStepIndex = 3;

// ----- MODES ---------------------------------------------------------------

const unsigned int modeMenuSize = 2;

const char* modeMenuText[] = {
  "VFO",
  "CAL"
};

unsigned int mode = 0;
unsigned int subMode = 0;

bool displayDirty = true;
unsigned long lastDisplayStamp = 0;

void setup() {
  
  Serial.begin(9600);
 
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);  

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  // initialize with the I2C addr (for the 128x64)
  display.begin(SSD1306_SWITCHCAPVCC,0x3C);
  display.clearDisplay();

  Timer1.initialize(1000);
  Timer1.attachInterrupt(serviceCb); 

  setFreq(displayFreq);

  pinMode(13,OUTPUT);
}

void updateDisplayFreq(unsigned long freq) {
  unsigned long f = freq;
  unsigned long f0 = f / 1000000;
  unsigned long f1 = (f - (f0 * 1000000)) / 1000;
  unsigned long f2 = f - f0 * 1000000 - f1 * 1000;
  display.setCursor(10,20);
  display.print(f0);
  display.setCursor(30,20);
  display.print(f1);
  display.setCursor(70,20);
  display.print(f2);
}

void updateDisplay() {

  display.clearDisplay();
  display.setTextColor(WHITE);

  // Top line - logo and mode
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("KC1FSZ VFO 3");
  display.setCursor(100,0);
  display.print(modeMenuText[mode]);

  if (mode == 0) { 

    // Frequency line 
    display.setTextSize(2);
    updateDisplayFreq(displayFreq);

    // Second line is the step size
    display.setTextSize(1);
    display.setCursor(5,43);
    display.print(freqStepMenuText[freqStepIndex]);

    // Show the marker
    if (subMode == 0) {
      display.drawPixel(0,20,WHITE);
      display.drawPixel(0,21,WHITE);
      display.drawPixel(0,22,WHITE);
      display.drawPixel(0,23,WHITE);
    } else if (subMode == 1) {
      display.drawPixel(0,43,WHITE);
      display.drawPixel(0,44,WHITE);
      display.drawPixel(0,45,WHITE);
      display.drawPixel(0,46,WHITE);
    }
  }
  else if (mode == 1) {

    // Adjustment line
    display.setTextSize(2);
    updateDisplayFreq(adjFreq);    

    // Second line is the step size
    display.setTextSize(1);
    display.setCursor(5,43);
    display.print(freqStepMenuText[freqStepIndex]);

    // Show the marker
    if (subMode == 0) {
      display.drawPixel(0,20,WHITE);
      display.drawPixel(0,21,WHITE);
      display.drawPixel(0,22,WHITE);
      display.drawPixel(0,23,WHITE);
    } else if (subMode == 1) {
      display.drawPixel(0,43,WHITE);
      display.drawPixel(0,44,WHITE);
      display.drawPixel(0,45,WHITE);
      display.drawPixel(0,46,WHITE);
    }
  }
     
  display.display();
}

void setFreq(unsigned long displayFreq) {
  unsigned long a = ifFreq - (displayFreq + adjFreq);
  si5351.set_freq(a * 100,SI5351_PLL_FIXED, SI5351_CLK0);
}

byte highNibble(byte b) {
  return (b >> 4) & 0x0f;
}

byte lowNibble(byte b) {
  return b & 0x0f;
}

void processCATCommand(byte* cmd) {
  // Set frequency
  if (cmd[4] == 1) {
    // Pull off each of the digits
    byte d7 = highNibble(cmd[0]);
    byte d6 = lowNibble(cmd[0]);
    byte d5 = highNibble(cmd[1]);
    byte d4 = lowNibble(cmd[1]); 
    byte d3 = highNibble(cmd[2]);
    byte d2 = lowNibble(cmd[2]); 
    byte d1 = highNibble(cmd[3]);
    byte d0 = lowNibble(cmd[3]); 
    unsigned long freq = 
      (unsigned long)d7 * 100000000L +
      (unsigned long)d6 * 10000000L +
      (unsigned long)d5 * 1000000L + 
      (unsigned long)d4 * 100000L + 
      (unsigned long)d3 * 10000L + 
      (unsigned long)d2 * 0000L + 
      (unsigned long)d1 * 100L + 
      (unsigned long)d0 * 10L; 
    setFreq(freq);       
  }
  else if (cmd[4] == 8) {
    digitalWrite(13,HIGH);
  }
  else if (cmd[4] == 88) {
    digitalWrite(13,LOW);
  }
}


long lastSerialReadStamp = 0;
byte cmdBuf[5];
int cmdBufPtr = 0;

void loop() {

  // Serial (CAT) interface
  if (Serial.available()) {
    if (millis() - lastSerialReadStamp > 500) {
      cmdBufPtr = 0;
    }
    cmdBuf[cmdBufPtr++] = Serial.read();
    // Check to see if we have a full read
    if (cmdBufPtr == 5) {
      processCATCommand(cmdBuf);
    }
  }
  
   // TODO: CONSIDER TIMER CONTROL
  // Periodic display update (if needed)
  if (displayDirty && millis() > lastDisplayStamp + 100) {
    lastDisplayStamp = millis();    
    displayDirty = false;
    updateDisplay();
  }

  // Sample the encoder and de-bounce the result
  int16_t value = encoder.getValue();
  ClickEncoder::Button button = encoder.getButton();

  if (mode == 0) {
    
    if (subMode == 0) {
      // Adjust the frequency by the selected step size
      if (value != 0) {
        displayFreq -= (unsigned long long)value * freqStepMenu[freqStepIndex];
        displayDirty = true;
        setFreq(displayFreq);
      }
    } else if (subMode == 1) {
      if (value != 0) {
        int newIndex = (int)freqStepIndex - value;
        if (newIndex < 0) {
          newIndex = 0;
        } else if ((unsigned int)newIndex >= freqStepMenuSize) {
          newIndex = freqStepMenuSize - 1;
        }
        freqStepIndex = newIndex;
        displayDirty = true;
      }    
    }
  
    if (button == 5) {
      displayDirty = true;
      if (subMode == 0) {
        subMode = 1;
      } else {
        subMode = 0;
      }
    } else if (button == 6) {
      mode++;
      if (mode >= modeMenuSize) {
        mode = 0;
      }
      displayDirty = true;
    }
  } 

  else if (mode == 1) {
    
    if (subMode == 0) {
      // Adjust the frequency by the selected step size
      if (value != 0) {
        adjFreq -= (unsigned long long)value * freqStepMenu[freqStepIndex];
        displayDirty = true;
        setFreq(displayFreq);
      }
    } else if (subMode == 1) {
      if (value != 0) {
        int newIndex = (int)freqStepIndex - value;
        if (newIndex < 0) {
          newIndex = 0;
        } else if ((unsigned int)newIndex >= freqStepMenuSize) {
          newIndex = freqStepMenuSize - 1;
        }
        freqStepIndex = newIndex;
        displayDirty = true;
      }    
    }
  
    if (button == 5) {
      displayDirty = true;
      if (subMode == 0) {
        subMode = 1;
      } else {
        subMode = 0;
      }
    } else if (button == 6) {
      mode++;
      if (mode >= modeMenuSize) {
        mode = 0;
      }
      displayDirty = true;
    }
  } 
}

