/*
 * NoiseMaster.h - Thư viện cho ESP32 Master Node (ĐÃ SỬA API)
 * Nhận dữ liệu ESP-NOW và cung cấp qua WebServer.
 */

#ifndef NoiseMaster_h
#define NoiseMaster_h

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <WebServer.h>

// --- CẤU TRÚC DỮ LIỆU ---
typedef struct struct_message {
    float dB;
    float angle;  
} struct_message;

typedef struct struct_command {
    int command;  
} struct_command;


class NoiseMaster {
  public:
    NoiseMaster(const char* ssid, const char* password);  
    void begin();  
    void loop();  
    float getDb();
    float getAngle();

  private:
    // --- PIN (Sửa đổi nếu cần) ---
    const int PIN_BUTTON = 12;
    const int PIN_LED_LOW_DB = 16;
    const int PIN_LED_HIGH_DB = 17;
    
    // --- Mạng & Web ---
    const char* _ssid;
    const char* _password;
    WebServer _server;  

    // --- Dữ liệu & Trạng thái ---
    volatile float _lastDB;
    volatile float _lastAngle;
    uint8_t _slaveMacAddress[6];
    bool _slaveMacKnown;
    struct_message _incomingData;

    // --- Biến Nút bấm (Debounce) ---
    int _buttonState;
    int _lastButtonState;
    unsigned long _lastDebounceTime;
    const long _debounceDelay = 50;

    // --- Hàm Private ---
    void sendCommandToSlave();
    void checkButton();

    // --- HÀM CALLBACK (Static) ---
    
    // ✅ ĐÃ SỬA: Cập nhật chữ ký hàm OnDataSent để khớp với API ESP-NOW mới
    static void OnDataSent(const esp_now_send_info_t *send_info, esp_now_send_status_t status);
    
    // ✅ ĐÃ SỬA: Cập nhật chữ ký hàm OnDataRecv
    static void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingDataBytes, int len);
    
    static void handleData();

    // Con trỏ static để truy cập instance
    static NoiseMaster* instance;  
};

#endif
