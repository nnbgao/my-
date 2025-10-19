"""
=============================
LAPTOP - VẼ BẢN ĐỒ NHIỆT TĨNH
=============================
📘 Chức năng:
 - Nhận dữ liệu dB từ ESP8266 Master qua Wi-Fi.
 - Gán mỗi giá trị dB vào 1 vị trí cố định trên bản đồ.
 - Vẽ heatmap thể hiện cường độ âm thanh.
"""

import requests
import numpy as np
import matplotlib.pyplot as plt
import time

# --- Địa chỉ IP của ESP8266 Master (in ra khi setup) ---
ESP_IP = "192.168.4.1"   # Laptop phải kết nối WiFi "NoiseMaster"

# --- Cấu hình kích thước bản đồ ---
rows, cols = 5, 5
heatmap = np.zeros((rows, cols))

# --- Vòng lặp mô phỏng lấy dữ liệu nhiều điểm ---
for i in range(rows):
    for j in range(cols):
        try:
            res = requests.get(f"http://{ESP_IP}/data")
            data = res.json()
            db_value = data["dB"]
            print(f"Điểm ({i},{j}) -> {db_value:.2f} dB")
            heatmap[i, j] = db_value
            time.sleep(0.5)
        except Exception as e:
            print("Lỗi:", e)

# --- Vẽ heatmap ---
plt.imshow(heatmap, cmap="hot", interpolation="nearest")
plt.title("Bản đồ nhiệt tiếng ồn (tĩnh)")
plt.colorbar(label="Mức dB")
plt.show()