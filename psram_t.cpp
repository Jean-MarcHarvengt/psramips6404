/*
  PSRAM driver for IPS6404
*/

#include "psram_t.h"

#include <SPI.h>

// Tested on Teensy 4.0 only
// Using SPI clock patch from Kurt allowing going above 40MHz clock, to be released in SDK 1.49???
// Serial mode only (not QUAD SPI)
// DMA tests where not successful but Teensy is at 600MHz anyway

// Uses SPI1 by default unless overwrite SPI below
//#define SPI SPI2


#define SPICLOCK 70000000 // tested at 70MHz max, 104000000 not successful
#define SPI_MODE SPI_MODE0

#define RAM_READ  0xB
//#define RAM_READ  0x3
#define RAM_WRITE 0x2


uint8_t PSRAM_T::_cs, PSRAM_T::_miso, PSRAM_T::_mosi, PSRAM_T::_sclk;
Page PSRAM_T::pages[MAX_PAGES];
uint8_t PSRAM_T::nbPages=0;
int8_t PSRAM_T::top=0;
int8_t PSRAM_T::last=0;


PSRAM_T::PSRAM_T(uint8_t cs, uint8_t mosi, uint8_t sclk, uint8_t miso)
{
  _cs   = cs;
  _mosi = mosi;
  _sclk = sclk;
  _miso = miso;
  pinMode(_cs, OUTPUT); 
  digitalWrite(_cs, 1);
}


void PSRAM_T::begin(void)
{
  SPI.setMOSI(_mosi);
  SPI.setMISO(_miso);
  SPI.setSCK(_sclk);
  SPI.begin();

  delay(1);

  digitalWrite(_cs, 0);
  SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE));
  SPI.transfer(0x66);
  digitalWrite(_cs, 1);
  SPI.endTransaction();  
  SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE));
  digitalWrite(_cs, 0);
  SPI.transfer(0x99);
  SPI.endTransaction();
  digitalWrite(_cs, 1);
  delayMicroseconds(20);
}



uint8_t PSRAM_T::psram_read(uint32_t addr) 
{
  uint8_t val=0;

  digitalWrite(_cs, 0);  
  SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE));
  SPI.transfer(RAM_READ);
  SPI.transfer((addr>>16)&0xff);
  SPI.transfer((addr>>8)&0xff);
  SPI.transfer(addr&0xff);
#if RAM_READ == 0xB  
  SPI.transfer(0xFF);       
#endif
  val = SPI.transfer(0xFF);  
  SPI.endTransaction();
  digitalWrite(_cs, 1);
  return val;
}


void PSRAM_T::psram_read_n(uint32_t addr, uint8_t * val, int n) 
{
  digitalWrite(_cs, 0);
  SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE));
  SPI.transfer(RAM_READ);
  SPI.transfer((addr>>16)&0xff);
  SPI.transfer((addr>>8)&0xff);
  SPI.transfer(addr&0xff);
#if RAM_READ == 0xB  
  SPI.transfer(0xFF);       
#endif
  /*
  while (n > 0) {
    *val++ = SPI.transfer(0xFF);
    n--;       
  }    
  */
  SPI.transfer(val,n);
  SPI.endTransaction();
  digitalWrite(_cs, 1);
}


void PSRAM_T::psram_write(uint32_t addr, uint8_t val) 
{
  digitalWrite(_cs, 0);
  SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE));
  SPI.transfer(RAM_WRITE);
  SPI.transfer((addr>>16)&0xff);
  SPI.transfer((addr>>8)&0xff);
  SPI.transfer(addr&0xff);    
  SPI.transfer(val);
  SPI.endTransaction();    
  digitalWrite(_cs, 1);
}


void PSRAM_T::psram_write_n(uint32_t addr, uint8_t * val, int n) 
{
  uint8_t resp[PAGE_SIZE];
    
  digitalWrite(_cs, 0);
  SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE));
  SPI.transfer(RAM_WRITE);
  SPI.transfer((addr>>16)&0xff);
  SPI.transfer((addr>>8)&0xff);
  SPI.transfer(addr&0xff);
  /*
  while (n > 0) {
    SPI.transfer(*val++);
    n--;       
  }
  */
  SPI.transfer(val,&resp[0],n);
  SPI.endTransaction();  
  digitalWrite(_cs, 1); 
}



/********************/
/* Public methods   */
/********************/

/* 
 * Write a byte with cache support 
 */
void PSRAM_T::pswrite(uint32_t addr, uint8_t val) 
{
  psram_write(addr, val);
  //return;
  uint32_t curPage=addr&(~(PAGE_SIZE-1));
  for (int i=0; i<nbPages; i++) {
    if (pages[i].pageid == curPage) {
      pages[i].page[addr&(PAGE_SIZE-1)] = val;
      break;
    }
  }   
}


/* 
 * Read a byte with cache support 
 */
uint8_t PSRAM_T::psread(uint32_t addr) 
{
  //uint8_t val = psram_read(addr);
  //return val;
  uint32_t curPage=addr&(~(PAGE_SIZE-1));
  uint32_t offs = addr&(PAGE_SIZE-1);

  for (int i=0; i<nbPages; i++) {
    if (pages[i].pageid == curPage) {
      if ( (pages[i].prev != i) && (pages[i].next != i) ) {
        pages[pages[i].prev].next =  pages[i].next;
        pages[pages[i].next].prev = pages[i].prev;     
      }
      else if (pages[i].next != i) {
        pages[pages[i].next].prev = i;
      }
      else if (pages[i].prev != i) {
        pages[pages[i].prev].next =  pages[i].prev;
        last = pages[i].prev;        
      }
       // last page accessed to top
      pages[i].prev = i; //-1;
      pages[i].next = top;      
      pages[top].prev = i;
      top = i;
      return pages[top].page[offs];
    }
  }
  if (nbPages<MAX_PAGES) 
  {
    // add at top
    pages[nbPages].pageid = curPage;           
    pages[nbPages].prev = nbPages; //-1;
    pages[nbPages].next = top;     
    pages[top].prev = nbPages;
    top = nbPages;
    nbPages++;
  }
  else {
      // replace last and move to top
      int n = pages[last].prev;
      pages[n].next = n; //-1;
      pages[last].pageid = curPage;
      pages[last].prev = last; //-1;
      pages[last].next = top;      
      pages[top].prev = last;
      top = last;
      last = n;
  }
  psram_read_n(curPage,&(pages[top].page[0]),PAGE_SIZE);   
  return pages[top].page[offs];
}


/* 
 * Write a word with cache support 
 */
uint16_t PSRAM_T::psread_w(uint32_t addr) 
{
  uint32_t curPage=addr&(~(PAGE_SIZE-1));
  uint32_t offs = addr&(PAGE_SIZE-1);

  for (int i=0; i<nbPages; i++) {
    if (pages[i].pageid == curPage) {
      if ( (pages[i].prev != i) && (pages[i].next != i) ) {
        pages[pages[i].prev].next =  pages[i].next;
        pages[pages[i].next].prev = pages[i].prev;     
      }
      else if (pages[i].next != i) {
        pages[pages[i].next].prev = i;
      }
      else if (pages[i].prev != i) {
        pages[pages[i].prev].next =  pages[i].prev;
        last = pages[i].prev;        
      }
       // last page accessed to top
      pages[i].prev = i; //-1;
      pages[i].next = top;      
      pages[top].prev = i;
      top = i;     
      return (pages[top].page[offs+1]<<8) + pages[top].page[offs];
    }
  }
  if (nbPages<MAX_PAGES) 
  {
    // add at top
    pages[nbPages].pageid = curPage;           
    pages[nbPages].prev = nbPages; //-1;
    pages[nbPages].next = top;     
    pages[top].prev = nbPages;
    top = nbPages;
    nbPages++;
  }
  else {
      // replace last and move to top
      int n = pages[last].prev;
      pages[n].next = n; //-1;
      pages[last].pageid = curPage;
      pages[last].prev = last; //-1;
      pages[last].next = top;      
      pages[top].prev = last;
      top = last;
      last = n;
  }
  psram_read_n(curPage,&(pages[top].page[0]),PAGE_SIZE);   
  return (pages[top].page[offs+1]<<8) + pages[top].page[offs];
}

