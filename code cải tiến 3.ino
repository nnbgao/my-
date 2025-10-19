/* === ESP32 + INMP441 Sound Level Monitor (v2.1) === 📌 Cập nhật:

Lọc DC offset để tránh trôi khi im lặng
Tính dB từ tín hiệu gốc, FFT chỉ dùng để quan sát phổ tần
Hiển thị mượt hơn trên Serial Plotter
=== Sơ đồ kết nối ===

INMP441	ESP32
VCC	3.3V
GND	GND
SD	GPIO33
WS (LRCL)	GPIO25
SCK (BCLK)	GPIO26
L/R	GND
*/	
#include <Arduino.h> #include <driver/i2s.h> #include <arduinoFFT.h>

#define I2S_WS 25 #define I2S_SD 33 #define I2S_SCK 26

#define SAMPLE_RATE 16000 #define SAMPLES 256

double vReal[SAMPLES]; double vImag[SAMPLES]; ArduinoFFT FFT;

void setupI2S() { i2s_config_t i2s_config = { .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), .sample_rate = SAMPLE_RATE, .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, .communication_format = I2S_COMM_FORMAT_I2S, .intr_alloc_flags = 0, .dma_buf_count = 8, .dma_buf_len = 64, .use_apll = false };

i2s_pin_config_t pin_config = { .bck_io_num = I2S_SCK, .ws_io_num = I2S_WS, .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = I2S_SD };

i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL); i2s_set_pin(I2S_NUM_0, &pin_config); i2s_zero_dma_buffer(I2S_NUM_0); }

// Lọc offset và tính RMS double calculateRMS(int32_t *data, int samples) { double mean = 0; for (int i = 0; i < samples; i++) mean += data[i]; mean /= samples;

double sum = 0; for (int i = 0; i < samples; i++) { double x = (data[i] - mean) / 2147483648.0; sum += x * x; } return sqrt(sum / samples); }

void setup() { Serial.begin(115200); delay(1000); setupI2S();

Serial.println("=== ESP32 + INMP441: Real-time FFT & dB (v2.1) ==="); Serial.println("Amplitude\tDecibel(dB)"); }

void loop() { size_t bytes_read; int32_t buffer[SAMPLES];

i2s_read(I2S_NUM_0, (void*)buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);

// RMS từ dữ liệu gốc double rms = calculateRMS(buffer, SAMPLES); double dB = 20 * log10(rms)+94; // 20 µPa là ngưỡng nghe được

// FFT để xem phổ tần for (int i = 0; i < SAMPLES; i++) { vReal[i] = (double)buffer[i] / 2147483648.0; vImag[i] = 0; }

static unsigned long lastFFT = 0; if (millis() - lastFFT > 100) { FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD); FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD); FFT.complexToMagnitude(vReal, vImag, SAMPLES); lastFFT = millis(); }

// Xuất ra Plotter //Serial.print(vReal[4] * 1000); // biên độ tần số thấp (4~8Hz) Serial.print("\t"); Serial.println(dB);

delay(50); }