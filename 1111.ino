/**
 * ==============================
 * ESP32 MASTER - 2 NÚT NHẤN
 * (Đã sửa lỗi mong đợi 8 byte)
 * ==============================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <WebServer.h>

// --- WiFi Access Point ---
const char* ssid = "NoiseMaster";
const char* password = "12345678";

// --- Cấu hình nút nhấn ---
const int TOGGLE_BUTTON_PIN = 12;
const int CALIBRATE_BUTTON_PIN = 27;

// --- Biến lưu trạng thái ---
volatile bool isReceivingEnabled = true;
volatile float lastDB = 0.0;     // Sẽ được cập nhật trong OnDataRecv
volatile float lastAngle = 0.0;  // Sẽ được cập nhật trong OnDataRecv

// --- Biến chống dội (Debounce) ---
unsigned long lastDebounceTime_toggle = 0;
unsigned long debounceDelay = 50;
int lastButtonState_toggle = HIGH;
int buttonState_toggle = HIGH;
unsigned long lastDebounceTime_cal = 0;
int lastButtonState_cal = HIGH;
int buttonState_cal = HIGH;

// ========================================================
// ✅ [SỬA] 1. STRUCT NHẬN (MONG ĐỢI 8 BYTE)
// ========================================================
// (Phải khớp 100% với struct_message_send của Slave: 2x float = 8 byte)
typedef struct struct_message {
  float dB;
  float angle;
} struct_message;

// Biến toàn cục để lưu dữ liệu nhận được
struct_message incomingData;

// --- Cấu trúc dữ liệu GỬI đến Slave (Lệnh) ---
typedef struct struct_command_message {
  int command; // 1 = Lệnh hiệu chỉnh (4 byte)
} struct_command_message;

struct_command_message commandData;

// --- Địa chỉ MAC Broadcast (gửi cho tất cả) ---
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- Tạo web server tại cổng 80 ---
WebServer server(80);

// ========================================================
// ✅ [SỬA] 2. HÀM ONDATARECV (Sửa logic kiểm tra 8 byte)
// ========================================================
void OnDataRecv(const esp_now_recv_info_t * recv_info, const uint8_t *incomingDataBuffer, int len) {
  // Chỉ xử lý nếu Master đang BẬT
  if (!isReceivingEnabled) {
    return;
  }

  Serial.print("\n===================================");
  Serial.print("\nĐã nhận được gói tin từ Slave: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", recv_info->src_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  // [SỬA] Kiểm tra độ dài có khớp với Struct 8 byte không
  if (len == sizeof(incomingData)) {
    
    // Sao chép dữ liệu vào biến toàn cục 'incomingData' (8 byte)
    memcpy(&incomingData, incomingDataBuffer, sizeof(incomingData));
    
    Serial.println("Đã sao chép dữ liệu (8 byte).");
    Serial.printf(" - Giá trị dB nhận được: %.2f dB\n", incomingData.dB);
    Serial.printf(" - Giá trị Angle nhận được: %.2f\n", incomingData.angle);

    // [SỬA] Cập nhật biến toàn cục cho Web Server
    lastDB = incomingData.dB;
    lastAngle = incomingData.angle;
    
  } else {
    // Lỗi: Kích thước gói tin không đúng
    Serial.printf("Lỗi: Kích thước gói tin không khớp! Mong đợi %d byte, nhận được %d byte.\n",
                  sizeof(incomingData), len); // Sửa để báo mong đợi 8 byte
  }
  Serial.println("===================================");
} // <-- ✅ [SỬA] 3. ĐÃ XÓA DẤU { THỪA GÂY LỖI CẤU TRÚC


// --- Callback khi GỬI lệnh đi (Giữ nguyên) ---
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("\r\nTrạng thái gửi lệnh Hiệu chỉnh: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Gửi thành công" : "Gửi thất bại");
  Serial.println("===================================");
}

// --- Route HTTP để laptop đọc dữ liệu (Giữ nguyên) ---
void handleData() {
  float currentDB = lastDB;
  float currentAngle = lastAngle;
  bool isListening = isReceivingEnabled;

  String json = "{\"dB\":" + String(currentDB, 2) +
                ",\"angle\":" + String(currentAngle, 2) +
                ",\"listening\":" + (isListening ? "true" : "false") + "}";
                
  server.send(200, "application/json", json);
}

// --- Hàm kiểm tra nút Bật/Tắt (pin 12) (Giữ nguyên) ---
void checkToggleButton() {
  int reading = digitalRead(TOGGLE_BUTTON_PIN);

  if (reading != lastButtonState_toggle) {
    lastDebounceTime_toggle = millis();
  }

  if ((millis() - lastDebounceTime_toggle) > debounceDelay) {
    if (reading != buttonState_toggle) {
      buttonState_toggle = reading;

      if (buttonState_toggle == LOW) {
        isReceivingEnabled = !isReceivingEnabled;
        Serial.println("===================================");
        Serial.printf("==> NÚT NHẤN: %s nhận tín hiệu <==\n", isReceivingEnabled ? "BẬT" : "TẮT");
        Serial.println("===================================");
      }
    }
  }
  lastButtonState_toggle = reading;
}

// --- Hàm kiểm tra nút Hiệu chỉnh (pin 21) (Giữ nguyên) ---
void checkCalibrateButton() {
  int reading = digitalRead(CALIBRATE_BUTTON_PIN);

  if (reading != lastButtonState_cal) {
    lastDebounceTime_cal = millis();
  }

  if ((millis() - lastDebounceTime_cal) > debounceDelay) {
    if (reading != buttonState_cal) {
      buttonState_cal = reading;

      if (buttonState_cal == LOW) {
        Serial.println("===================================");
        Serial.println("==> NÚT NHẤN: Gửi lệnh Hiệu chỉnh...");
        
        commandData.command = 1; // 1 = Lệnh hiệu chỉnh
        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &commandData, sizeof(commandData));

        if (result != ESP_OK) {
          Serial.println("==> Gửi lệnh thất bại ngay lập tức!");
          Serial.println("===================================");
        }
      }
    }
  }
  lastButtonState_cal = reading;
}


void setup() {
  Serial.begin(115200);
  Serial.println("Khởi động ESP32 Master...");

  pinMode(TOGGLE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CALIBRATE_BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Trạng thái ban đầu: Đang BẬT nhận tín hiệu.");

  Serial.print("Địa chỉ MAC của Master: ");
  Serial.println(WiFi.macAddress());

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password, 1);
  Serial.print("WiFi Master phát tại: ");
  Serial.println(WiFi.softAPIP());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW khởi tạo thất bại!");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  // Đăng ký peer Broadcast
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Thêm peer broadcast thất bại");
    return;
  }
  Serial.println("Đã thêm peer broadcast, sẵn sàng gửi lệnh.");

  server.on("/data", HTTP_GET, handleData);
  server.begin();
  Serial.println("HTTP Server đã sẵn sàng!");
}

void loop() {
  server.handleClient();
  checkToggleButton();
  checkCalibrateButton();
}
