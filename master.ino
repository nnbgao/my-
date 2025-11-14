#include <esp_now.h>
#include <WiFi.h>
#include <cmath>
// Mảng 4 phần tử để lưu 4 giá trị P_mượt
// P_muot_values[0] = giá trị của Slave 1
// P_muot_values[1] = giá trị của Slave 2
// ...
volatile float P_muot_values[4] = {0.0, 0.0, 0.0, 0.0};
const int NUM_MICS=4;
const float MIC_POSITIONS[NUM_MICS][2]={
  {0.0,0.0}, // Mic 1 (Slave 1) mỗi ô trong lớp học là 1 đơn vị
  {18.0,0.0}, // Mic 2 (Slave 2)
  {0.0,12.0}, // Mic 3 (Slave 3)
  {18.0,12.0}, // Mic 4 (Slave 4)
};
const int NUM_ZONES=8;
const float ZONE_CENTERS[NUM_ZONES][2]={
  {4.72, 5.0}, // Zone 1 (Như code gốc của bạn)
  {4.72, 9.85}, // Zone 2 (Trên trục X, gần mic 1)
  {8.5, 5.0}, // Zone 3 (Trên trục Y, giữa mic 0-2)
  {8.5, 9.85}, // Zone 4 (Trên trục X=6, giữa mic 1-3)
  {13.12, 5.0}, // Zone 5 (Trên trục Y=6, giữa mic 2-3)
  {13.12, 9.85}, // Zone 6 (Tâm của các mic)
  {17.95, 5.0}, // Zone 7 (Góc phần tư 2)
  {17.95, 9.85}, // Zone 8 (Góc phần tư 4)
};
double tinh_khoang_cach(double vung_x, double vung_y, double mic_x, double mic_y){
  double delta_x=vung_x-mic_x;
  double delta_y=vung_y-mic_y;

  double dinh_b=pow(delta_x,2)+pow(delta_y,2);
  double distance=sqrt(dinh_b);
  return distance;
};
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
float P_nguong=3600;
  // Giai đoạn 3 (IDW và NeoPixel) sẽ được thêm vào đây sau
  float P_est_zones[NUM_ZONES];
  for (int i=0; i<NUM_ZONES; i++){
    float vung_x=ZONE_CENTERS[i][0];
    float vung_y=ZONE_CENTERS[i][1];
   double tong_trong_so=0.0;
   double tong_P_nhan_trong_so=0.0;
    for (int j=0; j<NUM_MICS; j++){
    float mic_x=MIC_POSITIONS[j][0];
    float mic_y=MIC_POSITIONS[j][1];
    double d=tinh_khoang_cach(vung_x, vung_y, mic_x, mic_y);
    double w=0.0;
    if (d>0){
      w=1.0/d;
    }
    else{
      w=10000.0;
    }
    tong_trong_so+=w;
    tong_P_nhan_trong_so+=P_muot_values[j]*w;
    }
    P_est_zones[i]=tong_P_nhan_trong_so/ tong_trong_so;
    
    if (P_est_zones[i]>P_nguong){
      Serial.printf("-Vung %d ỒN (P=%.0f)",i+1,P_est_zones[i]);
    }
  }
  Serial.println();
  delay(200); // Cập nhật màn hình 5 lần/giây
}