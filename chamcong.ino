/*
 * ESP32-CAM SLAVE
 * Nhận "SNAP:<UID>" từ Master -> flash nháy + giữ sáng -> chụp ảnh -> upload Firebase Storage
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <Arduino.h>

// Firebase (Mobizt)
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ================= 1) WIFI + FIREBASE =================
const char *WIFI_SSID = "UTC";
const char *WIFI_PASS = "00000000";

#define API_KEY "AIzaSyBKIkzONHTQZZcmQIkzG9avMM8kwG8Yzck"
#define USER_EMAIL "admin@test.com"
#define USER_PASSWORD "123456"
#define STORAGE_BUCKET_ID "quanlychamcong-9dacd.firebasestorage.app"

// ================= 2) PINOUT AI THINKER =================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FLASH_GPIO_NUM     4

// ================= 3) FIREBASE OBJECTS =================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbConfig;

// ================= 4) FLASH PWM (COMPAT CORE 2.x / 3.x) =================
static const int FLASH_FREQ = 5000;   // Hz
static const int FLASH_RES  = 8;      // 0..255

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
// Core 3.x: dùng ledcAttach(pin, freq, resolution) + ledcWrite(pin, duty)
void flashInit() {
  ledcAttach(FLASH_GPIO_NUM, FLASH_FREQ, FLASH_RES);
  ledcWrite(FLASH_GPIO_NUM, 0);
}
void flashWrite(uint8_t duty) { // 0..255
  ledcWrite(FLASH_GPIO_NUM, duty);
}
#else
// Core 2.x: dùng ledcSetup(channel...) + ledcAttachPin(pin, channel) + ledcWrite(channel, duty)
static const int FLASH_CH = 7;
void flashInit() {
  ledcSetup(FLASH_CH, FLASH_FREQ, FLASH_RES);
  ledcAttachPin(FLASH_GPIO_NUM, FLASH_CH);
  ledcWrite(FLASH_CH, 0);
}
void flashWrite(uint8_t duty) {
  ledcWrite(FLASH_CH, duty);
}
#endif

// ================= 5) CAMERA INIT =================
bool initCamera() {
  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;

  c.pin_d0 = Y2_GPIO_NUM;
  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;
  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;
  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;
  c.pin_d7 = Y9_GPIO_NUM;

  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href  = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;

  c.pin_pwdn  = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;

  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;

  c.frame_size   = FRAMESIZE_VGA;
  c.jpeg_quality = 12;
  c.fb_count     = 1;

  return (esp_camera_init(&c) == ESP_OK);
}

// ================= 6) CHỤP + UPLOAD =================
void captureAndUpload(const String &id) {
  Serial.println("CAM: SNAP for " + id);

  // Nháy + giữ sáng để khỏi tối
  const uint8_t FLASH_DUTY = 220;   // giảm nếu cháy sáng

  // Strobe 2 lần để camera kịp auto-exposure
  for (int i = 0; i < 2; i++) {
    flashWrite(FLASH_DUTY);
    delay(60);
    flashWrite(0);
    delay(60);
  }

  // Giữ sáng ổn định
  flashWrite(FLASH_DUTY);
  delay(350);

  // Mồi 1 frame (ổn định AE/AWB)
  camera_fb_t *warm = esp_camera_fb_get();
  if (warm) esp_camera_fb_return(warm);

  // Chụp thật
  camera_fb_t *fb = esp_camera_fb_get();

  delay(80);
  flashWrite(0);

  if (!fb) {
    Serial.println("CAM: ERR no frame");
    return;
  }

  if (!Firebase.ready()) {
    Serial.println("CAM: ERR Firebase not ready");
    esp_camera_fb_return(fb);
    return;
  }

  String path = "/photos/" + id + "_" + String(millis()) + ".jpg";
  Serial.println("CAM: Upload -> " + path);

  bool ok = Firebase.Storage.upload(
    &fbdo,
    STORAGE_BUCKET_ID,
    fb->buf,
    fb->len,
    path.c_str(),
    "image/jpeg",
    nullptr
  );

  if (ok) Serial.println("CAM: OK " + path);
  else    Serial.println("CAM: FAIL " + fbdo.errorReason());

  esp_camera_fb_return(fb);
}

// ================= 7) SETUP / LOOP =================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(9600);

  flashInit();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); }
  Serial.println("CAM: WiFi OK");

  if (!initCamera()) {
    Serial.println("CAM: Camera Init Failed");
    while (true) delay(1000);
  }

  fbConfig.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // TokenHelper.h đã có sẵn tokenStatusCallback
  fbConfig.token_status_callback = tokenStatusCallback;

  Firebase.begin(&fbConfig, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("CAM: READY");
}

void loop() {
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    if (data.startsWith("SNAP:")) {
      String id = data.substring(5);
      id.trim();
      captureAndUpload(id);
    }
  }
}
