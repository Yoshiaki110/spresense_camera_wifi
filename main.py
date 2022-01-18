from flask import Flask, request, abort, render_template, send_file, Response, jsonify, make_response

app = Flask(__name__)
CORS(app)
app.config['JSONIFY_PRETTYPRINT_REGULAR'] = False     # jsonifyでのエラーを抑止するため

# SPRESENSEの写真受付
@app.route("/postData", methods=['POST'])
def post_data():
    a = request.get_data()
    b = request.data
    print('a len : ', len(a))
    print('b len : ', len(b))
    with open('static/images/sony.jpg', mode='wb') as fout:
        fout.write(b[128:])
    with open('static/images/sony.txt', mode='w') as fout:
        fout.write(b[:21].decode())
    return 'OK'

# SPRESENSEの写真表示
@app.route('/sony')
def user():
    try:
        with open('static/images/sony.txt', mode='r') as fin:
            pos = fin.read().split()
            print('OK sony.txt')
            print(pos)
    except:
        print('error sony.txt')
        pos = ['0.000000', '0.000000']
    tm = time.time()
    return render_template('sony.html', LNG=pos[1], LAT=pos[0], TIMESTAMP=str(tm))


# Flaskを使用してサーバーを起動
if __name__ == "__main__":
    port = int(os.getenv("PORT"))
    socketio.run(app, host="0.0.0.0", port=port)

