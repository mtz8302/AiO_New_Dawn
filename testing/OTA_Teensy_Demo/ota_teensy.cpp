/****************************************************************************************************************************
  OTA through Ethernet demo code for Teensy41
  by Olivier Merlet

  WARNING: You can brick your Teensy with incorrect flash erase/write, such as
  incorrect flash config (0x400-40F). This code may or may not prevent that.

  I, Olivier Merlet, give no warranty, expressed or implied for this software and/or
  documentation provided, including, without limitation, warranty of
  merchantability and fitness for a particular purpose.

  based on:
  - FlasherX Library (https://github.com/joepasquariello/FlasherX)
  - QNEthernet 
  - Async_AdvancedWebServer !!!! be sure to use the modified version fixing Upload bug 
  see (https://forum.pjrc.com/threads/72220-AsyncWebServer_Teensy41-bug-onUpload?p=321229&viewfull=1#post321229)
 
  
 *****************************************************************************************************************************/

#include "QNEthernet.h"       // https://github.com/ssilverman/QNEthernet
using namespace qindesign::network;

#if !( defined(CORE_TEENSY) && defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41) )
  #error Only Teensy 4.1 supported
#endif


//#define USING_DHCP            true
#define USING_DHCP            false

#if !USING_DHCP
  // Set the static IP address to use if the DHCP fails to assign
  IPAddress myIP(192, 168, 5, 126);
  IPAddress myNetmask(255, 255, 255, 0);
  IPAddress myGW(192, 168, 5, 1);
  IPAddress mydnsServer(192, 168, 5, 1);
  //IPAddress mydnsServer(8, 8, 8, 8);
#endif

#include <AsyncWebServer_Teensy41.h>

AsyncWebServer    server(80);

#include "FXUtil.h"		// read_ascii_line(), hex file support
extern "C" {
  #include "FlashTxx.h"		// TLC/T3x/T4x/TMM flash primitives
}


//******************************************************************************
// hex_info_t	struct for hex record and hex file info
//******************************************************************************
typedef struct {	// 
  char *data;		// pointer to array allocated elsewhere
  unsigned int addr;	// address in intel hex record
  unsigned int code;	// intel hex record type (0=data, etc.)
  unsigned int num;	// number of data bytes in intel hex record
 
  uint32_t base;	// base address to be added to intel hex 16-bit addr
  uint32_t min;		// min address in hex file
  uint32_t max;		// max address in hex file
  
  int eof;		// set true on intel hex EOF (code = 1)
  int lines;		// number of hex records received  
} hex_info_t;

//******************************************************************************
// hex_info_t	struct for hex record and hex file info
//******************************************************************************
void read_ascii_line( Stream *serial, char *line, int maxbytes );
int  parse_hex_line( const char *theline, char *bytes, unsigned int *addr, unsigned int *num, unsigned int *code );
int  process_hex_record( hex_info_t *hex );
void update_firmware( Stream *in, Stream *out, uint32_t buffer_addr, uint32_t buffer_size );


static char line[96];					// buffer for hex lines
int line_index=0;
static char data[32] __attribute__ ((aligned (8)));	// buffer for hex data
hex_info_t hex = {					// intel hex info struct
  data, 0, 0, 0,					//   data,addr,num,code
  0, 0xFFFFFFFF, 0, 					//   base,min,max,
  0, 0						//   eof,lines
};
bool ota_status=0; // 1=running
bool ota_final=0;
bool ota_apply=0;
uint32_t buffer_addr, buffer_size;

void OTAapply(){
  delay(100);
  if (ota_final){ 
    Serial.printf( "calling flash_move() to load new firmware...\n" );
    flash_move( FLASH_BASE_ADDR, buffer_addr, hex.max-hex.min );
    Serial.flush();
    REBOOT;
    for (;;) {}
  } else {
    Serial.printf( "erase FLASH buffer / free RAM buffer...\n" );
    firmware_buffer_free( buffer_addr, buffer_size );
    Serial.flush();
    REBOOT;
    for (;;) {}
  }
}

void OTAend(AsyncWebServerRequest *request){
  AsyncWebParameter* p = request->getParam(0);
  Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
  if (ota_final){
    Serial.printf( "\nhex file: %1d lines %1lu bytes (%08lX - %08lX)\n",
		hex.lines, hex.max-hex.min, hex.min, hex.max );

    // check FSEC value in new code -- abort if incorrect
    #if defined(KINETISK) || defined(KINETISL)
    uint32_t value = *(uint32_t *)(0x40C + buffer_addr);
    if (value != 0xfffff9de) {
      Serial.printf( "new code contains correct FSEC value %08lX\n", value );
    }
    else {
      Serial.printf( "abort - FSEC value %08lX should be FFFFF9DE\n", value );
      ota_final=false;
    } 
    #endif
  }

  if (ota_final){
    // check FLASH_ID in new code - abort if not found
    if (check_flash_id( buffer_addr, hex.max - hex.min )) {
      Serial.printf( "new code contains correct target ID %s\n", FLASH_ID );
    }
    else {
      Serial.printf( "abort - new code missing string %s\n", FLASH_ID );
      ota_final=false;
    }
  }

  AsyncWebServerResponse *response = request->beginResponse((!ota_final)?500:200, "text/plain", (!ota_final)?"OTA Failed... Going for reboot":"OTA Success! Going for reboot");
  response->addHeader("Connection", "close");
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);

  ota_apply=true;
}

//extern "C" char* sbrk(int incr);
int freeRam() {
  char top;
  return &top - reinterpret_cast<char*>(sbrk(0));
}

void OTA(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  if (!ota_status){
    Serial.println("Starting OTA...");
    if (firmware_buffer_init( &buffer_addr, &buffer_size ) == 0) {
        Serial.println( "unable to create buffer" );
    } else {
      Serial.printf( "created buffer = %1luK %s (%08lX - %08lX)\n",
      buffer_size/1024, IN_FLASH(buffer_addr) ? "FLASH" : "RAM",
      buffer_addr, buffer_addr + buffer_size );
      ota_status=true;
    }      
  }
  if (ota_status) {
    if(len){
      size_t i=0;
      while (i<len){
        if (data[i]==0x0A || (line_index==sizeof(line)-1)){ // '\n'
          line[line_index] = 0;	// null-terminate
          //Serial.printf( "%s\n", line );
          if (parse_hex_line( (const char*)line, hex.data, &hex.addr, &hex.num, &hex.code ) == 0) {
            Serial.printf( "abort - bad hex line %s\n", line );
            return request->send(400, "text/plain", "abort - bad hex line");
          }
          else if (process_hex_record( &hex ) != 0) { // error on bad hex code
            Serial.printf( "abort - invalid hex code %d\n", hex.code );
            return request->send(400, "text/plain", "invalid hex code");
          }
          else if (hex.code == 0) { // if data record
            uint32_t addr = buffer_addr + hex.base + hex.addr - FLASH_BASE_ADDR;
            if (hex.max > (FLASH_BASE_ADDR + buffer_size)) {
              Serial.print("addr: "); Serial.println(addr);
              Serial.print("buffer_addr: "); Serial.println(buffer_addr);
              Serial.print("buffer_size: "); Serial.println(buffer_size);
              Serial.print("hex.base: "); Serial.println(hex.base);
              Serial.print("hex.addr: "); Serial.println(hex.addr);
              Serial.print("hex.max: "); Serial.println(hex.max);
              Serial.print("FLASH_BASE_ADDR: "); Serial.println(FLASH_BASE_ADDR);
              Serial.printf( "abort - max address %08lX too large\n", hex.max );
              
              return request->send(400, "text/plain", "abort - max address too large");
            }
            else if (!IN_FLASH(buffer_addr)) {
              memcpy( (void*)addr, (void*)hex.data, hex.num );
            }
            else if (IN_FLASH(buffer_addr)) {
              int error = flash_write_block( addr, hex.data, hex.num );
              if (error) {
                Serial.printf( "abort - error %02X in flash_write_block()\n", error );
                return request->send(400, "text/plain", "abort - error in flash_write_block()");
              }
            }
          }
          hex.lines++;
          line_index=0;
        } else if (data[i]!=0x0D){ // '\r'
          line[line_index++]=data[i];
        }
        i++;
      }
    }
    if (final) { // if the final flag is set then this is the last frame of data
        Serial.println("Transfer finished");
        ota_final=true;
    }else{
        return;
    }
  } else {
    return request->send(400, "text/plain", "OTA could not begin");
  }
}

void handleNotFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

void setup()
{
  Serial.begin(115200);

  while (!Serial && millis() < 5000);

  delay(200);

  Serial.print("\nOTA through Ethernet demo code for ");
  Serial.println(BOARD_NAME);
  

  delay(500);

#if USING_DHCP
  // Start the Ethernet connection, using DHCP
  Serial.print("Initialize Ethernet using DHCP => ");
  Ethernet.begin();
#else
  // Start the Ethernet connection, using static IP
  Serial.print("Initialize Ethernet using static IP => ");
  Ethernet.begin(myIP, myNetmask, myGW);
  Ethernet.setDNSServerIP(mydnsServer);
#endif

  if (!Ethernet.waitForLocalIP(10000))
  {
    Serial.println(F("Failed to configure Ethernet"));

    if (!Ethernet.linkStatus())
    {
      Serial.println(F("Ethernet cable is not connected."));
    }

    // Stay here forever
    while (true)
    {
      delay(1);
    }
  }
  else
  {
    Serial.print(F("Connected! IP address:"));
    Serial.println(Ethernet.localIP());
  }

#if USING_DHCP
  delay(1000);
#else
  delay(2000);
#endif

  server.onNotFound(handleNotFound);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    String html = "<body><h1>OTA through Ethernet demo code for Teensy41</h1><br \><h2>select and send your binary file:</h2><br \><div><form method='POST' enctype='multipart/form-data' action='/'><input type='file' name='file'><button type='submit'>Send</button></form></div></body>";
    request->send(200, "text/html", html);
    });

  server.on("/", HTTP_POST, OTAend, OTA);

  server.begin();

  Serial.print(F("HTTP EthernetWebServer is @ IP : "));
  Serial.println(Ethernet.localIP());
  Serial.print(F("Visit http://"));
  Serial.print(Ethernet.localIP());
  Serial.println("/");
}

void loop()
{
  if (ota_apply){OTAapply();}
}
