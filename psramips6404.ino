#include <Arduino.h>

#include "psram_t.h"

#define PSRAM_CS      10  // to IPS pin 1 #CE 
#define PSRAM_MOSI    11  // to IPS pin 5 SI/SIO0
#define PSRAM_MISO    12  // to IPS pin 2 SO/SIO1
#define PSRAM_SCLK    13  // to IPS pin 6 SCLK


PSRAM_T psram = PSRAM_T(PSRAM_CS, PSRAM_MOSI, PSRAM_SCLK, PSRAM_MISO);

static int cnt = 0;
static uint8_t randomdata[0x10000]; // 64k rando data

void setup() 
{ 
  Serial.begin(115200);
  // Init random data
  for (int i=0; i<0x10000 ; i++) 
  {
    randomdata[i] = random(256);
  }
  Serial.println("Init PSRAM");  
  psram.begin();
  Serial.println("Testing PSRAM...");  
}



void loop() 
{
  Serial.print("loop");
  Serial.println(cnt++);

  // Write and read random patern table over the all device in a loop
  for (int i=0; i<IPS_SIZE ; i++) 
  {
    psram.pswrite(i,randomdata[i&0xffff]);      
   }
  delay(100);
  for (int i=0; i< IPS_SIZE; i++) 
  {
    uint8_t val = psram.psread(i);
    if (val != randomdata[i&0xffff] )
    {
      Serial.print("err at ");
      Serial.println(i);
    } 
  }
}
