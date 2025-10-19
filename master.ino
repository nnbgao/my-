/**
 * ==============================
 * ESP8266 MASTER - NHẬN DỮ LIỆU & GỬI QUA WIFI
 * ==============================
 * 📘 Chức năng:
 *  - Nhận dữ liệu dB từ ESP32 qua ESP-NOW.
 *  - Cung cấp dữ liệu qua HTTP server để laptop có thể truy cập vẽ heatmap.
 */

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266WebServer.h>

// --- WiFi Access Point để laptop kết nối ---
const char* ssid = "NoiseMaster";
const char* password = "12345678";

// --- Biến lưu dữ liệu mới nhất ---
float lastDB = 0.0;
float lastAngle = 0.0;

// --- Cấu trúc dữ liệu nhận từ ESP32 ---
typedef struct struct_message {
  float dB;
  float angle;
} struct_message;

struct_message incomingData;

// --- Tạo web server tại cổng 80 ---
ESP8266WebServer server(80);

// --- Callback khi nhận dữ liệu từ ESP32 ---
void OnDataRecv(uint8_t *mac, uint8_t *incomingDataBytes, uint8_t len) {
  memcpy(&incomingData, incomingDataBytes, sizeof(incomingData));
  lastDB = incomingData.dB;
  lastAngle = incomingData.angle;
  Serial.printf("Nhận được: %.2f dB | Góc %.2f°\n", lastDB, lastAngle);
}

// --- Route HTTP để laptop đọc dữ liệu ---
void handleData() {
  String json = "{\"dB\":" + String(lastDB, 2) + ",\"angle\":" + String(lastAngle, 2) + "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  // --- Tạo WiFi Access Point ---
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("WiFi Master phát tại: ");
  Serial.println(WiFi.softAPIP());

  // --- Khởi tạo ESP-NOW ---
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW khởi tạo thất bại!");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(OnDataRecv);

  // --- Cấu hình web server ---
  server.on("/data", handleData);
  server.begin();
  Serial.println("HTTP Server đã sẵn sàng!");
}

void loop() {
  server.handleClient(); // Lắng nghe laptop yêu cầu
}