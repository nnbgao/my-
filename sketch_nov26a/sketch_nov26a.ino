#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <cmath>
#include <Adafruit_NeoPixel.h>

// --- Cấu hình chân ---
const int BUTTON_PIN = 12;

// --- DẢI LED ---
#define LED_VUNG_PIN    23
#define LED_VUNG_COUNT  16
Adafruit_NeoPixel strip_Vung(LED_VUNG_COUNT, LED_VUNG_PIN, NEO_GRB + NEO_KHZ800);

#define LED_MODE_PIN    18 
Adafruit_NeoPixel strip_Mode(1, LED_MODE_PIN, NEO_GRB + NEO_KHZ800);

// --- GIÁ TRỊ TÍN HIỆU ---
volatile float P_muot_values[4] = {0.0, 0.0, 0.0, 0.0}; 
unsigned long last_receive_time[4] = {0, 0, 0, 0};

// --- Ngưỡng (CÓ THỂ THAY ĐỔI QUA WEB) ---
float P_NGUONG_MODE_1 = 1300.0;
float P_NGUONG_MODE_2 = 1170.0;
float P_NGUONG_MIC_MIN = 870.0;
float PHAN_TRAM_VANG = 0.65;

const unsigned long MIC_TIMEOUT = 1000;

// --- Biến trạng thái ---
volatile int count = 0;
volatile bool buttonPressed = false; 
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; 
unsigned long lastDebounceTime = 0; 
unsigned long debounceDelay = 800;   

// --- Hiệu chuẩn & Tọa độ ---
float CALIBRATION_FACTORS[4] = {1.0, 1.05, 0.85, 0.8};

const int NUM_MICS = 4;
const float MIC_POSITIONS[NUM_MICS][2] = {
  {0.0, 0.0}, {18.0, 0.0}, {0.0, 12.0}, {18.0, 12.0}
};

const int NUM_ZONES = 4;
const float ZONE_CENTERS[NUM_ZONES][2] = {
  {6.61, 5.0}, {6.61, 9.85}, {15.54, 5.0}, {15.54, 9.85}
};

const int ZONE_TO_LED_MAP[NUM_ZONES][4] = {
  {9, 8, 15, 14}, {0, 1, 6, 7}, {10, 11, 12, 13}, {2, 3, 4, 5}
};

// --- Web Server ---
WebServer server(80);

// --- WiFi AP Config ---
const char* ssid = "ESP32_NoiseMonitor";
const char* password = "12345678";

// --- Biến lưu giá trị zones để web đọc ---
float P_est_zones[NUM_ZONES] = {0, 0, 0, 0};

double tinh_khoang_cach(double vung_x, double vung_y, double mic_x, double mic_y) {
  double delta_x = vung_x - mic_x;
  double delta_y = vung_y - mic_y;
  return sqrt(delta_x * delta_x + delta_y * delta_y);
}

typedef struct struct_message {
  int id;
  float p_value;
} struct_message;

void IRAM_ATTR handleButtonInterrupt() {
  if (millis() - lastDebounceTime > debounceDelay) {
    portENTER_CRITICAL_ISR(&mux);
    buttonPressed = true;
    lastDebounceTime = millis();
    portEXIT_CRITICAL_ISR(&mux);
  }
}

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  struct_message myData;
  memcpy(&myData, incomingData, sizeof(myData));
  int slave_id = myData.id;
  float p_value_tho = myData.p_value;
  
  if (slave_id >= 1 && slave_id <= 4) {
    int index = slave_id - 1;
    P_muot_values[index] = p_value_tho * CALIBRATION_FACTORS[index];
    last_receive_time[index] = millis();
  }
}

// === WEB INTERFACE ===

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Noise Monitor Dashboard</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { 
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: #fff;
      padding: 20px;
    }
    .container { max-width: 1200px; margin: 0 auto; }
    h1 { text-align: center; margin-bottom: 30px; font-size: 2.5em; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }
    .card { 
      background: rgba(255,255,255,0.1);
      backdrop-filter: blur(10px);
      border-radius: 15px;
      padding: 20px;
      margin-bottom: 20px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.3);
    }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; }
    .sensor-box {
      background: rgba(255,255,255,0.15);
      padding: 15px;
      border-radius: 10px;
      text-align: center;
    }
    .sensor-value {
      font-size: 2em;
      font-weight: bold;
      margin: 10px 0;
    }
    .mode-buttons {
      display: flex;
      gap: 10px;
      justify-content: center;
      flex-wrap: wrap;
    }
    .btn {
      padding: 12px 30px;
      border: none;
      border-radius: 8px;
      font-size: 1em;
      cursor: pointer;
      transition: all 0.3s;
      font-weight: bold;
    }
    .btn-mode0 { background: #95a5a6; color: white; }
    .btn-mode1 { background: #27ae60; color: white; }
    .btn-mode2 { background: #e74c3c; color: white; }
    .btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.3); }
    .btn:active { transform: translateY(0); }
    input[type="number"] {
      padding: 8px;
      border-radius: 5px;
      border: none;
      width: 100px;
      font-size: 1em;
    }
    .setting-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin: 10px 0;
      padding: 10px;
      background: rgba(255,255,255,0.1);
      border-radius: 8px;
    }
    .status { 
      font-size: 1.5em; 
      text-align: center; 
      padding: 15px;
      background: rgba(255,255,255,0.2);
      border-radius: 10px;
      margin-bottom: 20px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🔊 Noise Monitor Dashboard</h1>
    
    <div class="status" id="status">Đang tải...</div>
    
    <div class="card">
      <h2>📊 Giá trị Microphone</h2>
      <div class="grid">
        <div class="sensor-box">
          <div>Mic 1</div>
          <div class="sensor-value" id="mic1">0</div>
        </div>
        <div class="sensor-box">
          <div>Mic 2</div>
          <div class="sensor-value" id="mic2">0</div>
        </div>
        <div class="sensor-box">
          <div>Mic 3</div>
          <div class="sensor-value" id="mic3">0</div>
        </div>
        <div class="sensor-box">
          <div>Mic 4</div>
          <div class="sensor-value" id="mic4">0</div>
        </div>
      </div>
    </div>

    <div class="card">
      <h2>🎯 Giá trị Vùng (IDW)</h2>
      <div class="grid">
        <div class="sensor-box">
          <div>Vùng 1</div>
          <div class="sensor-value" id="zone1">0</div>
        </div>
        <div class="sensor-box">
          <div>Vùng 2</div>
          <div class="sensor-value" id="zone2">0</div>
        </div>
        <div class="sensor-box">
          <div>Vùng 3</div>
          <div class="sensor-value" id="zone3">0</div>
        </div>
        <div class="sensor-box">
          <div>Vùng 4</div>
          <div class="sensor-value" id="zone4">0</div>
        </div>
      </div>
    </div>

    <div class="card">
      <h2>🎮 Điều khiển Mode</h2>
      <div class="mode-buttons">
        <button class="btn btn-mode0" onclick="setMode(0)">Mode 0: TẮT</button>
        <button class="btn btn-mode1" onclick="setMode(1)">Mode 1: Bình thường</button>
        <button class="btn btn-mode2" onclick="setMode(2)">Mode 2: Yên tĩnh</button>
      </div>
    </div>

    <div class="card">
      <h2>⚙️ Cài đặt Ngưỡng</h2>
      <form id="settingsForm">
        <div class="setting-row">
          <label>Ngưỡng Mic Min:</label>
          <input type="number" id="micMin" step="10" value="870">
          <button type="button" class="btn" style="padding: 5px 15px;" onclick="updateMicMin()">Cập nhật</button>
        </div>
        <div class="setting-row">
          <label>Ngưỡng Mode 1:</label>
          <input type="number" id="mode1" step="10" value="1300">
          <button type="button" class="btn" style="padding: 5px 15px;" onclick="updateMode1()">Cập nhật</button>
        </div>
        <div class="setting-row">
          <label>Ngưỡng Mode 2:</label>
          <input type="number" id="mode2" step="10" value="1170">
          <button type="button" class="btn" style="padding: 5px 15px;" onclick="updateMode2()">Cập nhật</button>
        </div>
        <div class="setting-row">
          <label>% Màu Vàng (0.0-1.0):</label>
          <input type="number" id="yellow" step="0.05" value="0.65" min="0" max="1">
          <button type="button" class="btn" style="padding: 5px 15px;" onclick="updateYellow()">Cập nhật</button>
        </div>
      </form>
    </div>
  </div>

  <script>
    function updateData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('mic1').textContent = data.mic[0].toFixed(0);
          document.getElementById('mic2').textContent = data.mic[1].toFixed(0);
          document.getElementById('mic3').textContent = data.mic[2].toFixed(0);
          document.getElementById('mic4').textContent = data.mic[3].toFixed(0);
          
          document.getElementById('zone1').textContent = data.zones[0].toFixed(0);
          document.getElementById('zone2').textContent = data.zones[1].toFixed(0);
          document.getElementById('zone3').textContent = data.zones[2].toFixed(0);
          document.getElementById('zone4').textContent = data.zones[3].toFixed(0);
          
          let modeText = ['MODE 0: TẮT', 'MODE 1: Bình thường', 'MODE 2: Yên tĩnh'];
          document.getElementById('status').textContent = modeText[data.mode];
        });
    }

    function setMode(mode) {
      fetch('/setMode?mode=' + mode).then(() => updateData());
    }

    function updateMicMin() {
      let val = document.getElementById('micMin').value;
      fetch('/setMicMin?value=' + val).then(() => alert('Đã cập nhật!'));
    }

    function updateMode1() {
      let val = document.getElementById('mode1').value;
      fetch('/setMode1?value=' + val).then(() => alert('Đã cập nhật!'));
    }

    function updateMode2() {
      let val = document.getElementById('mode2').value;
      fetch('/setMode2?value=' + val).then(() => alert('Đã cập nhật!'));
    }

    function updateYellow() {
      let val = document.getElementById('yellow').value;
      fetch('/setYellow?value=' + val).then(() => alert('Đã cập nhật!'));
    }

    setInterval(updateData, 500);
    updateData();
  </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"mic\":[" + String(P_muot_values[0]) + "," + String(P_muot_values[1]) + "," + String(P_muot_values[2]) + "," + String(P_muot_values[3]) + "],";
  json += "\"zones\":[" + String(P_est_zones[0]) + "," + String(P_est_zones[1]) + "," + String(P_est_zones[2]) + "," + String(P_est_zones[3]) + "],";
  json += "\"mode\":" + String(count);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetMode() {
  if (server.hasArg("mode")) {
    count = server.arg("mode").toInt();
    if (count < 0) count = 0;
    if (count > 2) count = 2;
  }
  server.send(200, "text/plain", "OK");
}

void handleSetMicMin() {
  if (server.hasArg("value")) {
    P_NGUONG_MIC_MIN = server.arg("value").toFloat();
  }
  server.send(200, "text/plain", "OK");
}

void handleSetMode1() {
  if (server.hasArg("value")) {
    P_NGUONG_MODE_1 = server.arg("value").toFloat();
  }
  server.send(200, "text/plain", "OK");
}

void handleSetMode2() {
  if (server.hasArg("value")) {
    P_NGUONG_MODE_2 = server.arg("value").toFloat();
  }
  server.send(200, "text/plain", "OK");
}

void handleSetYellow() {
  if (server.hasArg("value")) {
    PHAN_TRAM_VANG = server.arg("value").toFloat();
  }
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Noise Monitor with Web Interface");
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);

  // Khởi tạo WiFi AP
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Khởi tạo ESP-NOW
  WiFi.mode(WIFI_AP_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Loi khoi tao ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  // Khởi tạo Web Server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/setMode", handleSetMode);
  server.on("/setMicMin", handleSetMicMin);
  server.on("/setMode1", handleSetMode1);
  server.on("/setMode2", handleSetMode2);
  server.on("/setYellow", handleSetYellow);
  server.begin();
  
  Serial.println("Web server started!");
  
  strip_Vung.begin();
  strip_Vung.setBrightness(10);
  strip_Vung.clear();
  
  strip_Mode.begin();
  strip_Mode.setBrightness(10);
  strip_Mode.setPixelColor(0, strip_Mode.Color(100, 100, 100));
  
  strip_Vung.show();
  strip_Mode.show();
}

void loop() {
  server.handleClient(); // Xử lý web requests
  
  unsigned long current_time = millis();
  for (int i = 0; i < NUM_MICS; i++) {
    if (current_time - last_receive_time[i] > MIC_TIMEOUT) {
      P_muot_values[i] = 0.0;
    }
  }
  
  if (buttonPressed) {
    portENTER_CRITICAL(&mux);
    buttonPressed = false;
    portEXIT_CRITICAL(&mux);

    count++;
    if (count > 2) count = 0;
    
    if (count == 0) {
      Serial.println("MODE 0: IDLE");
      strip_Vung.clear();
      strip_Vung.show();
      strip_Mode.setPixelColor(0, strip_Mode.Color(100, 100, 100));
      strip_Mode.show();
    } else if (count == 1) {
      Serial.println("MODE 1: BINH THUONG");
      strip_Mode.setPixelColor(0, strip_Mode.Color(0, 255, 0));
      strip_Mode.show();
    } else {
      Serial.println("MODE 2: YEN TINH");
      strip_Mode.setPixelColor(0, strip_Mode.Color(255, 0, 0));
      strip_Mode.show();
    }
  }

  if (count == 0) {
    // MODE 0
  } else {
    // Tính IDW
    for (int i = 0; i < NUM_ZONES; i++) {
      float vung_x = ZONE_CENTERS[i][0];
      float vung_y = ZONE_CENTERS[i][1];
      double tong_trong_so = 0.0;
      double tong_P_nhan_trong_so = 0.0;
      int mic_co_tin_hieu = 0;
      
      for (int j = 0; j < NUM_MICS; j++) {
        if (P_muot_values[j] < P_NGUONG_MIC_MIN) continue;
        
        mic_co_tin_hieu++;
        float mic_x = MIC_POSITIONS[j][0];
        float mic_y = MIC_POSITIONS[j][1];
        double d = tinh_khoang_cach(vung_x, vung_y, mic_x, mic_y);
        double w = (d > 0.001) ? (1.0 / d) : 10000.0;
        
        tong_trong_so += w;
        tong_P_nhan_trong_so += P_muot_values[j] * w;
      }
      
      P_est_zones[i] = (mic_co_tin_hieu == 0 || tong_trong_so == 0.0) ? 0.0 : (tong_P_nhan_trong_so / tong_trong_so);
    }
    
    float P_max = 0.0;
    for (int i = 0; i < NUM_ZONES; i++) {
      if (P_est_zones[i] > P_max) P_max = P_est_zones[i];
    }
    
    float P_nguong_min = (count == 1) ? P_NGUONG_MODE_1 : P_NGUONG_MODE_2;
    float P_nguong_vang = P_max * PHAN_TRAM_VANG;
    if (P_nguong_vang < P_nguong_min) P_nguong_vang = P_nguong_min;

    for (int i = 0; i < NUM_ZONES; i++) {
      int led1 = ZONE_TO_LED_MAP[i][0];
      int led2 = ZONE_TO_LED_MAP[i][1];
      int led3 = ZONE_TO_LED_MAP[i][2];
      int led4 = ZONE_TO_LED_MAP[i][3];

      uint32_t color;
      if (P_est_zones[i] >= P_max && P_max > P_nguong_min) {
        color = strip_Vung.Color(255, 0, 0);
      } else if (P_est_zones[i] > P_nguong_vang) {
        color = strip_Vung.Color(255, 150, 0);
      } else {
        color = strip_Vung.Color(0, 255, 0);
      }
      
      strip_Vung.setPixelColor(led1, color);
      strip_Vung.setPixelColor(led2, color);
      strip_Vung.setPixelColor(led3, color);
      strip_Vung.setPixelColor(led4, color);
    }
    strip_Vung.show();
  }

  delay(200);
}