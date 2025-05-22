from flask import Flask, request
import os
import JSON
#new edited
app = Flask(__name__)
UPLOAD_FOLDER = "C:\\Users\\BiLKANCOMPUTERS\\Desktop"

@app.route('/api/data', methods=['POST'])
def receive_data():
    data = request.get_json()
    print("Received:", data)


    # Extract phone numbers
    primary_phone = data.get('primaryPhone', '')
    secondary_phone = data.get('secondaryPhone', '')

    # Extract filters
    filters = data.get('filters', {})

    # Gender extraction
    gender = [g for g in ['Male', 'Female'] if filters.get(g) == 1]

    # Age groups extraction
    age_groups = [age for age in ['20s', '30s', '40s', '50s', '60s', '70s', '80s'] if filters.get(age) == 1]

    # Emotions extraction
    emotions_list = ['Angry', 'Sad', 'Neutral', 'Calm', 'Happy', 'Fear', 'Disgust', 'Surprised']
    emotions = [emotion for emotion in emotions_list if filters.get(emotion) == 1]

    # Display result
    print("Primary Phone:", primary_phone)
    print("Secondary Phone:", secondary_phone)
    print("Gender:", gender)
    print("Age Groups:", age_groups)
    print("Emotions:", emotions)

    with open("settings.txt","w") as file:
        file.write(f"Primary Phone Number:{primary_phone}")
        file.write(f"Secondary Phone Number:{secondary_phone}")
        file.write(f"Gender:{gender}")
        file.write(f"Age Group :{age_groups}")
        file.write(f"Emotion:{emotions}")


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