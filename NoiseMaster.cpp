/*
 * NoiseMaster.cpp - File thực thi logic (ĐÃ CẬP NHẬT v3.0)
 */

#include "NoiseMaster.h"

// Khởi tạo con trỏ static
NoiseMaster* NoiseMaster::instance = nullptr;

// --- Constructor ---
NoiseMaster::NoiseMaster(const char* ssid, const char* password) : _server(80) {
  _ssid = ssid;
  _password = password;
  _lastDB = 0.0;
  _lastAngle = 0.0;
  _slaveMacKnown = false;
  _buttonState = HIGH;
  _lastButtonState = HIGH;
  _lastDebounceTime = 0;
}

// --- Hàm Begin (chạy trong setup) ---
void NoiseMaster::begin() {
  instance = this;
  Serial.println("Khởi động NoiseMaster Library...");

  // --- Khởi tạo Pin ---
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED_LOW_DB, OUTPUT);
  pinMode(PIN_LED_HIGH_DB, OUTPUT);
  digitalWrite(PIN_LED_LOW_DB, LOW);
  digitalWrite(PIN_LED_HIGH_DB, LOW);

  // --- WiFi & Server ---
  Serial.print("Địa chỉ MAC của Master: ");
  Serial.println(WiFi.macAddress());
  WiFi.mode(WIFI_AP);
  WiFi.softAP(_ssid, _password);
  Serial.print("WiFi Master phát tại: ");
  Serial.println(WiFi.softAPIP());
  _server.on("/data", HTTP_GET, NoiseMaster::handleData);
  _server.begin();
  Serial.println("HTTP Server đã sẵn sàng!");

  // --- Khởi tạo ESP-NOW ---
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW khởi tạo thất bại!");
    return;
  }
  esp_now_register_recv_cb(NoiseMaster::OnDataRecv);
  esp_now_register_send_cb(NoiseMaster::OnDataSent);
}

// --- Hàm Loop (chạy trong loop) ---
void NoiseMaster::loop() {
  _server.handleClient(); 
  checkButton(); 
}

// --- Hàm Public Getters ---
float NoiseMaster::getDb() {
  return _lastDB;
}

float NoiseMaster::getAngle() {
  return _lastAngle;
}

// --- HÀM PRIVATE ---

void NoiseMaster::sendCommandToSlave() {
  if (!_slaveMacKnown) {
    Serial.println("Chưa biết địa chỉ Slave (chờ Slave gửi tin nhắn đầu tiên)");
    return;
  }
  struct_command cmd;
  cmd.command = 1; // Gửi lệnh 1
  esp_now_send(_slaveMacAddress, (uint8_t *) &cmd, sizeof(cmd));
}

void NoiseMaster::checkButton() {
  int reading = digitalRead(PIN_BUTTON);
  if (reading != _lastButtonState) {
    _lastDebounceTime = millis();
  }
  if ((millis() - _lastDebounceTime) > _debounceDelay) {
    if (reading != _buttonState) {
      _buttonState = reading;
      if (_buttonState == LOW) {
        Serial.println("Phát hiện nhấn nút! Gửi tín hiệu (1) đến Slave...");
        sendCommandToSlave(); 
      }
    }
  }
  _lastButtonState = reading;
}

// --- HÀM CALLBACK STATIC ---

// Hàm OnDataSent không thay đổi
void NoiseMaster::OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Gửi tín hiệu đến Slave: ");
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Thành công");
  } else {
    Serial.println("Thất bại");
  }
}

// ✨ ĐÃ SỬA: Cập nhật toàn bộ hàm OnDataRecv
void NoiseMaster::OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingDataBytes, int len) {
  if (instance == nullptr) return;

  // Lấy địa chỉ MAC từ cấu trúc mới 'recv_info'
  const uint8_t *mac_addr = recv_info->src_addr;

  // 1. Xử lý dữ liệu
  if (len == sizeof(instance->_incomingData)) {
    memcpy(&instance->_incomingData, incomingDataBytes, sizeof(instance->_incomingData));
    instance->_lastDB = instance->_incomingData.dB;
    instance->_lastAngle = instance->_incomingData.angle; 

    Serial.printf("Nhận từ [");
    for (int i = 0; i < 6; i++) Serial.printf("%02X%s", mac_addr[i], (i < 5) ? ":" : ""); // Dùng mac_addr
    Serial.printf("]: %.2f dB | Góc %.2f°\n", instance->_lastDB, instance->_lastAngle); 
    
    // 2. Logic LED
    if (instance->_lastDB < 25.0) {
      digitalWrite(instance->PIN_LED_LOW_DB, HIGH);
      digitalWrite(instance->PIN_LED_HIGH_DB, LOW);
    } else {
      digitalWrite(instance->PIN_LED_LOW_DB, LOW);
      digitalWrite(instance->PIN_LED_HIGH_DB, HIGH);
    }
    
  } else {
     Serial.printf("Lỗi: Nhận gói tin không đúng kích thước (%d bytes)\n", len);
     return;
  }

  // 3. Tự động thêm Peer (Dùng mac_addr)
  if (!esp_now_is_peer_exist(mac_addr)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac_addr, 6); // Dùng mac_addr
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Không thể thêm Slave vào danh sách peer");
    } else {
      Serial.println("Đã thêm Slave vào danh sách peer để chuẩn bị gửi tín hiệu.");
      memcpy(instance->_slaveMacAddress, mac_addr, 6); // Dùng mac_addr
      instance->_slaveMacKnown = true;
    }
  }
}

// Hàm handleData không thay đổi
void NoiseMaster::handleData() {
  if (instance == nullptr) return;
  float currentDB = instance->_lastDB;
  float currentAngle = instance->_lastAngle; 
  String json = "{\"dB\":" + String(currentDB, 2) + ",\"angle\":" + String(currentAngle, 2) + "}";
  instance->_server.send(200, "application/json", json);
}

// --- Hàm Public Getters ---
float NoiseMaster::getDb() {
  return _lastDB;
}

float NoiseMaster::getAngle() {
  return _lastAngle;
}

// --- HÀM PRIVATE ---

void NoiseMaster::sendCommandToSlave() {
  if (!_slaveMacKnown) {
    Serial.println("Chưa biết địa chỉ Slave (chờ Slave gửi tin nhắn đầu tiên)");
    return;
  }
  
  struct_command cmd;
  cmd.command = 1; // Gửi lệnh 1
  
  esp_now_send(_slaveMacAddress, (uint8_t *) &cmd, sizeof(cmd));
}

void NoiseMaster::checkButton() {
  int reading = digitalRead(PIN_BUTTON);
  if (reading != _lastButtonState) {
    _lastDebounceTime = millis();
  }
  if ((millis() - _lastDebounceTime) > _debounceDelay) {
    if (reading != _buttonState) {
      _buttonState = reading;
      if (_buttonState == LOW) {
        Serial.println("Phát hiện nhấn nút! Gửi tínH hiệu (1) đến Slave...");
        sendCommandToSlave(); // Gọi hàm private
      }
    }
  }
  _lastButtonState = reading;
}

// --- HÀM CALLBACK STATIC ---
// Các hàm này dùng `instance->` để truy cập các biến và hàm của class

void NoiseMaster::OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Gửi tín hiệu đến Slave: ");
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Thành công");
  } else {
    Serial.println("Thất bại");
  }
}

void NoiseMaster::OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingDataBytes, int len) {
  // Kiểm tra xem instance đã được khởi tạo chưa
  if (instance == nullptr) return;

  // 1. Xử lý dữ liệu
  if (len == sizeof(instance->_incomingData)) {
    memcpy(&instance->_incomingData, incomingDataBytes, sizeof(instance->_incomingData));
    instance->_lastDB = instance->_incomingData.dB;
    instance->_lastAngle = instance->_incomingData.angle; 

    Serial.printf("Nhận từ [");
    for (int i = 0; i < 6; i++) Serial.printf("%02X%s", mac_addr[i], (i < 5) ? ":" : "");
    Serial.printf("]: %.2f dB | Góc %.2f°\n", instance->_lastDB, instance->_lastAngle); 
    
    // 2. Logic LED
    if (instance->_lastDB < 25.0) {
      digitalWrite(instance->PIN_LED_LOW_DB, HIGH);
      digitalWrite(instance->PIN_LED_HIGH_DB, LOW);
    } else {
      digitalWrite(instance->PIN_LED_LOW_DB, LOW);
      digitalWrite(instance->PIN_LED_HIGH_DB, HIGH);
    }
    
  } else {
     Serial.printf("Lỗi: Nhận gói tin không đúng kích thước (%d bytes)\n", len);
     return;
  }

  // 3. Tự động thêm Peer
  if (!esp_now_is_peer_exist(mac_addr)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac_addr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Không thể thêm Slave vào danh sách peer");
    } else {
      Serial.println("Đã thêm Slave vào danh sách peer để chuẩn bị gửi tín hiệu.");
      memcpy(instance->_slaveMacAddress, mac_addr, 6); 
      instance->_slaveMacKnown = true;
    }
  }
}

void NoiseMaster::handleData() {
  // Kiểm tra xem instance đã được khởi tạo chưa
  if (instance == nullptr) return;

  float currentDB = instance->_lastDB;
  float currentAngle = instance->_lastAngle; 
  String json = "{\"dB\":" + String(currentDB, 2) + ",\"angle\":" + String(currentAngle, 2) + "}";
  
  // Dùng _server của instance
  instance->_server.send(200, "application/json", json);
}
