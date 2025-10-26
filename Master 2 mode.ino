#include "NoiseMaster.h"

// --- Cấu hình Cảm biến/Nút bấm Mode ---
#define MODE_PIN 21 // Pin 21 cho nút bấm/cảm biến

// Khởi tạo thư viện với tên và mật khẩu WiFi AP
NoiseMaster master("NoiseMaster", "12345678");

// --- Biến quản lý Mode ---
volatile int currentMode = 0;      // 0 = Chạy bình thường, 1 = Tạm dừng
volatile int touchCount = 0;       // Đếm số lần nhấn

// --- Biến chống dội (Debounce) ---
// Chúng ta dùng biến riêng cho nút mode, không lẫn với nút trong thư viện
int modeButtonState = HIGH;
int lastModeButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 200; // Tăng delay (200ms) để tránh đổi mode quá nhanh

/**
 * @brief Kiểm tra nút bấm đổi mode (chống dội)
 * Hàm này chạy độc lập với thư viện
 */
void checkModeButton() {
  // Đọc trạng thái chân (LOW khi nhấn, do dùng PULLUP)
  int reading = digitalRead(MODE_PIN);

  // Nếu trạng thái thay đổi (có nhiễu hoặc nhấn) -> reset bộ đếm thời gian
  if (reading != lastModeButtonState) {
    lastDebounceTime = millis();
  }

  // Chờ cho đến khi trạng thái ổn định (hết thời gian debounce)
  if ((millis() - lastDebounceTime) > debounceDelay) {
    
    // Nếu trạng thái đã ổn định VÀ nó khác với trạng thái trước đó
    if (reading != modeButtonState) {
      modeButtonState = reading;

      // Chỉ kích hoạt khi nút VỪA ĐƯỢC NHẤN (trạng thái LOW)
      if (modeButtonState == LOW) {
        touchCount++; // Tăng số lần nhấn
        
        // Đảo mode (toggle)
        if (currentMode == 0) {
          currentMode = 1;
          Serial.println("=========================================");
          Serial.println("Chuyển sang Mode 1 (Tạm dừng thư viện)");
          Serial.printf("Tổng số lần nhấn: %d\n", touchCount);
          Serial.println("=========================================");
        } else {
          currentMode = 0;
          Serial.println("=========================================");
          Serial.println("Chuyển sang Mode 0 (Chạy bình thường)");
          Serial.printf("Tổng số lần nhấn: %d\n", touchCount);
          Serial.println("=========================================");
        }
      }
    }
  }
  
  // Lưu lại trạng thái cuối cùng
  lastModeButtonState = reading;
}


void setup() {
  Serial.begin(115200);

  // --- Khởi tạo Pin Mode ---
  // Dùng INPUT_PULLUP để ổn định nút, chống "floating"
  // Nút/cảm biến của bạn cần được nối giữa CHÂN 21 và GND
  pinMode(MODE_PIN, INPUT_PULLUP);
  
  // Chỉ cần gọi hàm begin() của thư viện
  master.begin(); 
  
  Serial.println("Setup chính hoàn tất. Đang ở Mode 0 (Chạy bình thường).");
}

void loop() {
  // 1. Luôn kiểm tra nút bấm đổi mode (bất kể đang ở mode nào)
  checkModeButton();

  // 2. Chỉ chạy thư viện nếu ở mode 0
  if (currentMode == 0) {
    master.loop(); // Chạy thư viện (nhận ESP-NOW, xử lý web, check nút bấm của thư viện)
  } else {
    // Đang ở Mode 1 (Tạm dừng)
    // Thư viện master.loop() không được gọi
    // -> ESP-NOW và WebServer sẽ không được xử lý.
    // Thêm delay nhỏ để CPU nghỉ
    delay(10); 
  }
}
