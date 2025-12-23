/*
 * DỰ ÁN: CAMERA GIÁM SÁT THÔNG MINH (SLAVE - ESP32-CAM)
 * -----------------------------------------------------
 * Chức năng chính:
 * 1. Kết nối WiFi và Firebase Storage.
 * 2. Lắng nghe lệnh từ ESP32 Master qua cổng Serial (RX/TX).
 * 3. Khi nhận lệnh "SNAP:<ID>":
 * - Bật đèn Flash (chế độ chống lóa/auto-exposure).
 * - Chụp ảnh JPEG.
 * - Upload ảnh trực tiếp lên Firebase Storage.
 */

#include "esp_camera.h"       // Thư viện điều khiển Camera
#include <WiFi.h>
#include <Arduino.h>

// Thư viện Firebase (Mobizt) để upload file
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// Thư viện xử lý phần cứng cấp thấp (để tắt Brown-out detector)
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ================= 1. CẤU HÌNH WIFI & FIREBASE =================
const char *WIFI_SSID = "UTC";
const char *WIFI_PASS = "00000000";

// Thông tin Project Firebase
#define API_KEY "AIzaSyBKIkzONHTQZZcmQIkzG9avMM8kwG8Yzck"
#define USER_EMAIL "admin@test.com"
#define USER_PASSWORD "123456"
// Bucket ID: Nơi lưu trữ ảnh (lấy trong tab Storage của Firebase Console)
#define STORAGE_BUCKET_ID "quanlychamcong-9dacd.firebasestorage.app"

// ================= 2. ĐỊNH NGHĨA CHÂN (PINOUT AI THINKER) =================
// Đây là sơ đồ chân chuẩn của board ESP32-CAM AI Thinker
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

#define FLASH_GPIO_NUM     4  // Chân đèn Flash tích hợp sẵn trên mạch

// ================= 3. ĐỐI TƯỢNG FIREBASE =================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbConfig;

// ================= 4. ĐIỀU KHIỂN ĐÈN FLASH (PWM) =================
/*
 * Lưu ý: ESP32 Core 3.x đã thay đổi cách dùng LEDC (PWM).
 * Đoạn code dưới đây tự động nhận diện version để biên dịch đúng hàm.
 */
static const int FLASH_FREQ = 5000;   // Tần số PWM 5kHz
static const int FLASH_RES  = 8;      // Độ phân giải 8-bit (0-255)

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
// --- Dành cho ESP32 Core 3.0 trở lên ---
void flashInit() {
  ledcAttach(FLASH_GPIO_NUM, FLASH_FREQ, FLASH_RES);
  ledcWrite(FLASH_GPIO_NUM, 0); // Tắt đèn ban đầu
}
void flashWrite(uint8_t duty) {
  ledcWrite(FLASH_GPIO_NUM, duty); // Điều chỉnh độ sáng (0-255)
}
#else
// --- Dành cho ESP32 Core 2.x (Cũ hơn) ---
static const int FLASH_CH = 7; // Kênh PWM số 7
void flashInit() {
  ledcSetup(FLASH_CH, FLASH_FREQ, FLASH_RES);
  ledcAttachPin(FLASH_GPIO_NUM, FLASH_CH);
  ledcWrite(FLASH_CH, 0);
}
void flashWrite(uint8_t duty) {
  ledcWrite(FLASH_CH, duty);
}
#endif

// ================= 5. KHỞI TẠO CAMERA =================
bool initCamera() {
  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;

  // Gán các chân dữ liệu
  c.pin_d0 = Y2_GPIO_NUM;
  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;
  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;
  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;
  c.pin_d7 = Y9_GPIO_NUM;

  // Gán chân điều khiển
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href  = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;

  c.pin_pwdn  = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;

  c.xclk_freq_hz = 20000000;      // XCLK 20MHz
  c.pixel_format = PIXFORMAT_JPEG; // Định dạng ảnh JPEG để nhẹ khi upload

  // Cấu hình chất lượng ảnh
  c.frame_size    = FRAMESIZE_VGA; // Độ phân giải VGA (640x480) - Cân bằng giữa nét và tốc độ
  c.jpeg_quality = 12;             // Chất lượng (0-63), càng thấp càng nét (10-12 là đẹp)
  c.fb_count      = 1;             // Số lượng frame buffer

  return (esp_camera_init(&c) == ESP_OK);
}

// ================= 6. QUY TRÌNH CHỤP VÀ UPLOAD =================
void captureAndUpload(const String &id) {
  Serial.println("CAM: SNAP for " + id);

  // Độ sáng đèn Flash (tối đa 255, giảm xuống 220 để tránh cháy sáng)
  const uint8_t FLASH_DUTY = 220;

  // --- KỸ THUẬT "NHÁY MỒI" (Pre-flash) ---
  // Mục đích: Giúp cảm biến camera đo sáng tự động (Auto Exposure) trước khi chụp thật
  for (int i = 0; i < 2; i++) {
    flashWrite(FLASH_DUTY);
    delay(60);
    flashWrite(0);
    delay(60);
  }

  // Bật đèn sáng ổn định để chụp
  flashWrite(FLASH_DUTY);
  delay(350); // Chờ 350ms để cân bằng trắng (AWB) ổn định

  // --- KỸ THUẬT "BỎ FRAME ĐẦU" ---
  // Lấy frame đầu tiên và bỏ đi vì nó thường bị ám màu hoặc chưa chỉnh sáng kịp
  camera_fb_t *warm = esp_camera_fb_get();
  if (warm) esp_camera_fb_return(warm); // Trả vùng nhớ frame này ngay

  // --- CHỤP ẢNH THẬT ---
  camera_fb_t *fb = esp_camera_fb_get();

  // Tắt đèn ngay sau khi chụp xong để đỡ tốn điện/nóng
  delay(80);
  flashWrite(0);

  // Kiểm tra lỗi chụp
  if (!fb) {
    Serial.println("CAM: ERR no frame");
    return;
  }

  // Kiểm tra kết nối Firebase
  if (!Firebase.ready()) {
    Serial.println("CAM: ERR Firebase not ready");
    esp_camera_fb_return(fb); // Nhớ giải phóng bộ nhớ ảnh nếu lỗi
    return;
  }

  // Tạo đường dẫn lưu file: /photos/ID_Timestamp.jpg
  // Timestamp lấy millis() vì ESP32-CAM này không cần đồng bộ giờ chính xác tuyệt đối (chỉ cần tên file khác nhau)
  String path = "/photos/" + id + "_" + String(millis()) + ".jpg";
  Serial.println("CAM: Upload -> " + path);

  // --- GỬI LÊN FIREBASE STORAGE ---
  bool ok = Firebase.Storage.upload(
    &fbdo,
    STORAGE_BUCKET_ID,
    fb->buf,       // Dữ liệu ảnh
    fb->len,       // Kích thước ảnh
    path.c_str(),  // Đường dẫn trên Cloud
    "image/jpeg",  // MIME type
    nullptr        // Không cần callback tiến trình
  );

  if (ok) Serial.println("CAM: OK " + path);
  else    Serial.println("CAM: FAIL " + fbdo.errorReason());

  // Quan trọng: Giải phóng bộ nhớ đệm frame ảnh sau khi dùng xong
  esp_camera_fb_return(fb);
}

// ================= 7. SETUP (KHỞI TẠO HỆ THỐNG) =================
void setup() {
  // Tắt Brown-out detector: Rất quan trọng với ESP32-CAM!
  // Nếu không tắt, khi bật đèn Flash hoặc Wifi sóng yếu, chip sẽ sụt áp và tự reset.
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(9600); // Tốc độ Serial phải khớp với mạch Master

  flashInit(); // Khởi tạo chân đèn Flash

  // Kết nối WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); }
  Serial.println("CAM: WiFi OK");

  // Khởi tạo Camera
  if (!initCamera()) {
    Serial.println("CAM: Camera Init Failed");
    while (true) delay(1000); // Treo máy nếu lỗi cam
  }

  // Cấu hình Firebase
  fbConfig.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Callback xử lý token xác thực
  fbConfig.token_status_callback = tokenStatusCallback;

  Firebase.begin(&fbConfig, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("CAM: READY"); // Báo hiệu đã sẵn sàng nhận lệnh
}

// ================= 8. LOOP (VÒNG LẶP CHÍNH) =================
void loop() {
  // Kiểm tra xem có dữ liệu gửi đến từ Master qua Serial không
  if (Serial.available()) {
    // Đọc chuỗi đến khi gặp ký tự xuống dòng
    String data = Serial.readStringUntil('\n');
    data.trim(); // Loại bỏ khoảng trắng thừa đầu/cuối

    // Kiểm tra định dạng lệnh: "SNAP:USER_ID"
    if (data.startsWith("SNAP:")) {
      String id = data.substring(5); // Cắt bỏ chữ "SNAP:" để lấy ID
      id.trim();
      
      // Gọi hàm chụp và upload
      captureAndUpload(id);
    }
  }
}
