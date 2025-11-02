/**
 * ==============================
 * ESP32 SLAVE - NHẬN LỆNH MASTER
 * ==============================
 * 📘 Chức năng:
 * - Nhận lệnh hiệu chỉnh từ Master qua ESP-NOW.
 * - Khi nhận lệnh (hoặc nhấn nút), tự động chạy hiệu chỉnh 2 giây.
 * - "Ngừng đo và gửi" trong khi đang hiệu chỉnh.
 * - Gửi giá trị dB đã hiệu chỉnh qua ESP-NOW.
 * - Bổ sung LED cảnh báo ở GPIO 16 và 17.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <driver/i2s.h>
#include <math.h>
#include <esp_wifi.h>

// --- Cấu hình I2S (Giữ nguyên) ---
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 26
#define SAMPLE_RATE 16000
#define SAMPLES 256

// --- Cấu hình LED (Giữ nguyên) ---
#define LED_ALERT_PIN 16  // LED Sáng khi dB > 15
#define LED_NORMAL_PIN 17 // LED Sáng khi dB <= 15

// --- Cấu hình Nút nhấn và Offset (Giữ nguyên) ---
#define BUTTON_PIN 12 
float db_offset = 0.0; 

// --- Cấu hình Hiệu chỉnh 2 giây (Giữ nguyên) ---
const long CALIBRATION_TIME_MS = 2000;
long cal_start_time = 0;
bool is_calibrating = false;
float cal_sum = 0.0;
int cal_count = 0;

// [MỚI] Thêm cờ (flag) để kích hoạt hiệu chỉnh từ callback
volatile bool start_calibration_flag = false;

// --- Địa chỉ MAC của ESP32 Master (Giữ nguyên) ---
uint8_t masterAddress[] = {0xB8, 0xD6, 0x1A, 0xB8, 0x9F, 0x8D};

// --- Cấu trúc dữ liệu GỬI (Giữ nguyên) ---
typedef struct struct_message_send {
  float dB;
  float angle; 
} struct_message_send;

struct_message_send soundPacket;

// --- [MỚI] Cấu trúc dữ liệu NHẬN từ Master ---
// !! Phải khớp 100% với struct "struct_command_message" bên Master !!
typedef struct struct_message_recv {
  int command; // 1 = Lệnh hiệu chỉnh
} struct_message_recv;

struct_message_recv incomingCommand;

// --- Hàm khởi tạo I2S (Giữ nguyên) ---
void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// --- Hàm tính RMS (Giữ nguyên) ---
double calculateRMS(int32_t *data, int samples) {
  double mean = 0;
  for (int i = 0; i < samples; i++) mean += data[i];
  mean /= samples;
  double sum = 0;
  for (int i = 0; i < samples; i++) {
    double x = (data[i] - mean) / 2147483648.0; 
    sum += x * x;
  }
  return sqrt(sum / samples);
}

// --- [SỬA] Callback khi gửi dữ liệu (Chữ ký hàm chuẩn ESP32) ---
void OnDataRecv(const esp_now_recv_info * recv_info, const uint8_t *incomingData, int len) {
    // 1. Lấy địa chỉ MAC của người gửi từ struct recv_info
    const uint8_t* mac_addr = recv_info->src_addr;

    // 2. Kiểm tra xem có đúng là Master gửi không
    if (memcmp(mac_addr, masterAddress, 6) != 0) {
        Serial.println("Nhận được gói tin từ MAC lạ, bỏ qua.");
        return;
    }
    
    // 3. Kiểm tra xem kích thước gói tin có phải là của "lệnh" không
    if (len == sizeof(incomingCommand)) {
        memcpy(&incomingCommand, incomingData, sizeof(incomingCommand));
        
        // 4. Nếu đúng là lệnh hiệu chỉnh (command = 1)
        if (incomingCommand.command == 1) {
            Serial.println("\n*** ĐÃ NHẬN LỆNH HIỆU CHỈNH TỪ MASTER ***");
            start_calibration_flag = true; 
        } else {
             Serial.printf("\nĐã nhận lệnh không xác định: %d\n", incomingCommand.command);
        }
    } else {
        Serial.printf("\nLỗi: Nhận gói tin không phải lệnh (Kích thước: %d).\n", len);
    }
}
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  // Chỉ in trạng thái khi KHÔNG đang hiệu chỉnh
  if (!is_calibrating) {
    if (status == ESP_NOW_SEND_SUCCESS) {
      Serial.println("📤 Gửi thành công.");
    } else {
      Serial.println("❌ Gửi thất bại!");
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_ALERT_PIN, OUTPUT);
  pinMode(LED_NORMAL_PIN, OUTPUT);
  
  setupI2S();

  WiFi.mode(WIFI_STA); 
  // [MỚI] Đặt địa chỉ MAC thủ công nếu cần (tùy chọn)
  // esp_wifi_set_mac(WIFI_IF_STA, &myMacAddress[0]);
esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Lỗi khởi tạo ESP-NOW");
    return;
  }
// [QUAN TRỌNG] Đăng ký hàm callback khi GỬI dữ liệu thành công/thất bại
    esp_now_register_send_cb(OnDataSent); 

    // [QUAN TRỌNG] Đăng ký hàm callback khi NHẬN dữ liệu
    esp_now_register_recv_cb(OnDataRecv);
  // Thêm Master làm peer (để gửi dữ liệu cho Master)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Lỗi thêm Peer");
  }

  Serial.println("✅ ESP32 SLAVE khởi động hoàn tất!");
  Serial.printf("💡 Sẵn sàng. Nhấn nút (GPIO %d) hoặc chờ lệnh Master để hiệu chỉnh.\n", BUTTON_PIN);
}
void loop() {
  size_t bytes_read;
  int32_t buffer[SAMPLES];
  i2s_read(I2S_NUM_0, (void*)buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);

  double rms = calculateRMS(buffer, SAMPLES);
  double raw_dB = 20 * log10(rms) + 120;
  
  // --- [SỬA] LOGIC HIỆU CHỈNH ---
  // Kích hoạt nếu Nút được nhấn HOẶC cờ từ Master được bật
  if ((digitalRead(BUTTON_PIN) == LOW || start_calibration_flag) && !is_calibrating) {
    is_calibrating = true;
    start_calibration_flag = false; // [MỚI] Reset cờ ngay
    cal_start_time = millis();
    cal_sum = 0.0;
    cal_count = 0;
    
    // Tắt cả 2 LED khi đang hiệu chỉnh
    digitalWrite(LED_ALERT_PIN, LOW);
    digitalWrite(LED_NORMAL_PIN, LOW);
    
    Serial.printf("\n*** BẮT ĐẦU HIỆU CHỈNH - GIỮ YÊN LẶNG (2 GIÂY) ***\n");
  }
  
  // --- XỬ LÝ KHI ĐANG HIỆU CHỈNH ---
  if (is_calibrating) {
    cal_sum += raw_dB;
    cal_count++;
    
    long elapsed = millis() - cal_start_time;
    Serial.printf("\rThu thập: %.2f giây. Đã đọc %d mẫu...", (float)elapsed / 1000.0, cal_count);

    if (elapsed >= CALIBRATION_TIME_MS) {
      is_calibrating = false;
      if (cal_count > 0) {
        db_offset = cal_sum / cal_count;
      } else {
        db_offset = 0.0;
      }
      
      Serial.printf("\n*** HIỆU CHỈNH HOÀN TẤT ***\n");
      Serial.printf("Offset mới (Mức ồn môi trường TB): %.2f dB (Trung bình từ %d mẫu)\n", db_offset, cal_count);
    }
  }
  
  // --- [SỬA] LOGIC GỬI DỮ LIỆU & LED (CHỈ CHẠY KHI KHÔNG HIỆU CHỈNH) ---
  if (!is_calibrating) {
    float adjusted_dB = raw_dB - db_offset;
    
    if (adjusted_dB < 0) {
      adjusted_dB = 0.0;
    }
    
    // Đóng gói dữ liệu
    soundPacket.dB = adjusted_dB;
    soundPacket.angle = 180.0; // Góc của Slave này (ví dụ)

    // Gửi dữ liệu
    esp_now_send(masterAddress, (uint8_t*)&soundPacket, sizeof(soundPacket));

    // Điều khiển LED
    if (adjusted_dB > 15.0) {
      digitalWrite(LED_ALERT_PIN, HIGH); 
      digitalWrite(LED_NORMAL_PIN, LOW); 
    } else {
      digitalWrite(LED_ALERT_PIN, LOW);  
      digitalWrite(LED_NORMAL_PIN, HIGH);
    }
    
    Serial.printf("Gửi: %.2f dB (Hiệu chỉnh: %.2f dB), Góc: %.2f\n", raw_dB, adjusted_dB, soundPacket.angle);
  }
  
  delay(100);
}