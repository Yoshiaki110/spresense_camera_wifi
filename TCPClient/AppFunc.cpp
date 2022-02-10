#include <Arduino.h>
#include <GS2200AtCmd.h>
#include <GS2200Hal.h>
#include "AppFunc.h"
#include "config.h"

#define  APP_DEBUG

char TCP_Data[]="GS2200 TCP Client Data Transfer Test";
extern uint8_t ESCBuffer[];
extern uint32_t ESCBufferCnt;

// モジュールの初期化
void App_InitModule(void) {
  ATCMD_RESP_E r = ATCMD_RESP_UNMATCH;
  ATCMD_REGDOMAIN_E regDomain;
  char macid[20];

  /* Try to read boot-up banner */
  while( Get_GPIO37Status() ){
    r = AtCmd_RecvResponse();
    if( r == ATCMD_RESP_NORMAL_BOOT_MSG ) {
      ConsoleLog("Normal Boot.\r\n");
    }
  }

  do {
    r = AtCmd_AT();
  } while (ATCMD_RESP_OK != r);

  /* Send command to disable Echo */
  do {
    r = AtCmd_ATE(0);
  } while (ATCMD_RESP_OK != r);

  /* AT+WREGDOMAIN=? should run after disabling Echo, otherwise the wrong domain is obtained. */
  do {
    r = AtCmd_WREGDOMAIN_Q( &regDomain );
  } while (ATCMD_RESP_OK != r);

  if( regDomain != ATCMD_REGDOMAIN_TELEC ){
    do {
      r = AtCmd_WREGDOMAIN( ATCMD_REGDOMAIN_TELEC );
    } while (ATCMD_RESP_OK != r);
  } 

  /* Read MAC Address */
  do{
    r = AtCmd_NMAC_Q( macid );
  }while(ATCMD_RESP_OK != r); 

  /* Read Version Information */
  do {
    r = AtCmd_VER();
  } while (ATCMD_RESP_OK != r);

  /* Enable Power save mode */
  /* AT+WRXACTIVE=0, AT+WRXPS=1 */
  do{
    r = AtCmd_WRXACTIVE(0);
  }while(ATCMD_RESP_OK != r); 

  do{
    r = AtCmd_WRXPS(1);
  }while(ATCMD_RESP_OK != r); 

  /* Bulk Data mode */
  do{
    r = AtCmd_BDATA(1);
  }while(ATCMD_RESP_OK != r); 
}

// アクセスポイントに接続
void App_ConnectAP(void) {
  ATCMD_RESP_E r;

#ifdef APP_DEBUG
  ConsolePrintf("Associating to AP: %s\r\n", AP_SSID);
#endif

  /* Set Infrastructure mode */
  do { 
    r = AtCmd_WM(ATCMD_MODE_STATION);
  }while (ATCMD_RESP_OK != r);

  /* Try to disassociate if not already associated */
  do { 
    r = AtCmd_WD(); 
  }while (ATCMD_RESP_OK != r);

  /* Enable DHCP Client */
  do{
    r = AtCmd_NDHCP( 1 );
  }while(ATCMD_RESP_OK != r); 

  /* Set WPA2 Passphrase */
  do{
    r = AtCmd_WPAPSK( (char *)AP_SSID, (char *)PASSPHRASE );
  }while(ATCMD_RESP_OK != r); 

  /* Associate with AP */
  do{
    r = AtCmd_WA( (char *)AP_SSID, (char *)"", 0 );
  }while(ATCMD_RESP_OK != r); 

  /* L2 Network Status */
  do{
    r = AtCmd_WSTATUS();
  }while(ATCMD_RESP_OK != r); 
}

// メインの処理
void App_TCPClient_Test(void) {
  ATCMD_RESP_E resp;
  char server_cid = 0;
  bool served = false;
  ATCMD_NetworkStatus networkStatus;

  ConsoleLog( "**** 1");
  AtCmd_Init();
  ConsoleLog( "**** 2");

  App_InitModule();     // モジュールの初期化
  ConsoleLog( "**** 3");
  App_ConnectAP();      // アクセスポイントに接続
  ConsoleLog( "**** 4");

  while (1) {
    ConsoleLog( "**** 5");
    if (!served) {
      ConsoleLog( "**** 61");
      resp = ATCMD_RESP_UNMATCH;
      // Start a TCP client
      ConsoleLog( "Start TCP Client");
      resp = AtCmd_NCTCP( (char *)TCPSRVR_IP, (char *)TCPSRVR_PORT, &server_cid);
      if (resp != ATCMD_RESP_OK) {
        ConsoleLog( "No Connect!" );    // サーバと接続できなかっt
        delay(2000);
        continue;
      }
      ConsoleLog( "**** 62");
      if (server_cid == ATCMD_INVALID_CID) {
        ConsoleLog( "No CID!" );
        delay(2000);
        continue;
      }
      ConsoleLog( "**** 63");
      do {
        resp = AtCmd_NSTAT(&networkStatus);
      } while (ATCMD_RESP_OK != resp);
      ConsoleLog( "**** 64");
      ConsoleLog( "Connected" );
      ConsolePrintf("IP: %d.%d.%d.%d\r\n", 
              networkStatus.addr.ipv4[0], networkStatus.addr.ipv4[1], networkStatus.addr.ipv4[2], networkStatus.addr.ipv4[3]);
      served = true;
    } else {
      ConsoleLog( "**** 71");
      ConsoleLog( "Start to send TCP Data");
      // Prepare for the next chunck of incoming data
      WiFi_InitESCBuffer();

      // ここでずっと送信している
      while( 1 ){
        ConsoleLog( "**** 72");
        if( ATCMD_RESP_OK != AtCmd_SendBulkData( server_cid, TCP_Data, strlen(TCP_Data) ) ){
          // Data is not sent, we need to re-send the data
          delay(10);
        }
        ConsoleLog( "**** 73");
        while( Get_GPIO37Status() ){    // 受信データがあるか
          ConsoleLog( "**** 731");
          resp = AtCmd_RecvResponse();
          ConsoleLog( "**** 732");
          if ( ATCMD_RESP_BULK_DATA_RX == resp ){
            ConsoleLog( "**** 7321");
            if( Check_CID( server_cid ) ){
              ConsoleLog( "**** 73211");
              ConsolePrintf( "Receive %d byte:%s \r\n", ESCBufferCnt-1, ESCBuffer+1 );
            }
            ConsoleLog( "**** 7322");
            WiFi_InitESCBuffer();
          } else if (ATCMD_RESP_ESC_FAIL == resp) {
            served = false;
            break;
          }
        }
        if (served == false) {
          break;
        }
      }
    }
  }
}
