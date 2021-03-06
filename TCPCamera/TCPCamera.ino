#include <Watchdog.h>
#include <GS2200Hal.h>
#include <GS2200AtCmd.h>
#include <TelitWiFi.h>
#include <Camera.h>
//#include <Wire.h>
#include "config.h"

#define POSITION_BUFFER_SIZE  128
#define BAUDRATE                (115200)
//#define THRESHOLD   0.15
#define STRING_BUFFER_SIZE  128

extern uint8_t  *RespBuffer[];
extern int   RespBuffer_Index;
extern uint8_t ESCBuffer[];
extern uint32_t ESCBufferCnt;
char server_cid = 0;
char httpsrvr_ip[16];
TelitWiFi gs2200;
TWIFI_Params gsparams;
uint32_t timer = 0;

static void led_onoff( int num, bool stat ){
  switch( num ){
  case 0:
    digitalWrite( LED0, stat );
    break;
  case 1:
    digitalWrite( LED1, stat );
    break;
  case 2:
    digitalWrite( LED2, stat );
    break;
  case 3:
    digitalWrite( LED3, stat );
    break;
  }
}

/*---------------------------------------------------------------------------*
 * led_effect
 *---------------------------------------------------------------------------*
 * Description: See this effect....
 *---------------------------------------------------------------------------*/
static void led_effect(void) {
  static int cur=0, next;
  int i;
  static bool direction=true; // which way to go
  for ( i=-1; i<5; i++ ){
    if ( i==cur ){
      led_onoff( i, true );
      if ( direction ) {
        led_onoff( i-1, false );
      } else {
        led_onoff( i+1, false );
      }
    }
  }
  if ( direction ){ // 0 -> 1 -> 2 -> 3
    if ( ++cur > 4 ) {
      direction = false;
    }
  } else {
    if ( --cur < -1 ) {
      direction = true;
    }
  }   
}

void dispLED(int sw) {
  if (sw & 0x1) {
    digitalWrite(LED0, HIGH);
  } else {
    digitalWrite(LED0, LOW);
  }
  if (sw & 0x2) {
    digitalWrite(LED1, HIGH);
  } else {
    digitalWrite(LED1, LOW);
  }
  if (sw & 0x4) {
    digitalWrite(LED2, HIGH);
  } else {
    digitalWrite(LED2, LOW);
  }
  if (sw & 0x8) {
    digitalWrite(LED3, HIGH);
  } else {
    digitalWrite(LED3, LOW);
  }
}

void errorLED(int sw) {
  while (1) {
    dispLED(sw);
    delay(100);
    dispLED(0);
    delay(200);
  }
}

void printError(enum CamErr err) {
  Serial.print("Error: ");
  switch (err) {
    case CAM_ERR_NO_DEVICE:
      Serial.println("No Device");
      break;
    case CAM_ERR_ILLEGAL_DEVERR:
      Serial.println("Illegal device error");
      break;
    case CAM_ERR_ALREADY_INITIALIZED:
      Serial.println("Already initialized");
      break;
    case CAM_ERR_NOT_INITIALIZED:
      Serial.println("Not initialized");
      break;
    case CAM_ERR_NOT_STILL_INITIALIZED:
      Serial.println("Still picture not initialized");
      break;
    case CAM_ERR_CANT_CREATE_THREAD:
      Serial.println("Failed to create thread");
      break;
    case CAM_ERR_INVALID_PARAM:
      Serial.println("Invalid parameter");
      break;
    case CAM_ERR_NO_MEMORY:
      Serial.println("No memory");
      break;
    case CAM_ERR_USR_INUSED:
      Serial.println("Buffer already in use");
      break;
    case CAM_ERR_NOT_PERMITTED:
      Serial.println("Operation not permitted");
      break;
    default:
      break;
  }
}

void CamCB(CamImage img) {
  if (img.isAvailable()) {
    img.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);
  } else {
    Serial.print("Failed to get video stream image\n");
  }
}

void parse_httpresponse(char *message) {
  char *p;
  Serial.print("parse_httpresponse : ");
  Serial.println(message);
  if ((p=strstr( message, "200 OK\r\n" )) != NULL) {
    ConsolePrintf( "Response : %s\r\n", p+8 );
  }
}

// HTTP POST??????
void post(char* sendData, uint32_t size) {
  ATCMD_RESP_E resp;
  int count;
  bool httpresponse=false;
  uint32_t start;
  char size_string[10];
  
  digitalWrite(LED3, HIGH);     // ???????????????LED3?????????
  ConsoleLog( "Start HTTP Client");

  // HTTP????????????
  AtCmd_HTTPCONF( HTTP_HEADER_AUTHORIZATION, "Basic dGVzdDp0ZXN0MTIz" );
  AtCmd_HTTPCONF( HTTP_HEADER_CONTENT_TYPE, "application/x-www-form-urlencoded" );
  AtCmd_HTTPCONF( HTTP_HEADER_HOST, HTTP_SRVR_IP );

  //
  WiFi_InitESCBuffer();
  count = 0;
  ConsoleLog( "POST Start" );

  do {
    resp = AtCmd_HTTPOPEN( &server_cid, HTTP_SRVR_IP, HTTP_PORT );
  } while (ATCMD_RESP_OK != resp);
  
  ConsoleLog( "Socket Opened" );

  // ???????????????????????????
  sprintf( size_string, "%d", size );
  do {
    resp = AtCmd_HTTPCONF( HTTP_HEADER_CONTENT_LENGTH, size_string );
  } while (ATCMD_RESP_OK != resp);
  // ??????
  do {
    resp = AtCmd_HTTPSEND( server_cid, HTTP_METHOD_POST, 10, "/postFace", sendData, size );
    //resp = AtCmd_HTTPSEND( server_cid, HTTP_METHOD_POST, 10, "/postData", sendData, size );
  } while (ATCMD_RESP_OK != resp);
  
  // ???????????????
  while( 1 ){
    if( Get_GPIO37Status() ){
      resp = AtCmd_RecvResponse();
      
      if( ATCMD_RESP_BULK_DATA_RX == resp ){
        if( Check_CID( server_cid ) ){
          parse_httpresponse( (char *)(ESCBuffer+1) );
        }
      }
      WiFi_InitESCBuffer();
      break;
    }
  }
  
  start = millis();
  while(1){
    if( Get_GPIO37Status() ){
      resp = AtCmd_RecvResponse();
      if( ATCMD_RESP_OK == resp ){
        // AT+HTTPSEND command is done
        break;
      }
    }
    if( msDelta(start)>20000 ) // Timeout
      break;
  }
  delay( 1000 );
  do {
    resp = AtCmd_HTTPCLOSE( server_cid );
  } while( (ATCMD_RESP_OK != resp) && (ATCMD_RESP_INVALID_CID != resp) );
  ConsoleLog( "Socket Closed" );

  digitalWrite(LED3, LOW);   // ???????????????????????????LED3?????????
}

void printDebug(char* title, char* sendData, uint32_t size) {
  Serial.print(title);
  Serial.print(" Image data size = ");
  Serial.print(size, DEC);
  Serial.print(" , ");

  Serial.print("buff addr = ");
  Serial.print((unsigned long)sendData, HEX);
  Serial.println("");
}

bool tcpConnect() {
  ATCMD_RESP_E resp;
  ATCMD_NetworkStatus networkStatus;
  
  resp = ATCMD_RESP_UNMATCH;
  // Start a TCP client
  ConsoleLog( "Start TCP Client");
  resp = AtCmd_NCTCP( (char *)TCPSRVR_IP, (char *)TCPSRVR_PORT, &server_cid);
  if (resp != ATCMD_RESP_OK) {
    ConsoleLog( "No Connect!" );    // ?????????????????????????????????t
    delay(2000);
    return false;
  }
  if (server_cid == ATCMD_INVALID_CID) {
    ConsoleLog( "No CID!" );
    delay(2000);
    return false;
  }
  do {
    resp = AtCmd_NSTAT(&networkStatus);
  } while (ATCMD_RESP_OK != resp);
  ConsoleLog( "**** 64");
  ConsoleLog( "Connected" );
  ConsolePrintf("IP: %d.%d.%d.%d\r\n", 
    networkStatus.addr.ipv4[0], networkStatus.addr.ipv4[1], networkStatus.addr.ipv4[2], networkStatus.addr.ipv4[3]);

  WiFi_InitESCBuffer();
  return true;
}

void setup() {
  pinMode(LED0, OUTPUT);    // ??????????????????
  pinMode(LED1, OUTPUT);    // ???????????????
  pinMode(LED2, OUTPUT);    // GPS FIX
  pinMode(LED3, OUTPUT);    // ????????????????????????????????????
  digitalWrite(LED0, LOW);  // turn the LED off (LOW is the voltage level)
  digitalWrite(LED1, LOW);  // turn the LED off (LOW is the voltage level)
  digitalWrite(LED2, LOW);  // turn the LED off (LOW is the voltage level)
  digitalWrite(LED3, LOW);  // turn the LED off (LOW is the voltage level)
  Serial.begin(BAUDRATE);   // talk to PC
  while (!Serial) {
    delay(100);
  }

  // ????????????????????????????????????
  Init_GS2200_SPI();
  gsparams.mode = ATCMD_MODE_STATION;
  gsparams.psave = ATCMD_PSAVE_DEFAULT;
  if (gs2200.begin(gsparams)){
    ConsoleLog("GS2200 Initilization Fails");
    errorLED(0xf);
  }
  // AP??????
  for (;;) {
    if (gs2200.connect(AP_SSID, PASSPHRASE)){
      ConsoleLog("Association Fails");
      digitalWrite(LED0, HIGH);
      delay(300);
      digitalWrite(LED0, LOW);
      delay(300);
    } else {
      break;
    }
  }
  digitalWrite(LED0, HIGH);       // ??????????????????????????????LED0???ON

  // ?????????????????????
  CamErr err;
  Serial.println("Prepare camera");
  err = theCamera.begin(0);
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
    errorLED(0x8);
  }
//  Serial.println("Start streaming");
//  err = theCamera.startStreaming(true, CamCB);
//  if (err != CAM_ERR_SUCCESS) {
//    printError(err);
//  }
  Serial.println("Set Auto white balance parameter");
  err = theCamera.setAutoWhiteBalanceMode(CAM_WHITE_BALANCE_AUTO);
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
    errorLED(0x8);
  }
  Serial.println("Set Auto ISO Sensitivity");
  err = theCamera.setAutoISOSensitivity(true);
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
    errorLED(0x8);
  }
  Serial.println("Set still picture format");
  err = theCamera.setStillPictureImageFormat(
//   CAM_IMGSIZE_QVGA_H,
//   CAM_IMGSIZE_QVGA_V,
   CAM_IMGSIZE_VGA_H,
   CAM_IMGSIZE_VGA_V,
//   CAM_IMGSIZE_HD_H,    // img.getImgBuff()?????????????????????
//   CAM_IMGSIZE_HD_V,    // img.getImgBuff()?????????????????????
//   CAM_IMGSIZE_QUADVGA_H,    // img.getImgBuff()?????????????????????
//   CAM_IMGSIZE_QUADVGA_V,    // img.getImgBuff()?????????????????????
   CAM_IMAGE_PIX_FMT_JPG);
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
    errorLED(0x8);
  }
  tcpConnect();
  
  Watchdog.begin();
  Watchdog.start(10000);

  digitalWrite(LED1, HIGH);       // ????????????????????????????????????LED1???ON
  timer = millis();
}

void loop() {
  ATCMD_RESP_E resp;
  bool shoot = false;
  
  Watchdog.kick();
  if ( msDelta( timer ) > 100 ){
    timer = millis();
    led_effect();
  }
  while( Get_GPIO37Status() ){    // ???????????????????????????
    ConsoleLog( "**** 731");
    resp = AtCmd_RecvResponse();
    ConsoleLog( "**** 732");
    if ( ATCMD_RESP_BULK_DATA_RX == resp ){
      ConsoleLog( "**** 7321");
      if( Check_CID( server_cid ) ){
        ConsoleLog( "**** 73211");
        ConsolePrintf( "Receive %d byte:%s \r\n", ESCBufferCnt-1, ESCBuffer+1 );
        shoot = true;
      }
      ConsoleLog( "**** 7322");
      WiFi_InitESCBuffer();
    } else if (ATCMD_RESP_ESC_FAIL == resp) {
      ConsoleLog("TCP Disconnect");
      delay(1000);
      ConsoleLog("try re connect");
      tcpConnect();
      break;
    }
  }

  if (shoot) {
    // ??????
    Serial.println("  === theCamera.takePicture() befor");
    CamImage img = theCamera.takePicture();
    Serial.println("  === theCamera.takePicture() after");
    if (img.isAvailable()) {
      Serial.println("  === img.isAvailable() TRUE");
      char* imgBuff = img.getImgBuff();
      Serial.println("  === img.getImgBuff()");
      uint32_t imgSize = img.getImgSize();
      uint32_t sendSize = imgSize + POSITION_BUFFER_SIZE;
      Serial.print("  ALLOC SIZE :");
      Serial.println(sendSize);
      char* sendBuff = (char*)malloc(sendSize);
      snprintf(sendBuff, POSITION_BUFFER_SIZE, "%10.6f %10.6f", 0.0, 0.0);
      memcpy(sendBuff + POSITION_BUFFER_SIZE, imgBuff, imgSize);
      printDebug("  === ", sendBuff, sendSize);
      post(sendBuff, sendSize);     // ??????
      free(sendBuff);

      delay(100000);
    } else {
      Serial.println("  === img.isAvailable() FALSE");
    }
  }
}
