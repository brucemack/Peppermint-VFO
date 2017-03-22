// BITX40 VFO
// Bruce MacKinnon KC1FSZ 22-March-2017
//
//#include "si5351.h"
#include "Wire.h"
#include <SPI.h>
#include <Adafruit_SSD1306.h>
#include <ClickEncoder.h>
#include <TimerOne.h>

#define OLED_RESET 4

Adafruit_SSD1306 display(OLED_RESET);

Si5351 si5351;

// ------ Encoder Related ----------------------------------------------------------------

ClickEncoder encoder(3,2,4,4);

TimerOne serviceTimer;

void serviceCb() {
  encoder.service();
}

// The frequency that is displayed 
long long displayFreq = 720000000LL;
// The IF frequency 
long long ifFreq = 1200000000LL;
// The adjustment
long long adjFreq = 160000LL;

// ----- FREQUENCY STEP MENU ---------------------------------------------------------------
const unsigned int freqStepMenuSize = 7;

const long long freqStepMenu[freqStepMenuSize] = {
  100LL,
  1000LL,
  10000LL,
  50000LL,
  100000LL,
  1000000LL,
  10000000LL
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
}

void updateDisplayFreq(unsigned long long freq) {
  unsigned long long f = freq / 100;
  unsigned long f0 = f / 1000000;
  unsigned long f1 = (f - (f0 * 1000000)) / 1000;
  unsigned long f2 = f - f0 * 1000000 - f1 * 1000;
  display.setCursor(10,20);
  display.print((long)f0);
  display.setCursor(30,20);
  display.printf("%03d",(long)f1);
  display.setCursor(70,20);
  display.printf("%03d",(long)f2);
}

void updateDisplay() {

  display.clearDisplay();
  display.setTextColor(WHITE);

  // Top line - logo and mode
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("KC1FSZ VFO 2");
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

void setFreq(long long displayFreq) {
  unsigned long long a = ifFreq - (displayFreq + adjFreq);
  si5351.set_freq(a,SI5351_PLL_FIXED, SI5351_CLK0);
  long f = a / 100ULL;
}

void processCATCommand(byte* cmd) {
  if (cmd[4] == 1) {
    
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
    if (cmdByfPtr == 5) {
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

