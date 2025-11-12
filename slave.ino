/*
 * CODE CHO 4 "SLAVE" (ESP32-C3 SUPER MINI)
 * Nhiệm vụ: Tính P_mượt và gửi qua ESP-NOW cho Master.
 */
volatile bool heThongDangBat = true; // Mặc định là Bật khi khởi động
// --- 1. THÊM CÁC THƯ VIỆN CẦN THIẾT ---
#include "driver/adc.h" // Cho ADC C3
#include <esp_now.h>   // Cho ESP-NOW
#include <WiFi.h>      // Cho ESP-NOW
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
// --- 2. CẤU HÌNH CẢM BIẾN (Giống Giai đoạn 1) ---
const int MIC_PIN = 2; // (Hoặc 3, 4... tùy bạn cắm chân nào)
const int SAMPLE_WINDOWS = 50;
const int ADC_MAX = 4095;
const float ALPHA = 0.2; 
float P_cu = 0.0;       

// --- 3. CẤU HÌNH ESP-NOW ---

// !!! THAY THẾ ĐỊA CHỈ NÀY BẰNG ĐỊA CHỈ MAC MASTER CỦA BẠN (Lấy ở Bước 1)
uint8_t masterAddress[] = {0xB8, 0xD6, 0x1A, 0xB8, 0x9F, 0x8C}; 

// Cấu trúc dữ liệu để gửi đi
// Master cần biết 'id' (slave nào) và 'p_value' (giá trị P_mượt)
typedef struct struct_message {
  int id;
  float p_value;
} struct_message;

// Tạo một biến dữ liệu
struct_message myData;
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  // Bỏ comment dòng dưới nếu bạn muốn xem kết quả
  Serial.print("Trang thai gui P_muot: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Thanh Cong" : "That Bai");
}
void OnDataRecv(const esp_now_recv_info * info, const uint8_t *incomingData, int len) {
  int command;
  memcpy(&command, incomingData, sizeof(command));

  if (command == 0) {
    heThongDangBat = false; // Master bảo Tắt
  } else {
    heThongDangBat = true;  // Master bảo Bật
  }
}
// --- 4. HÀM TÍNH P_THÔ (Giữ nguyên) ---
float tinh_P_tho() {
  unsigned long bat_dau = millis();
  int sample_max = 0;
  int sample_min = ADC_MAX;
  int sample;
  while (millis() < bat_dau + SAMPLE_WINDOWS) {
    sample = adc1_get_raw(ADC1_CHANNEL_2); // Dùng hàm cấp thấp (Giả sử MIC_PIN = 2)
    if (sample > sample_max) sample_max = sample;
    if (sample < sample_min) sample_min = sample;
    yield(); // Tránh Crash WDT
  }
  return (float)(sample_max - sample_min);
}

// --- 5. HÀM SETUP (Thiết lập ADC và ESP-NOW) ---
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //tắt giám sát sụt áp
  Serial.begin(115200);
  // Cài đặt ADC cho C3 (Như bạn đã làm)
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_2, ADC_ATTEN_DB_11); // Kênh 2 là GPIO 2
  // Cài đặt ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Loi khoi tao ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);// lắng nghe
  // Đăng ký hàm callback GỬI
  esp_now_register_send_cb(OnDataSent);
  // Đăng ký "bạn bè" (Master)
  esp_now_peer_info_t peerInfo = {}; // Khởi tạo struct peerInfo
  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Loi dang ky Master");
    return;
  }
}

// --- 6. HÀM LOOP (Tính toán và Gửi) ---
void loop() {
  // Tính P_mượt (Giống Giai đoạn 1)
  if (heThongDangBat){
  float P_thuc_te = tinh_P_tho();
  float P_now = (ALPHA * P_thuc_te) + ((1.0 - ALPHA) * P_cu);
  P_cu = P_now;

  // Gán dữ liệu vào biến struct
  myData.id = 4; // !!! THAY SỐ NÀY (1, 2, 3, 4) CHO TỪNG CON SLAVE
  myData.p_value = P_now;

  // Gửi dữ liệu P_mượt đến Master
  esp_now_send(masterAddress, (uint8_t *) &myData, sizeof(myData));}

  delay(100); // Gửi 10 lần mỗi giây
}
