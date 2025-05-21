from flask import Flask, request
import os

app = Flask(__name__)
UPLOAD_FOLDER = "C:\\Users\\BiLKANCOMPUTERS\\Desktop"

@app.route('/api/data', methods=['POST'])
def receive_data():
    data = request.get_json()
    print("Received:", data)
    return {"status": "ok"}, 200

@app.route('/upload', methods=['POST'])
def upload_file():
    if 'file' not in request.files:
        return 'No file part', 400

    file = request.files['file']
    if file.filename == '':
        return 'No selected file', 400

    filepath = os.path.join(UPLOAD_FOLDER, file.filename)
    file.save(filepath)
    return f'File saved to {filepath}', 200

if __name__ == '__main__':
    app.run(host='127.0.0.1', port=12000)