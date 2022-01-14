#include <GS2200Hal.h>
#include <GS2200AtCmd.h>
#include <TelitWiFi.h>
#include "config.h"
#include <stdio.h>  /* for sprintf */
#include <Camera.h>

extern uint8_t  *RespBuffer[];
extern int   RespBuffer_Index;
extern uint8_t ESCBuffer[];
extern uint32_t ESCBufferCnt;

char server_cid = 0;
char httpsrvr_ip[16];

TelitWiFi gs2200;
TWIFI_Params gsparams;
#define BAUDRATE                (115200)

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

/**
 * Callback from Camera library when video frame is captured.
 */

void CamCB(CamImage img) {
  /* Check the img instance is available or not. */
  if (img.isAvailable()) {
    /* If you want RGB565 data, convert image data format to RGB565 */
    img.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);
    /* You can use image data directly by using getImgSize() and getImgBuff().
     * for displaying image to a display, etc. */
  } else {
    Serial.print("Failed to get video stream image\n");
  }
}


void setup() {
  /* initialize digital pin LED_BUILTIN as an output. */
  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  digitalWrite(LED0, LOW);   // turn the LED off (LOW is the voltage level)
  digitalWrite(LED1, LOW);   // turn the LED off (LOW is the voltage level)
  digitalWrite(LED2, LOW);   // turn the LED off (LOW is the voltage level)
  digitalWrite(LED3, LOW);   // turn the LED off (LOW is the voltage level)
  Serial.begin(BAUDRATE);   // talk to PC
  while (!Serial) {
    ; /* wait for serial port to connect. Needed for native USB port only */
  }

  Init_GS2200_SPI();

  gsparams.mode = ATCMD_MODE_STATION;
  gsparams.psave = ATCMD_PSAVE_DEFAULT;
  if ( gs2200.begin( gsparams ) ){
    ConsoleLog( "GS2200 Initilization Fails" );
    while(1);
  }

  // AP接続
  if ( gs2200.connect( AP_SSID, PASSPHRASE ) ){
    ConsoleLog( "Association Fails" );
    while(1);
  }
  digitalWrite(LED0, HIGH); // turn on LED

  // カメラ初期化
  CamErr err;
  Serial.println("Prepare camera");
  err = theCamera.begin();
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
  }

  Serial.println("Start streaming");
  err = theCamera.startStreaming(true, CamCB);
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
  }

  Serial.println("Set Auto white balance parameter");
  err = theCamera.setAutoWhiteBalanceMode(CAM_WHITE_BALANCE_DAYLIGHT);
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
  }
 
  Serial.println("Set still picture format");
  err = theCamera.setStillPictureImageFormat(
   CAM_IMGSIZE_QUADVGA_H,
   CAM_IMGSIZE_QUADVGA_V,
   CAM_IMAGE_PIX_FMT_JPG);
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
  }
  digitalWrite(LED1, HIGH); // turn on LED
}

void parse_httpresponse(char *message) {
  char *p;
  if ((p=strstr( message, "200 OK\r\n" )) != NULL) {
    ConsolePrintf( "Response : %s\r\n", p+8 );
  }
}

void post(char* sendData, uint32_t size) {
  ATCMD_RESP_E resp;
  int count;
  bool httpresponse=false;
  uint32_t start;
  char size_string[10];
  
  digitalWrite(LED3, HIGH); // turn on LED
  ConsoleLog( "Start HTTP Client");

  /* Set HTTP Headers */
  AtCmd_HTTPCONF( HTTP_HEADER_AUTHORIZATION, "Basic dGVzdDp0ZXN0MTIz" );
  AtCmd_HTTPCONF( HTTP_HEADER_CONTENT_TYPE, "application/x-www-form-urlencoded" );
  AtCmd_HTTPCONF( HTTP_HEADER_HOST, HTTP_SRVR_IP );

  /* Prepare for the next chunck of incoming data */
  WiFi_InitESCBuffer();
  count = 0;
  ConsoleLog( "POST Start" );

  do {
    resp = AtCmd_HTTPOPEN( &server_cid, HTTP_SRVR_IP, HTTP_PORT );
  } while (ATCMD_RESP_OK != resp);
  
  ConsoleLog( "Socket Opened" );
    

  /* Content-Length should be set BEFORE sending the data */
  sprintf( size_string, "%d", size );
  do {
    resp = AtCmd_HTTPCONF( HTTP_HEADER_CONTENT_LENGTH, size_string );
  } while (ATCMD_RESP_OK != resp);
  
  do {
    resp = AtCmd_HTTPSEND( server_cid, HTTP_METHOD_POST, 10, "/postData", sendData, size );
  } while (ATCMD_RESP_OK != resp);
  
  /* Need to receive the HTTP response */
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

  digitalWrite(LED3, LOW);   // turn the LED off (LOW is the voltage level)
}

/*
void setupCamera() {
  CamErr err;
  Serial.begin(BAUDRATE);
  while (!Serial) {
    ; 
  }

  Serial.println("Prepare camera");
  err = theCamera.begin();
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
  }

  Serial.println("Start streaming");
  err = theCamera.startStreaming(true, CamCB);
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
  }

  Serial.println("Set Auto white balance parameter");
  err = theCamera.setAutoWhiteBalanceMode(CAM_WHITE_BALANCE_DAYLIGHT);
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
  }
 
  Serial.println("Set still picture format");
  err = theCamera.setStillPictureImageFormat(
   CAM_IMGSIZE_QUADVGA_H,
   CAM_IMGSIZE_QUADVGA_V,
   CAM_IMAGE_PIX_FMT_JPG);
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
  }
}
*/
void printDebug(char* title, char* sendData, uint32_t size) {
  Serial.print(title);
  Serial.print(" Image data size = ");
  Serial.print(size, DEC);
  Serial.print(" , ");

  Serial.print("buff addr = ");
  Serial.print((unsigned long)sendData, HEX);
  Serial.println("");
}

void loop() {
  sleep(5);
  digitalWrite(LED2, HIGH); // turn on LED
  Serial.println("=== loop start ====");
  Serial.println("  === call takePicture start ===");
  CamImage img = theCamera.takePicture();
  Serial.println("  === call takePicture end ===");

  if (img.isAvailable()) {
    char* sendData = img.getImgBuff();
    uint32_t size = img.getImgSize();
    printDebug("  === ", sendData, size);
    post(sendData, size);
  } else {
    Serial.println("  === img.isAvailable() FALSE");      
  }
  
  Serial.println("=== loop end sleep====");
  digitalWrite(LED2, LOW);   // turn the LED off (LOW is the voltage level)
  sleep(55);
}
/*
void setup() {
  setupNetwork();
  setupCamera();
}

void loop() {
  sleep(5);
  Serial.println("=== loop start ====");
  loopCamera();
  Serial.println("=== loop end ====");
  sleep(55);
}*/
