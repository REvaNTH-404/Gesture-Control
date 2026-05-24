import cv2
import mediapipe as mp
import requests
import numpy as np
import urllib.request
import threading
import time

# CONFIGURATION
esp_ip = "10.96.135.23" 
stream_url = f"http://{esp_ip}/stream"
control_url = f"http://{esp_ip}/move"

# Global variable for thread sharing
latest_frame = None

def frame_fetcher():
    global latest_frame
    print("Connecting to Stream...")
    stream = urllib.request.urlopen(stream_url)
    bytes_data = bytes()
    while True:
        try:
            bytes_data += stream.read(3072)
            a = bytes_data.find(b'\xff\xd8')
            b = bytes_data.find(b'\xff\xd9')
            if a != -1 and b != -1:
                jpg = bytes_data[a:b+2]
                bytes_data = bytes_data[b+2:]
                latest_frame = cv2.imdecode(np.frombuffer(jpg, dtype=np.uint8), cv2.IMREAD_COLOR)
        except:
            continue

# Start video fetching in background
threading.Thread(target=frame_fetcher, daemon=True).start()

# Initialize MediaPipe
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(max_num_hands=1, min_detection_confidence=0.6, min_tracking_confidence=0.5)
mp_draw = mp.solutions.drawing_utils

last_send_time = 0
print("AI Active! Move your hand in front of the ESP32.")

while True:
    if latest_frame is None: continue
    
    frame = cv2.flip(latest_frame, 1)
    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    results = hands.process(rgb)

    if results.multi_hand_landmarks:
        raw_x = results.multi_hand_landmarks[0].landmark[9].x
        # Stretch range (0.2-0.8 -> 0-100)
        stretched_x = (raw_x - 0.2) / (0.8 - 0.2)
        int_x = int(max(0, min(1, stretched_x)) * 100)

        # Update ESP32 every 150ms (Stable Network Speed)
        if time.time() - last_send_time > 0.15:
            try:
                requests.get(f"{control_url}?x={int_x}", timeout=0.05)
                last_send_time = time.time()
                print(f"Tracking Hand: {int_x}%", end="\r")
            except: pass
        
        mp_draw.draw_landmarks(frame, results.multi_hand_landmarks[0], mp_hands.HAND_CONNECTIONS)

    cv2.imshow("Gesture AI Monitor", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'): break

cv2.destroyAllWindows()