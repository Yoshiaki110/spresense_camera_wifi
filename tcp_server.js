require('date-utils');

var net = require('net');
var port = 10001;
var server;

var dt = new Date();
var formatted = dt.toFormat("HH24:MI:SS");
var startTime=0;
var endTime;
var len=0;

var fs = require('fs');
var fname = "TCP_TEST"
var wdata;
var _conn;


// File Write Function
function writeFile(path, data) {
    fs.appendFile(path, data, function (err) {
	if (err) {
            throw err;
	}
    });
}


server = net.createServer(function(conn){
    _conn = conn;
    console.log('server-> tcp server created');

    conn.on('data', function(data){
	endTime = new Date();
	len = len + data.length;
	
	if( endTime - startTime > 1000 ){
	    dt = new Date();
	    formatted = dt.toFormat("HH24:MI:SS");
	    console.log(formatted, Math.floor(len*8/(endTime-startTime)), "Kbps");
	    wdata = formatted + "," + Math.floor(len*8/(endTime-startTime)) + "Kbps\r\n";
	    writeFile(fname, wdata);
	    len = 0;
	    startTime = endTime;
            conn.write('aaa');
	}
    });
	
    conn.on('close', function(){
	console.log('server-> client closed connection');
    });

    
}).listen(port);


if( process.argv.length > 2 ){
    fname = fname + "_" + process.argv[2] + ".log";
}
else{
    console.log('usage: node tcp_server.js <log file name>');
    testtime = new Date();
    formatted = testtime.toFormat("HH24MISS");
    fname = fname + "_" + formatted + ".log";
}

console.log('listening on port %d', port);


const readline = require('readline');

/**
 * メイン処理
 */
const main = async () => {
  for (;;) {
    console.log('エリクサーちょうだい！');
    if (await confirm('> あげますか？')) {
      console.log('ありがとう！^_^');
      _conn.write('aaa');
    } else {
      console.log('死ね！');
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

// 起動
(async () => {
  await main();
})();

