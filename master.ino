#include <esp_now.h>
#include <WiFi.h>

// Mảng 4 phần tử để lưu 4 giá trị P_mượt
// P_muot_values[0] = giá trị của Slave 1
// P_muot_values[1] = giá trị của Slave 2
// ...
volatile float P_muot_values[4] = {0.0, 0.0, 0.0, 0.0};

// Cấu trúc dữ liệu (PHẢI GIỐNG HỆT BÊN SLAVE)
typedef struct struct_message {
  int id;
  float p_value;
} struct_message;

// Hàm Callback (Hàm gọi lại) - Sẽ tự động chạy khi nhận được dữ liệu
// DÙNG CÚ PHÁP MỚI CHO THƯ VIỆN ESP32 MỚI
void OnDataRecv(const esp_now_recv_info * info, const uint8_t *incomingData, int len) {
  
  struct_message myData;
  memcpy(&myData, incomingData, sizeof(myData));

  // Phân loại dữ liệu dựa trên ID mà Slave gửi
  int slave_id = myData.id;
  float p_value = myData.p_value;

  // Cập nhật giá trị vào đúng mảng
  // (Trừ 1 vì ID bắt đầu từ 1, nhưng chỉ số mảng bắt đầu từ 0)
  if (slave_id >= 1 && slave_id <= 4) {
    P_muot_values[slave_id - 1] = p_value;
  }
}

// --- HÀM SETUP ---
void setup() {
  Serial.begin(115200);
  Serial.println("Day la MASTER. Dang lang nghe...");
  
  WiFi.mode(WIFI_STA);

  // Khởi tạo ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Loi khoi tao ESP-NOW");
    return;
  }
  
  // Đăng ký Hàm Callback Nhận Dữ liệu (Quan trọng nhất)
  esp_now_register_recv_cb(OnDataRecv);
}

// --- HÀM LOOP ---
void loop() {
  // In 4 giá trị P_mượt ra Serial Monitor (Để kiểm tra)
  Serial.printf("P1: %.0f | P2: %.0f | P3: %.0f | P4: %.0f \n",
                P_muot_values[0],
                P_muot_values[1],
                P_muot_values[2],
                P_muot_values[3]);

  // Giai đoạn 3 (IDW và NeoPixel) sẽ được thêm vào đây sau
  
  delay(200); // Cập nhật màn hình 5 lần/giây
}