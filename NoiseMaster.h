/*
 * NoiseMaster.h - Thư viện cho ESP32 Master Node (ĐÃ CẬP NHẬT v3.0)
 * Nhận dữ liệu ESP-NOW và cung cấp qua WebServer.
 */

#ifndef NoiseMaster_h
#define NoiseMaster_h

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h> // Đảm bảo include ở đây
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
    NoiseMaster(const char* ssid, const char* password); 
    void begin(); 
    void loop(); 
    float getDb();
    float getAngle();

  private:
    const int PIN_BUTTON = 12;
    const int PIN_LED_LOW_DB = 16;
    const int PIN_LED_HIGH_DB = 17;

    const char* _ssid;
    const char* _password;
    WebServer _server; 

    volatile float _lastDB;
    volatile float _lastAngle;
    uint8_t _slaveMacAddress[6];
    bool _slaveMacKnown;
    struct_message _incomingData;

    int _buttonState;
    int _lastButtonState;
    unsigned long _lastDebounceTime;
    const long _debounceDelay = 50;

    void sendCommandToSlave();
    void checkButton();

    // --- HÀM CALLBACK (Static) ---
    static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    
    // ✨ ĐÃ SỬA: Thay đổi chữ ký hàm OnDataRecv
    static void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingDataBytes, int len);
    
    static void handleData();

    static NoiseMaster* instance; 
};

#endif
