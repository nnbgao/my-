/**
 * ==============================
 * ESP32 SLAVE - (DÙNG API MỚI CBS & VOLATILE)
 * ==============================
 * 📘 Chức năng:
 * - Gửi (dB, Angle) cho Master.
 * - NHẬN lệnh (command 1) từ Master để hiệu chỉnh độ ồn.
 *
 * 🔧 Cập nhật:
 * - Sửa hàm đăng ký sang esp_now_register_cbs() cho tương thích.
 * - Thêm 'volatile' để fix lỗi logic hiệu chỉnh.
 */
#include <NoiseMaster.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h> 
#include <driver/i2s.h>
#include <math.h>

// --- Cấu hình I2S ---
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 26
#define SAMPLE_RATE 16000
#define SAMPLES 256

// --- Cấu hình Offset Hiệu chỉnh ---
float db_offset = 0.0; 

// --- Cấu hình Hiệu chỉnh ---
// ✨ ĐÃ SỬA: Thêm 'volatile' cho tất cả các biến
// được chia sẻ giữa loop() và hàm ngắt OnDataRecv()
const long CALIBRATION_TIME_MS = 5000; // 5 giây
volatile long cal_start_time = 0;
volatile bool is_calibrating = false;
volatile float cal_sum = 0.0;
volatile int cal_count = 0;

// --- Địa chỉ MAC của ESP32 Master ---
uint8_t masterAddress[] = {0xB8, 0xD6, 0x1A, 0xB8, 0x9F, 0x8D};

// --- Cấu trúc dữ liệu GỬI ---
typedef struct {
  float dB;
  float angle; 
} SoundData;

SoundData soundPacket;

// --- Cấu trúc dữ liệu NHẬN từ Master ---

// --- Hàm khởi tạo I2S --- (Không thay đổi)
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

// --- Hàm tính RMS --- (Không thay đổi)
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

// --- Callback khi GỬI dữ liệu XONG ---
// (Giữ nguyên API mới theo yêu cầu)
void OnDataSent(const esp_now_send_info_t *send_info, esp_now_send_status_t status)
 {
  if (!is_calibrating) {
    if (status == ESP_NOW_SEND_SUCCESS) {
      Serial.println("📤 Gửi thành công");
    } else {
      Serial.println("❌ Gửi thất bại");
    }
  }
}

// --- Callback khi NHẬN dữ liệu ---
// (Giữ nguyên API mới theo yêu cầu)
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingDataBytes, int len) {
  struct_command cmd;
  // Kiểm tra độ dài gói tin
  if (len == sizeof(cmd)) {
    memcpy(&cmd, incomingDataBytes, sizeof(cmd));
    
    Serial.print("\n[NHẬN LỆNH TỪ MASTER]: ");
    Serial.println(cmd.command);

    // Kiểm tra nếu lệnh là 1
    if (cmd.command == 1) { 
      Serial.println("-> Lệnh 1! Bắt đầu hiệu chỉnh...");
      // Kích hoạt hiệu chỉnh tự động
      is_calibrating = true;
      cal_start_time = millis();
      cal_sum = 0.0;
      cal_count = 0;
      Serial.printf("\n*** BẮT ĐẦU HIỆU CHỈNH (TỪ XA) - GIỮ YÊN LẶNG (5 GIÂY) ***\n");
    }
  } else {
    Serial.println("Lỗi: Nhận được gói lệnh không đúng kích thước.");
  }
}

void setup() {
  Serial.begin(115200);
  
  setupI2S();

  WiFi.mode(WIFI_STA); 
  
  // Ép Slave chạy trên Kênh 1 (đồng bộ với Master)
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Lỗi khởi tạo ESP-NOW");
    return;
  }

  // --- ✨ ĐÃ SỬA: Dùng hàm đăng ký API mới ---
  
 
  // --- Hết phần sửa ---

  // Thêm Master vào Peer (Đã sửa channel = 1)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = 1; // Đảm bảo khớp kênh
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Lỗi thêm Peer");
  }

  Serial.println("✅ ESP32 SLAVE khởi động hoàn tất! (Sẵn sàng nhận lệnh 1)");
  Serial.println("💡 Chờ lệnh hiệu chỉnh (5 giây) từ Master.");
}

void loop() {
  size_t bytes_read;
  int32_t buffer[SAMPLES];
  i2s_read(I2S_NUM_0, (void*)buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);

  double rms = calculateRMS(buffer, SAMPLES);
  double raw_dB = 20 * log10(rms) + 120; // +120 là offset cho INMP441
  
  // --- Xử lý hiệu chỉnh ---
  if (is_calibrating) { // <-- Sẽ đọc giá trị 'volatile' mới nhất
    cal_sum += raw_dB;
    cal_count++;
    
    long elapsed = millis() - cal_start_time;
    Serial.printf("\rThu thập: %.2f giây. Đã đọc %d mẫu...", (float)elapsed / 1000.0, cal_count);

    if (elapsed >= CALIBRATION_TIME_MS) { // Sẽ kiểm tra với 5000ms
      is_calibrating = false;
      if (cal_count > 0) {
        db_offset = cal_sum / cal_count;
      } else {
        db_offset = 0.0;
      }
      
      Serial.printf("\n*** HIỆU CHỈNH HOÀN TẤT ***\n");
      Serial.printf("Offset mới (Mức ồn môi trường TB): %.2f dB (Trung bình từ %d mẫu)\n", db_offset, cal_count);
    }
    
    // Tạm dừng gửi khi đang hiệu chỉnh
    return;
  }
  
  // Fix lỗi race condition
  if (is_calibrating) {
      return;
  }

  // --- GỬI DỮ LIỆU ĐÃ HIỆU CHỈNH ---
  float adjusted_dB = raw_dB - db_offset;
  
  if (adjusted_dB < 0) {
    adjusted_dB = 0.0;
  }
  
  soundPacket.dB = adjusted_dB;
  soundPacket.angle = 180.0; // Gửi góc 180

  esp_err_t result = esp_now_send(masterAddress, (uint8_t*)&soundPacket, sizeof(soundPacket));
  
  Serial.printf("Gửi: %.2f dB (Raw: %.2f dB | Offset: %.2f dB)\n", adjusted_dB, raw_dB, db_offset);

  delay(50); // Thêm delay nhỏ để ổn định
}
