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
  {0.0,0.0},
  {6.0,0.0},
  {0.0,6.0},
  {6.6,6.6},
};
const int NUM_ZONES=8;
const float ZONE_CENTERS[NUM_ZONES][2]={
  {1.6,0.0}, // điền đầy đủ 8 khu vực vô cho tôi
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
