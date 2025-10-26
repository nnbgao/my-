#include "NoiseMaster.h"

// --- Cấu hình Cảm biến/Nút bấm Mode ---
#define MODE_PIN 21 // Pin 21 cho nút bấm/cảm biến

// Khởi tạo thư viện với tên và mật khẩu WiFi AP
NoiseMaster master("NoiseMaster", "12345678");

// --- Biến quản lý Mode ---
volatile int currentMode = 0;      // 0 = Chạy bình thường, 1 = Tạm dừng
volatile int touchCount = 0;       // Đếm số lần nhấn

// --- Biến chống dội (Debounce) ---
int modeButtonState = HIGH;
int lastModeButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 200; 

// --------------------------------------------------
// --- LOGIC THANH MÁU (HP) - Đặt tại file .ino ---
// --------------------------------------------------
int health = 100;
unsigned long ledHighStartTime = 0; // Thời điểm dB bắt đầu >= 25
bool isDbHigh = false; // Trạng thái dB có đang >= 25 không
bool healthReducedThisCycle = false; // Cờ để đảm bảo chỉ trừ máu 1 lần
// --------------------------------------------------


/**
 * @brief Kiểm tra nút bấm đổi mode (chống dội)
 * Hàm này chạy độc lập với thư viện
 */
void checkModeButton() {
  int reading = digitalRead(MODE_PIN);
  if (reading != lastModeButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != modeButtonState) {
      modeButtonState = reading;
      if (modeButtonState == LOW) {
        touchCount++; 
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
  lastModeButtonState = reading;
}

/**
 * @brief Kiểm tra thanh máu (HP)
 * Hàm này chạy trong loop() và lấy dữ liệu từ thư viện
 */
void checkHealth() {
  // Lấy giá trị dB hiện tại từ thư viện
  float currentDb = master.getDb(); 

  // 1. Kiểm tra trạng thái dB (để bật/tắt bộ đếm giờ)
  if (currentDb < 25.0) {
    // (dB Thấp) -> Reset bộ đếm
    isDbHigh = false;
    ledHighStartTime = 0;
    healthReducedThisCycle = false; // Sẵn sàng cho lần trừ máu tiếp theo
  } else {
    // (dB Cao >= 25) -> Bắt đầu đếm giờ (nếu chưa đếm)
    if (isDbHigh == false) {
      isDbHigh = true;
      ledHighStartTime = millis();
    }
  }

  // 2. Xử lý trừ máu (nếu bộ đếm đang chạy)
  // Chỉ kiểm tra nếu dB đang Cao VÀ chưa bị trừ máu trong chu kỳ này
  if (isDbHigh && !healthReducedThisCycle) {
    unsigned long duration = millis() - ledHighStartTime;

    // Nếu đã Cao > 2 giây (2000ms)
    if (duration > 2000) {
      health--; // Trừ 1 máu
      if (health < 0) health = 0; // Không cho máu âm

      healthReducedThisCycle = true; // Đánh dấu là đã trừ máu lần này
      
      // In ra cảnh báo
      Serial.println("-------------------------------------------------");
      Serial.printf("!!! dB Cao > 2 giây. Máu giảm! HP còn: %d/100 !!!\n", health);
      Serial.println("-------------------------------------------------");
    }
  }
}


void setup() {
  Serial.begin(115200);

  // --- Khởi tạo Pin Mode ---
  pinMode(MODE_PIN, INPUT_PULLUP);
  
  // Gọi hàm begin() của thư viện
  master.begin(); 
  
  Serial.println("Setup chính hoàn tất. Đang ở Mode 0 (Chạy bình thường).");
  Serial.printf("Thanh máu khởi tạo: %d/100 HP\n", health);
}

void loop() {
  // 1. Luôn kiểm tra nút bấm đổi mode (bất kể đang ở mode nào)
  checkModeButton();

  // 2. Chỉ chạy thư viện nếu ở mode 0
  if (currentMode == 0) {
    // BƯỚC 1: Chạy thư viện. 
    // (Việc này sẽ cập nhật giá trị dB bên trong thư viện)
    master.loop(); 

    // BƯỚC 2: Kiểm tra thanh máu.
    // (Hàm này sẽ gọi master.getDb() để lấy giá trị mới nhất)
    checkHealth(); 
    
  } else {
    // Đang ở Mode 1 (Tạm dừng)
    delay(10); 
  }
}
