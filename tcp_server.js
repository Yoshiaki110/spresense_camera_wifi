var net = require('net');
var port = 10001;
var tcp_server;
var len=0;

var fs = require('fs');
var _conn;


tcp_server = net.createServer(function(conn){
  _conn = conn;
  console.log('server-> tcp server created');

  conn.on('data', function(data){
    len = len + data.length;
  });

  conn.on('close', function(){
    console.log('server-> client closed connection');
  });
}).listen(port);

console.log('listening on port %d', port);


const readline = require('readline');

/**
 * メイン処理
 */
const main = async () => {
  for (;;) {
    if (await confirm('撮影しますか？>')) {
      console.log('シャッター切りました');
      _conn.write('aaa');
    } else {
      console.log('なにもしません');
    }
    console.log('');  // 改行
  }
};

/**
 * ユーザーにYes/Noで答えられる質問をする
 */
const confirm = async (msg) => {
  const answer = await question(`${msg}(y/n): `);
  return answer.trim().toLowerCase() === 'y';
};

/**
 * 標準入力を取得する
 */
const question = (question) => {
  const readlineInterface = readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });
  return new Promise((resolve) => {
    readlineInterface.question(question, (answer) => {
      resolve(answer);
      readlineInterface.close();
    });
  });
};


const server = require("ws").Server;
const ws_server = new server({ port: 8080 });
var blinks = [];
var eyeMoves = [];
var noises = [];

ws_server.on("connection", ws => {
  console.log("connected from client");
  ws.on('message',function(message){
    if(message.indexOf("heartbeat") === -1){
      //メッセージを出力
      //console.log(message);

      //実際の処理する場合はparseして処理していきます
      const obj = JSON.parse(message);
      //console.log(obj);
      //console.log(obj['sequenceNumber']);
      //console.log(obj['eyeMoveUp'], obj['eyeMoveDown'], obj['eyeMoveRight'], obj['eyeMoveLeft']);
      //console.log(obj['blinkSpeed'], obj['blinkStrength']);
      //console.log(obj['fitError'], obj['noiseStatus']);
      var blink = (obj['blinkSpeed'] != 0 || obj['blinkStrength'] != 0) ? 1 : 0;
      blinks.push(blink);
      if (blinks.length > 20) {
        blinks.shift();
      }
      var eyeMove = (obj['eyeMoveUp'] != 0 || obj['eyeMoveDown'] != 0 || obj['eyeMoveRight'] != 0 || obj['eyeMoveLeft'] != 0) ? 1 : 0;
      eyeMoves.push(eyeMove);
      if (eyeMoves.length > 20) {
        eyeMoves.shift();
      }
      var noise = obj['noiseStatus'] ? 1 : 0;
      noises.push(noise);
      if (noises.length > 40) {
        noises.shift();
      }
      let bs = blinks.reduce((sum, element) => sum + element, 0);
      let es = eyeMoves.reduce((sum, element) => sum + element, 0);
      let ns = noises.reduce((sum, element) => sum + element, 0);
      let shoot = bs > 2 || es > 3 || ns > 30 ? 'shoot!!' : ''
      console.log(blinks.length, eyeMoves.length, bs, es, ns, shoot);
    }
  });
});




// 起動
(async () => {
  await main();
})();


/*
{
  sequenceNumber: 215,
  pitch: 0,
  eyeMoveRight: 0,
  yaw: 0,
  accX: -1.75,
  blinkSpeed: 0,
  powerLeft: 4,
  accY: 13.0625,
  roll: 0,
  eyeMoveDown: 0,
  fitError: 0,
  eyeMoveUp: 0,
  noiseStatus: false,
  accZ: -9.4375,
  eyeMoveLeft: 0,
  blinkStrength: 0,
  walking: false
}
*/