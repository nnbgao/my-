/*
  === ESP32 + INMP441 (Sender Node - Slave) ===
  📡 Gửi dữ liệu dB qua ESP-NOW đến Master (ESP8266)

  === Sơ đồ nối chân INMP441 – ESP32 ===
  | INMP441 | ESP32 |
  |----------|--------|
  | VCC      | 3.3V  |
  | GND      | GND   |
  | SD       | GPIO33 |
  | WS (LRCL)| GPIO25 |
  | SCK (BCLK)| GPIO26 |
  | L/R      | GND   |
*/

#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_now.h>
#include <WiFi.h>

#define I2S_WS  25
#define I2S_SD  33
#define I2S_SCK 26
#define SAMPLE_RATE 16000
#define SAMPLES 256

uint8_t masterAddress[] = {0xEC, 0xFA, 0xBC, 0x12, 0x34, 0x56}; // ⚠️ Thay bằng MAC của ESP8266 master

typedef struct {
  float dB;
  uint8_t micID;
} SoundData;

SoundData soundPacket;

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

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "✅ Gửi thành công" : "❌ Gửi thất bại");
}

void setup() {
  Serial.begin(115200);
  setupI2S();

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Lỗi khởi tạo ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  soundPacket.micID = 1;
}

void loop() {
  size_t bytes_read;
  int32_t buffer[SAMPLES];
  i2s_read(I2S_NUM_0, (void*)buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);

  double rms = calculateRMS(buffer, SAMPLES);
  double dB = 20 * log10(rms) + 94;
  soundPacket.dB = dB;

  esp_now_send(masterAddress, (uint8_t*)&soundPacket, sizeof(soundPacket));
  Serial.print("📊 Mic "); Serial.print(soundPacket.micID);
  Serial.print(": "); Serial.print(dB); Serial.println(" dB");
  delay(200);
}