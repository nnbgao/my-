/**
 * ==============================
 * ESP32 SLAVE - (ĐÃ SỬA LỖI API & VOLATILE)
 * ==============================
 * 📘 Chức năng:
 * - [span_0](start_span)Gửi (dB, Angle) cho Master.[span_0](end_span)
 * - [span_1](start_span)NHẬN lệnh (command 1) từ Master để hiệu chỉnh độ ồn.[span_1](end_span)
 *
 * 🔧 Cập nhật:
 * - Thêm lại struct_command bị thiếu.
 * - Thêm lại khối đăng ký esp_now_register_cbs bị thiếu.
 * - Xóa include <NoiseMaster.h> thừa.
 */

// #include <NoiseMaster.h> // <-- ĐÃ XÓA
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
[span_2](start_span)const long CALIBRATION_TIME_MS = 5000;[span_2](end_span) [span_3](start_span)// 5 giây[span_3](end_span)
[span_4](start_span)volatile long cal_start_time = 0;[span_4](end_span)
[span_5](start_span)volatile bool is_calibrating = false;[span_5](end_span)
[span_6](start_span)volatile float cal_sum = 0.0;[span_6](end_span)
[span_7](start_span)volatile int cal_count = 0;[span_7](end_span)

// --- Địa chỉ MAC của ESP32 Master ---
uint8_t masterAddress[] = {0xB8, 0xD6, 0x1A, 0xB8, 0x9F, 0x8D};

[span_8](start_span)// --- Cấu trúc dữ liệu GỬI ---[span_8](end_span)
typedef struct {
  float dB;
  float angle;
} SoundData;

[span_9](start_span)SoundData soundPacket;[span_9](end_span)

[span_10](start_span)// --- Cấu trúc dữ liệu NHẬN từ Master ---[span_10](end_span)
// ✨ THÊM LẠI STRUCT BỊ MẤT
typedef struct struct_command {
  int command;
} struct_command;

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
  [span_11](start_span)i2s_pin_config_t pin_config = {[span_11](end_span)
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  [span_12](start_span)};[span_12](end_span)
  [span_13](start_span)i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);[span_13](end_span)
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// --- Hàm tính RMS --- (Không thay đổi)
double calculateRMS(int32_t *data, int samples) {
  double mean = 0;
  [span_14](start_span)for (int i = 0; i < samples; i++) mean += data[i];[span_14](end_span)
  mean /= samples;
  double sum = 0;
  [span_15](start_span)for (int i = 0; i < samples; i++) {[span_15](end_span)
    double x = (data[i] - mean) / 2147483648.0;
    [span_16](start_span)sum += x * x;[span_16](end_span)
  }
  return sqrt(sum / samples);
[span_17](start_span)}

// --- Callback khi GỬI dữ liệu XONG ---
// (Giữ nguyên API mới theo yêu cầu)
void OnDataSent(const esp_now_send_info_t *send_info, esp_now_send_status_t status)
 {
  if (!is_calibrating) {
    if (status == ESP_NOW_SEND_SUCCESS) {
      Serial.println("📤 Gửi thành công");
    } else {[span_17](end_span)
      [span_18](start_span)Serial.println("❌ Gửi thất bại");[span_18](end_span)
    }
  }
}

// --- Callback khi NHẬN dữ liệu ---
// (Giữ nguyên API mới theo yêu cầu)
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingDataBytes, int len) {
  struct_command cmd; // <-- Dòng này bây giờ đã hợp lệ
  [span_19](start_span)// Kiểm tra độ dài gói tin[span_19](end_span)
  if (len == sizeof(cmd)) {
    memcpy(&cmd, incomingDataBytes, sizeof(cmd));
    [span_20](start_span)Serial.print("\n[NHẬN LỆNH TỪ MASTER]: ");[span_20](end_span)
    Serial.println(cmd.command);

    // Kiểm tra nếu lệnh là 1
    if (cmd.command == 1) {
      Serial.println("-> Lệnh 1! Bắt đầu hiệu chỉnh...");
      [span_21](start_span)// Kích hoạt hiệu chỉnh tự động[span_21](end_span)
      [span_22](start_span)is_calibrating = true;[span_22](end_span)
      [span_23](start_span)cal_start_time = millis();[span_23](end_span)
      [span_24](start_span)cal_sum = 0.0;[span_24](end_span)
      [span_25](start_span)cal_count = 0;[span_25](end_span)
      Serial.printf("\n*** BẮT ĐẦU HIỆU CHỈNH (TỪ XA) - GIỮ YÊN LẶNG (5 GIÂY) ***\n");
    [span_26](start_span)}
  } else {
    Serial.println("Lỗi: Nhận được gói lệnh không đúng kích thước.");[span_26](end_span)
  }
}

void setup() {
  Serial.begin(115200);

  setupI2S();

  WiFi.mode(WIFI_STA);

  // Ép Slave chạy trên Kênh 1 (đồng bộ với Master)
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  [span_27](start_span)if (esp_now_init() != ESP_OK) {[span_27](end_span)
    Serial.println("❌ Lỗi khởi tạo ESP-NOW");
    return;
  [span_28](start_span)}

  // --- ✨ THÊM LẠI KHỐI ĐĂNG KÝ BỊ MẤT ---[span_28](end_span)
  esp_now_cbs_t cbs;
  cbs.recv_cb = OnDataRecv; // Đăng ký hàm NHẬN
  cbs.send_cb = OnDataSent; // Đăng ký hàm GỬI

  if (esp_now_register_cbs(&cbs) != ESP_OK) {
    Serial.println("❌ Lỗi đăng ký Callbacks (cbs)");
    return;
  }
  // --- Hết phần sửa ---

  // Thêm Master vào Peer (Đã sửa channel = 1)
  esp_now_peer_info_t peerInfo = {};
  [span_29](start_span)memcpy(peerInfo.peer_addr, masterAddress, 6);[span_29](end_span)
  peerInfo.channel = 1; // Đảm bảo khớp kênh
  peerInfo.encrypt = false;
  [span_30](start_span)if (esp_now_add_peer(&peerInfo) != ESP_OK) {[span_30](end_span)
    [span_31](start_span)Serial.println("❌ Lỗi thêm Peer");[span_31](end_span)
  }

  [span_32](start_span)Serial.println("✅ ESP32 SLAVE khởi động hoàn tất! (Sẵn sàng nhận lệnh 1)");[span_32](end_span)
  [span_33](start_span)Serial.println("💡 Chờ lệnh hiệu chỉnh (5 giây) từ Master.");[span_33](end_span)
}

void loop() {
  size_t bytes_read;
  int32_t buffer[SAMPLES];
  [span_34](start_span)i2s_read(I2S_NUM_0, (void*)buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);[span_34](end_span)

  double rms = calculateRMS(buffer, SAMPLES);
  double raw_dB = 20 * log10(rms) + 120; [span_35](start_span)// +120 là offset cho INMP441[span_35](end_span)

  // --- Xử lý hiệu chỉnh ---
  [span_36](start_span)if (is_calibrating) { // <-- Sẽ đọc giá trị 'volatile' mới nhất[span_36](end_span)
    cal_sum += raw_dB;
    [span_37](start_span)cal_count++;[span_37](end_span)

    long elapsed = millis() - cal_start_time;
    Serial.printf("\rThu thập: %.2f giây. Đã đọc %d mẫu...", (float)elapsed / 1000.0, cal_count);
    [span_38](start_span)if (elapsed >= CALIBRATION_TIME_MS) {[span_38](end_span) [span_39](start_span)// Sẽ kiểm tra với 5000ms[span_39](end_span)
      is_calibrating = false;
      [span_40](start_span)if (cal_count > 0) {[span_40](end_span)
        [span_41](start_span)db_offset = cal_sum / cal_count;[span_41](end_span)
      [span_42](start_span)} else {[span_42](end_span)
        [span_43](start_span)db_offset = 0.0;[span_43](end_span)
      }

      [span_44](start_span)Serial.printf("\n*** HIỆU CHỈNH HOÀN TẤT ***\n");[span_44](end_span)
      [span_45](start_span)Serial.printf("Offset mới (Mức ồn môi trường TB): %.2f dB (Trung bình từ %d mẫu)\n", db_offset, cal_count);[span_45](end_span)
    }

    // Tạm dừng gửi khi đang hiệu chỉnh
    [span_46](start_span)return;[span_46](end_span)
  }

  // Fix lỗi race condition
  if (is_calibrating) {
      [span_47](start_span)return;[span_47](end_span)
  }

  // --- GỬI DỮ LIỆU ĐÃ HIỆU CHỈNH ---
  float adjusted_dB = raw_dB - db_offset;
  [span_48](start_span)if (adjusted_dB < 0) {[span_48](end_span)
    adjusted_dB = 0.0;
  }

  soundPacket.dB = adjusted_dB;
  [span_49](start_span)soundPacket.angle = 180.0;[span_49](end_span) [span_50](start_span)// Gửi góc 180[span_50](end_span)

  esp_err_t result = esp_now_send(masterAddress, (uint8_t*)&soundPacket, sizeof(soundPacket));
  [span_51](start_span)Serial.printf("Gửi: %.2f dB (Raw: %.2f dB | Offset: %.2f dB)\n", adjusted_dB, raw_dB, db_offset);[span_51](end_span)

  delay(50); [span_52](start_span)// Thêm delay nhỏ để ổn định[span_52](end_span)
}
