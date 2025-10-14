/*
  === ESP32 + INMP441 Sound Level Monitor ===
  📌 Chức năng:
  - Đọc tín hiệu âm thanh từ micro INMP441 qua I2S
  - Tính toán FFT và dB trung bình
  - Xuất dữ liệu ra Serial Plotter để xem sóng âm + mức dB real-time

  === Sơ đồ kết nối ===
  | INMP441 | ESP32 NodeMCU-32S |
  |----------|-------------------|
  | VCC      | 3.3V              |
  | GND      | GND               |
  | SD       | GPIO33            |  // DOUT → D33
  | WS (LRCL)| GPIO25            |  // L/R Clock → D25
  | SCK (BCLK)| GPIO26           |  // Bit Clock → D26
  | L/R      | GND               |  // chọn kênh trái
*/

#include <Arduino.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>  // chú ý chữ 'a' viết thường nếu thư viện của bạn là 'arduinoFFT'

#define I2S_WS  25   // LRCL
#define I2S_SD  33   // DOUT
#define I2S_SCK 26   // BCLK

#define SAMPLE_RATE 16000
#define SAMPLES 256   // giảm còn 256 để cập nhật nhanh hơn

double vReal[SAMPLES];
double vImag[SAMPLES];

ArduinoFFT<double> FFT;

// ==== Thiết lập I2S ====
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

// ==== Hàm tính dB ====
double calculateDB(double *data, int samples) {
  double sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += data[i] * data[i];
  }
  double rms = sqrt(sum / samples);
  double db = 20 * log10(rms / 0.00002);  // quy chuẩn 20 µPa
  return db;
}

// ==== SETUP ====
void setup() {
  Serial.begin(115200);
  delay(1000);
  setupI2S();

  Serial.println("=== ESP32 + INMP441: Real-time FFT & dB ===");
  Serial.println("Amplitude\tDecibel(dB)");
}

// ==== LOOP ====
void loop() {
  size_t bytes_read;
  int32_t buffer[SAMPLES];

  // Đọc dữ liệu từ INMP441
  i2s_read(I2S_NUM_0, (void*)buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);

  for (int i = 0; i < SAMPLES; i++) {
    vReal[i] = (double)buffer[i] / 2147483648.0; // chuẩn hóa -1.0 đến 1.0
    vImag[i] = 0;
  }

  // FFT mỗi 100ms, còn dB cập nhật liên tục
  static unsigned long lastFFT = 0;
  if (millis() - lastFFT > 100) {
    FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, SAMPLES);
    lastFFT = millis();
  }

  double avgDB = calculateDB(vReal, SAMPLES);

  // In ra hai cột để xem trên Serial Plotter
  Serial.print(vReal[0] * 1000); // biên độ sóng
  Serial.print("\t");
  Serial.println(avgDB);         // dB trung bình

  delay(5);  // cập nhật nhanh ~200 Hz
}
