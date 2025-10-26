/*
 * NoiseMaster.h - Thư viện cho ESP32 Master Node
 * Nhận dữ liệu ESP-NOW và cung cấp qua WebServer.
 */

#ifndef NoiseMaster_h
#define NoiseMaster_h

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <WebServer.h>

// --- CẤU TRÚC DỮ LIỆU (Nội bộ) ---
// Dùng để NHẬN từ Slave
typedef struct struct_message {
  float dB;
  float angle; 
} struct_message;

// Dùng để GỬI cho Slave
typedef struct struct_command {
  int command; 
} struct_command;


class NoiseMaster {
  public:
    // --- HÀM PUBLIC ---
    // Constructor: Khởi tạo với tên & mật khẩu WiFi AP
    NoiseMaster(const char* ssid, const char* password); 
    
    // Hàm begin(): Chạy trong setup()
    void begin(); 
    
    // Hàm loop(): Chạy trong loop()
    void loop(); 

    // Hàm public để lấy dữ liệu (nếu cần)
    float getDb();
    float getAngle();

  private:
    // --- PIN CẤU HÌNH (Nội bộ) ---
    const int PIN_BUTTON = 12;
    const int PIN_LED_LOW_DB = 16;
    const int PIN_LED_HIGH_DB = 17;

    // --- BIẾN NỘI BỘ ---
    const char* _ssid;
    const char* _password;
    WebServer _server; // Đối tượng WebServer

    // Biến trạng thái
    volatile float _lastDB;
    volatile float _lastAngle;
    uint8_t _slaveMacAddress[6];
    bool _slaveMacKnown;
    struct_message _incomingData;

    // Biến chống dội phím
    int _buttonState;
    int _lastButtonState;
    unsigned long _lastDebounceTime;
    const long _debounceDelay = 50;

    // --- HÀM HELPER (Nội bộ) ---
    void sendCommandToSlave();
    void checkButton();

    // --- HÀM CALLBACK (Static) ---
    // Các hàm này phải là 'static' để ESP-NOW và WebServer có thể gọi
    // Chúng sẽ dùng con trỏ 'instance' để truy cập các biến của class
    static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingDataBytes, int len);
    static void handleData();

    // Con trỏ static để các hàm callback truy cập đối tượng
    static NoiseMaster* instance; 
};

#endif
