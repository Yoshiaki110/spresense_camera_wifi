#include <GS2200Hal.h>
#include <GS2200AtCmd.h>
#include <TelitWiFi.h>
#include "config.h"
//#include <stdio.h>
#include <Camera.h>
#include <GNSS.h>
#include <Wire.h>
#include "KX224.h"

#define POSITION_BUFFER_SIZE  128       /**< %Buffer size */
#define BAUDRATE                (115200)
#define THRESHOLD   0.15

static SpGnss Gnss;                   /**< SpGnss object */
extern uint8_t  *RespBuffer[];
extern int   RespBuffer_Index;
extern uint8_t ESCBuffer[];
extern uint32_t ESCBufferCnt;
char server_cid = 0;
char httpsrvr_ip[16];
TelitWiFi gs2200;
TWIFI_Params gsparams;
KX224 kx224(KX224_DEVICE_ADDRESS_1E);
float g_X = 0.0;
float g_Y = 0.0;
float g_Z = 0.0;


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

void post(char* sendData, uint32_t size) {
  ATCMD_RESP_E resp;
  int count;
  bool httpresponse=false;
  uint32_t start;
  char size_string[10];
  
  digitalWrite(LED3, HIGH); // turn on LED
  ConsoleLog( "Start HTTP Client");

  // HTTPヘッダー
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

  // コンテントレングス
  sprintf( size_string, "%d", size );
  do {
    resp = AtCmd_HTTPCONF( HTTP_HEADER_CONTENT_LENGTH, size_string );
  } while (ATCMD_RESP_OK != resp);
  // 送信
  do {
    resp = AtCmd_HTTPSEND( server_cid, HTTP_METHOD_POST, 10, "/postData", sendData, size );
  } while (ATCMD_RESP_OK != resp);
  
  // レスポンス
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

void printDebug(char* title, char* sendData, uint32_t size) {
  Serial.print(title);
  Serial.print(" Image data size = ");
  Serial.print(size, DEC);
  Serial.print(" , ");

  Serial.print("buff addr = ");
  Serial.print((unsigned long)sendData, HEX);
  Serial.println("");
}

bool wait(SpNavData *pNavData, int sec, bool br) {
  byte rc;
  float acc[3];
  for (int i = 0; i < sec; i++) {
    rc = kx224.get_val(acc);
    if (rc == 0) {
      Serial.write("KX224 (X) = ");
      Serial.print(acc[0]);
      Serial.print(" [g]  (Y) = ");
      Serial.print(acc[1]);
      Serial.print(" [g]  (Z) = ");
      Serial.print(acc[2]);
      Serial.println(" [g]");
      if ((acc[0] - g_X > THRESHOLD) || (acc[1] - g_Y > THRESHOLD) || (acc[2] - g_Z > THRESHOLD)) {
        if (br) {
          return true;
        }
      }
      g_X = acc[0];
      g_Y = acc[1];
      g_Z = acc[2];
    }
    
    digitalWrite(LED3, HIGH); // turn on LED
    delay(100);
    digitalWrite(LED3, LOW);   // turn the LED off (LOW is the voltage level)
    if (Gnss.waitUpdate(1000)) {
      Gnss.getNavData(pNavData);
      if (pNavData->posFixMode != FixInvalid && pNavData->posDataExist != 0) {
        digitalWrite(LED2, HIGH); // turn on LED
      } else {
        digitalWrite(LED2, LOW);   // turn the LED off (LOW is the voltage level)
      }
    }
  }
  return false;
}

void setup() {
  byte rc;
  SpNavData NavData;
  pinMode(LED0, OUTPUT);    // ネットワーク
  pinMode(LED1, OUTPUT);    // 初期化終了
  pinMode(LED2, OUTPUT);    // GPS FIX
  pinMode(LED3, OUTPUT);    // 点滅、送信中点灯
  digitalWrite(LED0, LOW);   // turn the LED off (LOW is the voltage level)
  digitalWrite(LED1, LOW);   // turn the LED off (LOW is the voltage level)
  digitalWrite(LED2, LOW);   // turn the LED off (LOW is the voltage level)
  digitalWrite(LED3, LOW);   // turn the LED off (LOW is the voltage level)
  Serial.begin(BAUDRATE);    // talk to PC
  while (!Serial) {
    ;
  }

  // ネットワーク関連の初期化
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

  // カメラの初期化
  CamErr err;
  Serial.println("Prepare camera");
  err = theCamera.begin();
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
  }
//  Serial.println("Start streaming");
//  err = theCamera.startStreaming(true, CamCB);
//  if (err != CAM_ERR_SUCCESS) {
//    printError(err);
//  }
  Serial.println("Set Auto white balance parameter");
  err = theCamera.setAutoWhiteBalanceMode(CAM_WHITE_BALANCE_DAYLIGHT);
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
  }
  Serial.println("Set still picture format");
  err = theCamera.setStillPictureImageFormat(
   CAM_IMGSIZE_QVGA_H,
   CAM_IMGSIZE_QVGA_V,
   CAM_IMAGE_PIX_FMT_JPG);
  if (err != CAM_ERR_SUCCESS) {
    printError(err);
  }

  // GPSの初期化
  Gnss.setDebugMode(PrintInfo);
  int result;
  result = Gnss.begin();
  if (result != 0) {
    Serial.println("Gnss begin error!!");
  } else {
    Gnss.select(GPS);
    Gnss.select(QZ_L1CA);
    Gnss.select(QZ_L1S);
    result = Gnss.start(COLD_START);
    if (result != 0) {
      Serial.println("Gnss start error!!");
    } else {
      Serial.println("Gnss setup OK");
    }
  }

  Wire.begin();
  rc = kx224.init();
  if (rc != 0) {
    Serial.println("KX224 initialization failed");
    Serial.flush();
  }
  wait(&NavData, 60, true);
  digitalWrite(LED1, HIGH); // turn on LED
}

void loop() {
  SpNavData NavData;
  float lat = 0.0;
  float lng = 0.0;
  // 位置情報取得
  if (wait(&NavData, 60, true)) {     // breakする
    if (NavData.posFixMode != FixInvalid && NavData.posDataExist != 0) {
      lat = NavData.latitude;
      lng = NavData.longitude;
    }
    // 撮影
    CamImage img = theCamera.takePicture();
    if (img.isAvailable()) {
      char* imgBuff = img.getImgBuff();
      uint32_t imgSize = img.getImgSize();
      uint32_t sendSize = imgSize + POSITION_BUFFER_SIZE;
      char* sendBuff = (char*)malloc(sendSize);
      snprintf(sendBuff, POSITION_BUFFER_SIZE, "%10.6f %10.6f", lat, lng);
      memcpy(sendBuff + POSITION_BUFFER_SIZE, imgBuff, imgSize);
      printDebug("  === ", sendBuff, sendSize);
      post(sendBuff, sendSize);
      free(sendBuff);
    } else {
      Serial.println("  === img.isAvailable() FALSE");      
    }
    wait(&NavData, 60, false);
  }
}
