#include <GS2200Hal.h>
#include <GS2200AtCmd.h>
#include <TelitWiFi.h>
#include <Camera.h>
#include <GNSS.h>
#include <Wire.h>
#include "KX224.h"
#include "config.h"

#define POSITION_BUFFER_SIZE  128
#define BAUDRATE                (115200)
#define THRESHOLD   0.15
#define STRING_BUFFER_SIZE  128

static SpGnss Gnss;
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

void errorLED(int sw) {
  while (1) {
    if (sw & 0x1) {
      digitalWrite(LED0, HIGH);
    }
    if (sw & 0x2) {
      digitalWrite(LED1, HIGH);
    }
    if (sw & 0x4) {
      digitalWrite(LED2, HIGH);
    }
    if (sw & 0x8) {
      digitalWrite(LED3, HIGH);
    }
    delay(100);
    digitalWrite(LED0, LOW);
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    digitalWrite(LED3, LOW);
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
    resp = AtCmd_HTTPSEND( server_cid, HTTP_METHOD_POST, 10, "/postData", sendData, size );
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

static void print_pos(SpNavData *pNavData)
{
  char StringBuffer[STRING_BUFFER_SIZE];

  /* print time */
  snprintf(StringBuffer, STRING_BUFFER_SIZE, "%04d/%02d/%02d ", pNavData->time.year, pNavData->time.month, pNavData->time.day);
  Serial.print(StringBuffer);

  snprintf(StringBuffer, STRING_BUFFER_SIZE, "%02d:%02d:%02d.%06d, ", pNavData->time.hour, pNavData->time.minute, pNavData->time.sec, pNavData->time.usec);
  Serial.print(StringBuffer);

  /* print satellites count */
  snprintf(StringBuffer, STRING_BUFFER_SIZE, "numSat:%2d, ", pNavData->numSatellites);
  Serial.print(StringBuffer);

  /* print position data */
  if (pNavData->posFixMode == FixInvalid)
  {
    Serial.print("No-Fix, ");
  }
  else
  {
    Serial.print("Fix, ");
  }
  if (pNavData->posDataExist == 0)
  {
    Serial.print("No Position");
  }
  else
  {
    Serial.print("Lat=");
    Serial.print(pNavData->latitude, 6);
    Serial.print(", Lon=");
    Serial.print(pNavData->longitude, 6);
  }

  Serial.println("");
}

// ?????????????????????GPS????????????????????????????????????????????????????????????????????????????????????????????????????????????
bool wait(SpNavData *pNavData, int sec, bool br) {
  bool ret = false;
  byte rc;
  float acc[3];
  for (int i = 0; i < sec; i++) {
    rc = kx224.get_val(acc);
    if (rc == 0) {
/*      Serial.write("KX224 (X) = ");
      Serial.print(acc[0]);
      Serial.print(":");
      Serial.print(g_X);
      Serial.print("  (Y) = ");
      Serial.print(acc[1]);
      Serial.print(":");
      Serial.print(g_Y);
      Serial.print("  (Z) = ");
      Serial.print(acc[2]);
      Serial.print(":");
      Serial.println(g_Z);*/
      if ((acc[0] - g_X > THRESHOLD) || (acc[1] - g_Y > THRESHOLD) || (acc[2] - g_Z > THRESHOLD)) {
        if (br) {
          Serial.println("Moved break!");
          ret = true;
          delay(2000);          // ???????????????????????????????????????????????????Gnss.waitUpdate???????????????
          break;
         } else {
          Serial.println("Moved not break");
        }
      }
      g_X = acc[0];
      g_Y = acc[1];
      g_Z = acc[2];
    }
    
    digitalWrite(LED3, HIGH);       // ?????????????????????????????????????????????LED3??????????????????
    delay(100);
    digitalWrite(LED3, LOW);        // LED3?????????
    if (!br) {
      delay(100);
      digitalWrite(LED3, HIGH);     // ????????????2?????????
      delay(100);
      digitalWrite(LED3, LOW);      // LED3?????????      
    }
    if (Gnss.waitUpdate(1000)) {
      Gnss.getNavData(pNavData);
      print_pos(pNavData);
      if (pNavData->posFixMode != FixInvalid && pNavData->posDataExist != 0) {
        digitalWrite(LED2, HIGH);   // GPS??????????????????LED2?????????
      } else {
        digitalWrite(LED2, LOW);    // GPS????????????????????????LED2?????????
      }
    }
  }
  return ret;
}

void setup() {
  byte rc;
  float acc[3];
  SpNavData NavData;
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

  // GPS????????????
  Gnss.setDebugMode(PrintInfo);
  int result;
  result = Gnss.begin();
  if (result != 0) {
    Serial.println("Gnss begin error!!");
    errorLED(0x4);
  } else {
    Gnss.select(GPS);
    //Gnss.select(QZ_L1CA);
    //Gnss.select(QZ_L1S);
    result = Gnss.start(COLD_START);
    if (result != 0) {
      Serial.println("Gnss start error!!");
    } else {
      Serial.println("Gnss setup OK");   
    }
  }

  // ?????????????????????
  Wire.begin();
  rc = kx224.init();
  if (rc != 0) {
    Serial.println("KX224 initialization failed");
    Serial.flush();
    errorLED(0x2);
  }
  delay(300);
  rc = kx224.get_val(acc);
  if (rc != 0) {
    Serial.println("KX224 get value failed");
    Serial.flush();
    errorLED(0x2);
  } else {
    g_X = acc[0];
    g_Y = acc[1];
    g_Z = acc[2];
  }
  digitalWrite(LED1, HIGH);       // ????????????????????????????????????LED1???ON
}

void loop() {
  SpNavData NavData;
  float lat = 0.0;
  float lng = 0.0;
  // ??????????????????
  if (wait(&NavData, 60, true)) {     // ??????????????????????????????????????????if??????????????????
    if (NavData.posFixMode != FixInvalid && NavData.posDataExist != 0) {
      lat = NavData.latitude;
      lng = NavData.longitude;
    }
    // ??????
    Serial.println("  === theCamera.takePicture() befor");
    CamImage img = theCamera.takePicture();
    Serial.println("  === theCamera.takePicture() after");
    if (img.isAvailable()) {
      Gnss.saveEphemeris();
      if (Gnss.stop() != 0) {
        Serial.println("Gnss stop error!!");
      } else if (Gnss.end() != 0) {
        Serial.println("Gnss end error!!");
      } else {
        Serial.println("Gnss stop OK.");
      }
      //Gnss.stop();
      //Gnss.end();
      Serial.println("  === img.isAvailable() TRUE");
      char* imgBuff = img.getImgBuff();
      Serial.println("  === img.getImgBuff()");
      uint32_t imgSize = img.getImgSize();
      uint32_t sendSize = imgSize + POSITION_BUFFER_SIZE;
      Serial.print("  ALLOC SIZE :");
      Serial.println(sendSize);
//      char* sendBuff = (char*)malloc(sendSize);
//      snprintf(sendBuff, POSITION_BUFFER_SIZE, "%10.6f %10.6f", lat, lng);
//      memcpy(sendBuff + POSITION_BUFFER_SIZE, imgBuff, imgSize);
//      printDebug("  === ", sendBuff, sendSize);
//      post(sendBuff, sendSize);     // ??????
//      free(sendBuff);
      //Gnss.start(HOT_START);
      //Gnss.begin();
      delay(1000);
      if (Gnss.begin() != 0) {
        Serial.println("Gnss begin error!!");
      }
      delay(1000);
      Gnss.select(GPS);
      delay(1000);
      if (Gnss.start(HOT_START) != 0) {
        Serial.println("Gnss start error!!");
      } else {
        Serial.println("Gnss restart OK.");
      }

      
      wait(&NavData, 60, false);    // ????????????????????????????????????
    } else {
      Serial.println("  === img.isAvailable() FALSE");
    }
  }
}
