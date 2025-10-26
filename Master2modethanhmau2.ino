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
// --- (Đã xóa 'healthReducedThisCycle' vì logic mới không cần) ---
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
          // --- CHUYỂN SANG MODE 1 ---
          currentMode = 1;
          master.pause(true); // <-- SỬA: Ra lệnh thư viện DỪNG
          Serial.println("=========================================");
          Serial.println("Chuyển sang Mode 1 (Tạm dừng thư viện)");
          Serial.printf("Tổng số lần nhấn: %d\n", touchCount);
          Serial.println("=========================================");
        } else {
          // --- CHUYỂN SANG MODE 0 ---
          currentMode = 0;
          master.pause(false); // <-- SỬA: Ra lệnh thư viện CHẠY
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

  // 1. Kiểm tra trạng thái dB
  if (currentDb < 25.0) {
    // (dB Thấp) -> Reset bộ đếm
    isDbHigh = false;
    ledHighStartTime = 0;
  } else {
    // (dB Cao >= 25)
    if (isDbHigh == false) {
      // (Vừa vượt ngưỡng) -> Bắt đầu đếm giờ
      isDbHigh = true;
      ledHighStartTime = millis();
    }
  }

  // 2. ✨ LOGIC TRỪ MÁU ĐỊNH KỲ (Đã sửa)
  // Chỉ trừ máu nếu: dB đang cao VÀ đã qua 2 giây
  if (isDbHigh && (millis() - ledHighStartTime > 2000)) {
    health--; // Trừ 1 máu
    if (health < 0) health = 0; // Không cho máu âm
      
    // In ra cảnh báo
    Serial.println("-------------------------------------------------");
    Serial.printf("!!! dB Cao > 2 giây. Máu giảm! HP còn: %d/100 !!!\n", health);
    Serial.println("-------------------------------------------------");

    // QUAN TRỌNG: Reset lại bộ đếm
    // Để 2 giây sau nó kiểm tra và trừ tiếp (nếu dB vẫn cao)
    ledHighStartTime = millis(); 
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
    // (Thư viện sẽ tự xử lý loop() và ngắt OnDataRecv)
    master.loop(); 

    // BƯỚC 2: Kiểm tra thanh máu.
    checkHealth(); 
    
  } else {
    // Đang ở Mode 1 (Tạm dừng)
    // Thư viện đã được pause, không làm gì cả
    delay(10); 
  }
}
