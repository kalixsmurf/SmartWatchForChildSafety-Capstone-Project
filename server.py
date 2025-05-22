from flask import Flask, request, jsonify
import os
import joblib
import numpy as np
import librosa
from datetime import datetime

# Load the trained model for emotion detection
with open('emotion_model.joblib', 'rb') as file:
    emotion_model = joblib.load(file)
# Load the trained age model for age detection
with open('age_model.joblib', 'rb') as file:
    age_model = joblib.load(file)

app = Flask(__name__)
UPLOAD_FOLDER = "C:\\Users\\hayaa\\Desktop"

@app.route('/api/data', methods=['POST'])
def receive_data():
    data = request.get_json()
    print("Received:", data)
    return {"status": "ok"}, 200

# @app.route('/upload', methods=['POST'])
# def upload_file():
#     print("entered upload file")
#     if 'file' not in request.files:
#         return 'No file part', 400

#     file = request.files['file']
#     if file.filename == '':
#         return 'No selected file', 400

#     filepath = os.path.join(UPLOAD_FOLDER, file.filename)
#     file.save(filepath)
#     print('before start prediction')
#     return startPrediction(filepath)

@app.route('/upload', methods=['POST'])
def upload_file():
    print("Entered /upload")

    if request.content_type == 'audio/wav':
        # Save raw audio data sent directly in the request body
        filepath = os.path.join(UPLOAD_FOLDER, 'esp32_audio.wav')
        with open(filepath, 'wb') as f:
            f.write(request.data)

        print('File saved, starting prediction')
        return startPrediction(filepath)

    return 'Unsupported Content-Type', 415

def startPrediction(file_path):
    feature = extract_feature(file_path, mfcc=True)
    if feature is not None:
        feature_2d = np.array([feature])  # Convert to 2D array
        emotion_prediction = emotion_model.predict(feature_2d)
        age_prediction = age_model.predict(feature_2d)
        # Get current time
        current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        print(f"Emotion Prediction: {emotion_prediction[0]}, Age Prediction: {age_prediction[0]}, Timestamp: {current_time}")  # Debug print
        return jsonify({"emotion_prediction": emotion_prediction[0],"age_prediction": age_prediction[0],"timestamp": current_time})
    else:
        return jsonify({"error": "Feature extraction failed"}), 400

def extract_feature(file_name, mfcc=True):
    try:
        print(f"Entering extract_feature with file: {file_name}")  # Debug print so we can see if correct file was send
        X, sample_rate = librosa.load(file_name, res_type='kaiser_fast')
        print(f"Loaded file {file_name} with sample rate {sample_rate}")  # Debug print

        result = np.array([])

        if mfcc:
            print("Extracting MFCC features...")  # Debug print
            mfccs = np.mean(librosa.feature.mfcc(y=X, sr=sample_rate, n_mfcc=40).T, axis=0)
            print(f"MFCC features extracted: {mfccs}")  # Debug print
            result = np.hstack((result, mfccs)) if result.size else mfccs

        return result
    except Exception as e:
        print(f"Error processing file {file_name}: {e}")  # Debug print
        return None
    
if __name__ == '__main__':
    app.run(host='127.0.0.1', port=12000)