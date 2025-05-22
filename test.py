import requests

# Adjust this to match your Flask server's IP and port
url = "http://127.0.0.1:12000/upload"

# Path to your test .wav file
file_path = "myfile.wav"
print("Response Text: starting")

with open(file_path, 'rb') as f:
    files = {'file': (file_path, f, 'audio/wav')}
    response = requests.post(url, files=files)

print("Status Code:", response.status_code)

if response.headers.get('Content-Type') == 'application/json':
    data = response.json()
    if 'error' in data:
        print("Server error:", data['error'])
    else:
        print("Emotion:", data.get("emotion_prediction"))
        print("Age:", data.get("age_prediction"))
        print("Timestamp:", data.get("timestamp"))
else:
    print("Response:", response.text)
