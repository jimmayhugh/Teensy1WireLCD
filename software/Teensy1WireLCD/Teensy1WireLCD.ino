/********************

Teensy1WireLCD.ino

Version 0.0.5
Last Modified 12/06/2014
By Jim Mayhugh

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

This software uses multiple libraries that are subject to additional
licenses as defined by the author of that software. It is the user's
and developer's responsibility to determine and adhere to any additional
requirements that may arise.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


*********************/

#include <OneWireSlave.h>
#include <LiquidCrystalFast.h>
#include <t3mac.h>


const uint8_t  dsslavepin =    9;
const uint8_t  lcdRS      =   12;
const uint8_t  lcdEnable  =   11;
const uint8_t  lcdRW      =   10;
const uint8_t  lcdD4      =    5;
const uint8_t  lcdD5      =    4;
const uint8_t  lcdD6      =    3;
const uint8_t  lcdD7      =    2;
const uint8_t  familyCode = 0x47;
const uint8_t  rows       =    4;
const uint8_t  cols       =   20;
const uint8_t  REDled     =   20;
const uint8_t  GRNled     =   21;
const uint8_t  BLUled     =   22;
const uint8_t  BLled      =   23;

//               {Family    , <---, ----, ----, ID--, ----, --->, CRC} 
uint8_t rom[8] = {familyCode, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
//                            {TLSB, TMSB, THRH, TLRL, Conf, 0xFF, Rese, 0x10,  CRC}
// uint8_t scratchpad[9] = {0x00, 0x00, 0x4B, 0x46, 0x7F, 0xFF, 0x00, 0x10, 0x00};

typedef struct
{
  uint8_t   t3LCDcmd;
  uint8_t   row;
  uint8_t   col;
  char      lcdStr[cols+1];
  uint8_t   t3LCDrdy;
}t3LCDcommand;

t3LCDcommand t3LCDclr = {0, 0, 0, "", 0 };

union tc3LCDdata
{
  t3LCDcommand t3LCDbuf;
  uint8_t      lcdArray[sizeof(t3LCDcommand)];
}t3LCDunion;

const uint8_t clrLCD      = 0x01;
const uint8_t setLCDon    = (clrLCD      << 1); // 0x02
const uint8_t setLCDoff   = (setLCDon    << 1); // 0x04
const uint8_t setLCDBLon  = (setLCDoff   << 1); // 0x08
const uint8_t setLCDBLoff = (setLCDBLon  << 1); // 0x10
const uint8_t prtLCD      = (setLCDBLoff << 1); // 0x20

const char Version[] = "    Version 0.5     ";

// initialize the library with the numbers of the interface pins
LiquidCrystalFast lcd(lcdRS, lcdRW, lcdEnable, lcdD4, lcdD5, lcdD6, lcdD7);
OneWireSlave ds(dsslavepin);

//Blinking
const uint8_t ledPin = 13;
int ledState = LOW;             // ledState used to set the LED
//long previousMillis = 0;        // will store last time LED was updated
elapsedMillis interval;
uint32_t interval_cnt = 250;
//long interval = 250;            // interval at which to blink (milliseconds)
uint8_t setDebug = 0x0;         // set to 0 to disable serial debug
uint32_t watchdogTimeout = 300000;


void setup() {
  if(setDebug)
  {
    delay(5000);
    Serial.begin(115200);
    delay(3000);
    Serial.println(F("Serial Debug started at 115200 baud"));
  }

  pinMode(REDled, OUTPUT);
  pinMode(GRNled, OUTPUT);
  pinMode(BLUled, OUTPUT);
  pinMode(BLled, OUTPUT);
  digitalWrite(REDled, HIGH);  
  digitalWrite(GRNled, HIGH);  
  digitalWrite(BLUled, HIGH);  
  digitalWrite(BLled, LOW);  

  clearCommandBuf();
  read_mac();
  memcpy( (uint8_t *) &rom[1], (uint8_t *) &mac, 6 );
  rom[7]=ds.crc8(rom, rom[7]); 
  
  // set up the LCD's number of rows and columns: 
  lcd.begin(cols, rows);

  attachInterrupt(dsslaveassignedint, slave, CHANGE);
  pinMode(ledPin, OUTPUT);
  ds.init(rom);

  if(setDebug)
  {
    Serial.print(F("MAC Address is "));
    print_mac();
    Serial.println();
  }
  
  lcd.print(F("   MAC Address is   "));
  lcd.setCursor(0, 1);
  for(uint8_t x = 0; x < 4; x++)
  {
    lcd.print(F(" 0x"));
    if(rom[x] < 16)
    {
      lcd.print(F("0"));
    }
    lcd.print(rom[x], HEX);
  }
  lcd.setCursor(0, 2);
  for(uint8_t x = 4; x < 8; x++)
  {
    lcd.print(F(" 0x"));
    if(rom[x] < 16)
    {
      lcd.print(F("0"));
    }
    lcd.print(rom[x], HEX);
  }
  lcd.setCursor(0, 3);
  lcd.print(Version);
  ds.setPower(EXTERNAL);
  if(setDebug)
  {
    Serial.println(F("Setting attach0F"));
  }
  ds.attach0Fh(lcdDisplay);
  if(setDebug)
  {
    Serial.println(F("Exiting attach0F"));
  }
  setWatchDog(watchdogTimeout);
}

void slave() {
  ds.MasterResetPulseDetection();
}

void loop() {
  blinking();
}

void lcdDisplay()
{
  noInterrupts();
  ds.recvData(t3LCDunion.lcdArray, sizeof(t3LCDcommand));

  if(setDebug)
  {
    Serial.println(F("Entering lcdDisplay"));
    Serial.print(F("t3LCDunion.t3LCDbuf.t3LCDcmd = "));
    Serial.println(t3LCDunion.t3LCDbuf.t3LCDcmd);
/*
    Serial.print(F("t3LCDunion.t3LCDbuf.row = "));
    Serial.println(t3LCDunion.t3LCDbuf.row);
    Serial.print(F("t3LCDunion.t3LCDbuf.col = "));
    Serial.println(t3LCDunion.t3LCDbuf.col);
    Serial.print(F("t3LCDunion.t3LCDbuf.lcdStr = "));
    Serial.println(t3LCDunion.t3LCDbuf.lcdStr);
*/
    Serial.print(F("t3LCDunion.t3LCDbuf.t3LCDrdy = "));
    Serial.println(t3LCDunion.t3LCDbuf.t3LCDrdy);
  }

  parseCommand();

  if(setDebug)
  {
    Serial.println(F("Exiting lcdDisplay"));
  }
  clearCommandBuf();
  interrupts();
}

void parseCommand(void)
{
  if(setDebug)
  {
    Serial.println(F("Entering parseCommand"));
  }
  
  for(uint8_t x = 0; x < 7; x++)
  {
    uint8_t y = 0x01;
    y = y << x;
    if(setDebug)
    {
      Serial.print(F("y = "));
      Serial.println(y, HEX);
      Serial.print(F("command - "));
      Serial.println((t3LCDunion.t3LCDbuf.t3LCDcmd & y), HEX);
    }

    switch(t3LCDunion.t3LCDbuf.t3LCDcmd & y)
    {
      case clrLCD: // clear screen
      {
        if(setDebug)
        {
          Serial.println(F("clear screen"));
        }
        lcd.clear();
        for(uint8_t x = 0; x < rows; x++)
        {
          lcd.setCursor(0, x);
          lcd.print("                    ");
        }
        break;
      }

      case setLCDon: // turn Display on
      {
        lcd.display();
        break;
      }

      case setLCDoff: // turn Display off
      {
        lcd.noDisplay();
        break;
      }

      case prtLCD: // display test at specified row and column
      {
        if(setDebug)
        {
          Serial.print(F("print string is "));
          Serial.print(strlen(t3LCDunion.t3LCDbuf.lcdStr));
          Serial.println(F(" bytes long"));
          for(uint8_t cnt = 0; cnt < strlen(t3LCDunion.t3LCDbuf.lcdStr); cnt++)
          {
            Serial.print(F("0x"));
            if(t3LCDunion.t3LCDbuf.lcdStr[cnt] < 0x10)
              Serial.print(F("0"));
            Serial.print(t3LCDunion.t3LCDbuf.lcdStr[cnt], HEX);
            if(cnt < (strlen(t3LCDunion.t3LCDbuf.lcdStr) -1))
              Serial.print(F(", "));
          }
          Serial.println();
        }
        if(strlen(t3LCDunion.t3LCDbuf.lcdStr) > cols)
        {
          t3LCDunion.t3LCDbuf.lcdStr[cols] = 0x00;
          if(setDebug)
          {
            Serial.print(F("print string is too long, truncating"));
          }
        }          
        lcd.setCursor(t3LCDunion.t3LCDbuf.col, t3LCDunion.t3LCDbuf.row);
        lcd.print(t3LCDunion.t3LCDbuf.lcdStr);
        break;
      }

      default:
      {
        if(setDebug)
        {
          Serial.print(F("invalid command - "));
          Serial.println((t3LCDunion.t3LCDbuf.t3LCDcmd & y), HEX);
        }
        break; // ignore invalid commands
      }
    }
  }
}

void clearCommandBuf(void)
{
  if(setDebug)
  {
    Serial.println(F("Entering clearCommandBuf"));
  }
  for(uint8_t x = 0; x < sizeof(t3LCDunion); x++)
  {
    t3LCDunion.lcdArray[x] = 0x00;
  }
  if(setDebug)
  {
    Serial.println(F("Exiting clearCommandBuf"));
  }
}

void blinking() {
  if(interval >= interval_cnt) 
  {
    ledState = !ledState;
    digitalWrite(ledPin, ledState);
    interval = 0;
/*
    if(setDebug)
    {
      ds.showID();
    }
*/
    kickWatchDog();
  }
}

void setWatchDog(uint32_t timeout)
{
WDOG_UNLOCK = WDOG_UNLOCK_SEQ1;
WDOG_UNLOCK = WDOG_UNLOCK_SEQ2;
delayMicroseconds(1); // Need to wait a bit..
WDOG_STCTRLH = 0x0001; // Enable WDG
WDOG_TOVALL = timeout & 0xffff; // The next 2 lines sets the time-out value. This is the value that the watchdog timer compare itself to.
WDOG_TOVALH = (timeout & 0xffff0000) >> 16;
WDOG_PRESC = 0; // This sets prescale clock so that the watchdog timer ticks at 1kHZ instead of the default 1kHZ/4 = 200 HZ
}

void kickWatchDog(void)
{
noInterrupts();
WDOG_REFRESH = 0xA602;
WDOG_REFRESH = 0xB480;
interrupts();
delay(1);
}
