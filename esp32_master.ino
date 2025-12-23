/*
 * ESP32 MASTER - FULL FEATURES
 * - RFID + ESP32-CAM + FINGERPRINT + FIREBASE QUEUE
 * - NEW: AUDIO (LM358/DAC) + BUTTON ENROLL (Nút BOOT)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "time.h"
#include <Adafruit_Fingerprint.h>
#include <Firebase_ESP_Client.h>

// --- THÊM: THƯ VIỆN ÂM THANH ---
#include <driver/dac.h>
#include "sound.h" // Bắt buộc phải có file này ở tab bên cạnh

// ================= 1) WIFI + FIREBASE =================
#define WIFI_SSID "UTC"
#define WIFI_PASS "00000000"
#define SPLIT_HOUR 13  // Mốc 13 giờ (1 giờ chiều). Trước giờ này là Vào, sau là Ra.
#define DATABASE_URL    "https://quanlychamcong-9dacd-default-rtdb.firebaseio.com/"
#define DATABASE_SECRET "Wu3ugJvvXLvIESnx4Po40PPLgACeYqeOmpEF1Bsl"

// Time VN (UTC+7)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200;
const int   daylightOffset_sec = 0;

// ================= 2) PINOUT =================
// --- TFT ST7735 ---
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

// --- RFID RC522 ---
#define RFID_SDA 21
#define RFID_RST 22
#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23

// --- UART tới ESP32-CAM ---
#define CAM_TX_PIN 32   // Master TX -> Cam RX
#define CAM_RX_PIN 33   // Master RX <- Cam TX
#define CAM_BAUD   9600

// --- Fingerprint UART2 ---
#define FP_RX   16      // RX2
#define FP_TX   17      // TX2
#define FP_BAUD 57600   

// --- THÊM: LOA VÀ NÚT BẤM ---
#define SPEAKER_PIN 25  // Nối ra LM358
#define BUTTON_PIN 0    // Nút BOOT để đăng ký vân tay

#define CAM_SUPPORTS_REQID 0
#define CAPTURE_ON_FINGER  0

// ================= 3) OBJECTS =================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
MFRC522 rfid(RFID_SDA, RFID_RST);

HardwareSerial FingerSerial(2);
Adafruit_Fingerprint finger(&FingerSerial);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbConfig;

// ================= 4) TIỆN ÍCH & ÂM THANH =================

// --- HÀM ÂM THANH (MỚI) ---
void speakThankYou() {
  dac_output_enable(DAC_CHANNEL_1); // GPIO 25
  int delayTime = 125; 
  for (int i = 0; i < audioLen; i++) {
    int sample = pgm_read_byte(&cam_on_ban[i]);
    dac_output_voltage(DAC_CHANNEL_1, sample);
    delayMicroseconds(delayTime);
  }
  dac_output_voltage(DAC_CHANNEL_1, 0);
}

void beep(int times) {
  dac_output_enable(DAC_CHANNEL_1);
  for(int k=0; k<times; k++){
    for(int i=0; i<200; i++) {
      dac_output_voltage(DAC_CHANNEL_1, 255); delayMicroseconds(250);
      dac_output_voltage(DAC_CHANNEL_1, 0);   delayMicroseconds(250);
    }
    delay(100);
  }
  dac_output_voltage(DAC_CHANNEL_1, 0);
}
// ----------------------------

String viToEn(String str) {
  String withMarks[] = { "á","à","ả","ã","ạ","ă","ắ","ằ","ẳ","ẵ","ặ","â","ấ","ầ","ẩ","ẫ","ậ","đ","é","è","ẻ","ẽ","ẹ","ê","ế","ề","ể","ễ","ệ","í","ì","ỉ","ĩ","ị","ó","ò","ỏ","õ","ọ","ô","ố","ồ","ổ","ỗ","ộ","ơ","ớ","ờ","ở","ỡ","ợ","ú","ù","ủ","ũ","ụ","ư","ứ","ừ","ử","ữ","ự","ý","ỳ","ỷ","ỹ","ỵ","Á","À","Ả","Ã","Ạ","Ă","Ắ","Ằ","Ẳ","Ẵ","Ặ","Â","Ấ","Ầ","Ẩ","Ẫ","Ậ","Đ","É","È","Ẻ","Ẽ","Ẹ","Ê","Ế","Ề","Ể","Ễ","Ệ","Í","Ì","Ỉ","Ĩ","Ị","Ó","Ò","Ỏ","Õ","Ọ","Ô","Ố","Ồ","Ổ","Ỗ","Ộ","Ơ","Ớ","Ờ","Ở","Ỡ","Ợ","Ú","Ù","Ủ","Ũ","Ụ","Ư","Ứ","Ừ","Ử","Ữ","Ự","Ý","Ỳ","Ỷ","Ỹ","Ỵ" };
  String withoutMarks[] = { "a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","d","e","e","e","e","e","e","e","e","e","e","e","i","i","i","i","i","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","u","u","u","u","u","u","u","u","u","u","u","y","y","y","y","y","A","A","A","A","A","A","A","A","A","A","A","A","A","A","A","A","A","D","E","E","E","E","E","E","E","E","E","E","E","I","I","I","I","I","O","O","O","O","O","O","O","O","O","O","O","O","O","O","O","O","O","U","U","U","U","U","U","U","U","U","U","U","Y","Y","Y","Y","Y" };
  for (int i = 0; i < (int)(sizeof(withMarks) / sizeof(withMarks[0])); i++) {
    str.replace(withMarks[i], withoutMarks[i]);
  }
  return str;
}

String deviceId() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[20];
  snprintf(buf, sizeof(buf), "%04X%08X", (uint16_t)(mac >> 32), (uint32_t)mac);
  return String(buf);
}

String nowTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return String(millis());
  char buf[24];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buf);
}

bool cooldown(unsigned long &lastMs, uint32_t gapMs) {
  unsigned long m = millis();
  if (m - lastMs < gapMs) return false;
  lastMs = m;
  return true;
}

// ================= 5) Firebase enqueue =================
String enqueueNewEnroll(const String &src, const String &value) {
  if (!Firebase.ready()) return "";
  if (!Firebase.RTDB.setString(&fbdo, "/new_enroll", value)) {
    Serial.println("set /new_enroll FAIL: " + fbdo.errorReason());
  }
  FirebaseJson j;
  j.set("src", src);
  j.set("value", value);
  j.set("status", "pending");
  j.set("ts", nowTimestamp());
  j.set("device", deviceId());
  if (!Firebase.RTDB.pushJSON(&fbdo, "/new_enroll_queue", &j)) return "";
  return fbdo.pushName();
}

// ================= 6) TFT UI =================
void showStandby() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW); tft.setTextSize(2);
  tft.setCursor(15, 20); tft.print("XIN CHAO");
  tft.setCursor(40, 45); tft.print("BAN!");

  char dateStr[20];
  sprintf(dateStr, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  tft.setTextColor(ST77XX_GREEN); tft.setTextSize(1);
  tft.setCursor(35, 80); tft.print(dateStr);

  char timeStr[20];
  sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2);
  tft.setCursor(15, 100); tft.print(timeStr);
}

// ================= 7) CAMERA COMMAND =================
void sendSnapFor(const String &tag, const String &reqOrUid) {
  // Hàm này hỗ trợ tương thích ngược hoặc gửi lệnh chụp đơn giản
  Serial1.println("SNAP:" + reqOrUid);
}

// ================= 8) LOGIC THÊM VÂN TAY (MỚI) =================
int getFingerprintID() {
  int p = -1;
  for (int i = 1; i < 127; i++) {
    p = finger.loadModel(i);
    if (p != FINGERPRINT_OK) return i;
  }
  return -1;
}

void enrollNewFinger() {
  int id = getFingerprintID();
  if (id == -1) { beep(3); return; }

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN); tft.setTextSize(2);
  tft.setCursor(10, 10); tft.print("THEM VAN TAY");
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1);
  tft.setCursor(10, 40); tft.print("Dat tay lan 1");
  tft.setCursor(10, 60); tft.print("ID: " + String(id));
  
  // 1. Quét lần 1
  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (digitalRead(BUTTON_PIN) == LOW) return; // Bấm lần nữa để hủy
  }
  finger.image2Tz(1);
  
  tft.setTextColor(ST77XX_GREEN); tft.setCursor(10, 80); tft.print("Lay tay ra...");
  beep(1); delay(1000);
  p = 0; while (p != FINGERPRINT_NOFINGER) { p = finger.getImage(); }

  // 2. Quét lần 2
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN); tft.setTextSize(2);
  tft.setCursor(10, 10); tft.print("XAC NHAN");
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1);
  tft.setCursor(10, 40); tft.print("Dat lai lan 2...");
  
  p = -1; while (p != FINGERPRINT_OK) { p = finger.getImage(); }
  finger.image2Tz(2);

  // 3. Lưu
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    p = finger.storeModel(id);
    if (p == FINGERPRINT_OK) {
      tft.fillScreen(ST77XX_GREEN);
      tft.setTextColor(ST77XX_BLACK); tft.setTextSize(2);
      tft.setCursor(10, 40); tft.print("THANH CONG!");
      
      speakThankYou(); // Nói "Cảm ơn"
      
      // Chụp ảnh đăng ký
      struct tm timeinfo; getLocalTime(&timeinfo);
      char uniqueCode[30]; strftime(uniqueCode, 30, "%Y%m%d%H%M%S", &timeinfo);
      String imgName = "FINGER_" + String(id) + "_" + String(uniqueCode);
      Serial1.println("SNAP:" + imgName);

      // Gửi vào hàng đợi Enroll
      enqueueNewEnroll("FINGER", "FINGER_" + String(id));
      
    } else {
      tft.fillScreen(ST77XX_RED); tft.print("Loi Flash"); beep(3);
    }
  } else {
    tft.fillScreen(ST77XX_RED); tft.print("Khong khop!"); beep(3);
  }
  delay(2000);
}

// ================= 9) RFID HANDLE (Logic Chấm Công) =================
// ================= XỬ LÝ CHẤM CÔNG (LOGIC: LẦN 1 VÀO - LẦN 2 RA) =================
void handleCard(const String &uid) {
  Serial.println("Card Detected: " + uid);

  // 1. Lấy thời gian để tạo Key theo ngày
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) Serial.println("Loi lay gio!");
  
  // Tạo chuỗi ngày hôm nay (Ví dụ: "20231225")
  char dayStr[10];
  strftime(dayStr, 10, "%Y%m%d", &timeinfo);
  
  // Tạo chuỗi thời gian cho ảnh
  char uniqueCode[30];
  strftime(uniqueCode, 30, "%Y%m%d%H%M%S", &timeinfo);
  String imgFileName = uid + "_" + String(uniqueCode); 

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 50); tft.setTextColor(ST77XX_YELLOW); tft.setTextSize(1);
  tft.print("Dang kiem tra...");

  // Gửi lệnh chụp ảnh ngay cho đỡ trễ
  Serial1.println("SNAP:" + imgFileName);

  // 2. Tìm tên & Kiểm tra trạng thái hôm nay trên Firebase
  String name = "";
  String currentStatus = ""; // Trạng thái hiện tại
  String statusPath = "/daily_check/" + String(dayStr) + "/" + uid; // Đường dẫn lưu trạng thái ngày

  if (Firebase.ready()) {
    // Lấy tên
    if (Firebase.RTDB.getString(&fbdo, "/users/" + uid + "/name")) name = fbdo.stringData();
    
    // Lấy trạng thái hôm nay (Xem lần cuối là IN hay OUT)
    if (Firebase.RTDB.getString(&fbdo, statusPath)) {
      currentStatus = fbdo.stringData();
    }
  }

  // 3. Quyết định VÀO hay RA
  // Nếu chưa có dữ liệu hoặc lần trước là OUT -> Giờ sẽ là IN
  // Nếu lần trước là IN -> Giờ sẽ là OUT
  String newStatus = "IN"; 
  if (currentStatus == "IN") {
    newStatus = "OUT";
  }

  // Chuẩn bị JSON log
  FirebaseJson json;
  json.set("id", uid);
  json.set("timestamp", nowTimestamp());
  json.set("type", "RFID");
  json.set("device", deviceId());
  json.set("image", imgFileName + ".jpg");
  json.set("status", newStatus); // Gửi trạng thái mới lên

  if (name.length() > 0) {
    // >> NGƯỜI QUEN
    tft.fillScreen(ST77XX_GREEN);
    tft.setTextColor(ST77XX_BLACK); 
    
    // Hiện tên
    tft.setTextSize(2); tft.setCursor(10, 20); tft.print(viToEn(name));
    
    // Hiện trạng thái VÀO / RA
    tft.setTextSize(1); tft.setCursor(10, 60);
    
    if (newStatus == "IN") {
      tft.print("XIN CHAO!");      
      tft.setCursor(10, 80); 
      tft.setTextSize(2); tft.print("VAO LAM");
    } else {
      tft.print("TAM BIET!");      
      tft.setCursor(10, 80); 
      tft.setTextSize(2); tft.print("RA VE");
    }
    
    speakThankYou(); 
    
    // A. Gửi log chấm công
    json.set("name", name);
    if (Firebase.ready()) Firebase.RTDB.pushJSON(&fbdo, "/attendance", &json);
    
    // B. Cập nhật trạng thái mới vào ngày hôm nay để nhớ cho lần sau
    if (Firebase.ready()) Firebase.RTDB.setString(&fbdo, statusPath, newStatus);
    
    delay(2500);
  } 
  else {
    // >> NGƯỜI LẠ
    tft.fillScreen(ST77XX_RED);
    tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2);
    tft.setCursor(20, 40); tft.print("THE LA");
    tft.setTextSize(1); tft.setCursor(5, 80); tft.print("ID: " + uid);
    
    beep(2); 

    json.set("name", "Nguoi La (" + uid + ")");
    if (Firebase.ready()) Firebase.RTDB.pushJSON(&fbdo, "/attendance", &json);

    static unsigned long lastCardEnrollMs = 0;
    if (cooldown(lastCardEnrollMs, 1200)) enqueueNewEnroll("CARD", uid);
    
    delay(2000);
  }
}
// ================= 10) FINGERPRINT CHECK (Logic Chấm Công) =================
void checkFingerprint() {
  // throttle
  static unsigned long lastPoll = 0;
  if (!cooldown(lastPoll, 150)) return;

  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) { beep(2); return; } // Lỗi đọc

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    // --> TÌM THẤY VÂN TAY (QUEN)
    String val = "FINGER_" + String(finger.fingerID);
    // Gọi lại hàm handleCard để tái sử dụng logic (chụp ảnh, lưu data, nói)
    handleCard(val); 
  } else {
    // --> VÂN TAY LẠ (CHƯA ĐK)
    tft.fillScreen(ST77XX_RED);
    tft.setCursor(10, 50); tft.print("CHUA DANG KY!");
    beep(3);
    
    // Vẫn chụp ảnh người lạ
    struct tm timeinfo; getLocalTime(&timeinfo);
    char uniqueCode[30]; strftime(uniqueCode, 30, "%Y%m%d%H%M%S", &timeinfo);
    String imgName = "UNKNOWN_" + String(uniqueCode);
    Serial1.println("SNAP:" + imgName);
    
    delay(1500);
    showStandby();
  }
}

// ================= 11) SETUP / LOOP =================
void setup() {
  Serial.begin(115200);
  Serial1.begin(CAM_BAUD, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);

  // Cấu hình Nút bấm
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // SPI init
  pinMode(TFT_CS, OUTPUT);   digitalWrite(TFT_CS, HIGH);
  pinMode(RFID_SDA, OUTPUT); digitalWrite(RFID_SDA, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // TFT init
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1);
  tft.setCursor(10, 10); tft.print("Khoi dong...");

  // RFID init
  rfid.PCD_Init();
  
  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }

  // NTP time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Firebase (đã thêm cấu hình tăng bộ đệm để tránh lỗi SSL)
  fbConfig.database_url = DATABASE_URL;
  fbConfig.signer.tokens.legacy_token = DATABASE_SECRET;
  fbConfig.timeout.wifiReconnect = 10000;
  fbdo.setResponseSize(2048); 
  
  Firebase.begin(&fbConfig, &auth);
  Firebase.reconnectWiFi(true);

  // Fingerprint init
  FingerSerial.begin(FP_BAUD, SERIAL_8N1, FP_RX, FP_TX);
  finger.begin(FP_BAUD);
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint: OK");
  } else {
    tft.setCursor(0, 20); tft.print("Loi Van Tay!");
  }

  showStandby();
}

void loop() {
  // 1. KIỂM TRA NÚT BẤM (Để thêm vân tay)
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50); // chống rung
    if (digitalRead(BUTTON_PIN) == LOW) {
       enrollNewFinger(); // Vào chế độ đăng ký
       showStandby();     // Xong thì quay về
       while(digitalRead(BUTTON_PIN) == LOW); // Chờ thả nút
    }
  }

  // 2. Fingerprint Check (Chấm công)
  checkFingerprint();

  // 3. Standby refresh
  static unsigned long lastTime = 0;
  if (millis() - lastTime > 1000) {
    showStandby();
    lastTime = millis();
  }

  // 4. RFID debounce & Check (Chấm công)
  static String lastUID = "";
  static unsigned long lastUIDms = 0;

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    if (uid == lastUID && (millis() - lastUIDms) < 1800) return;
    lastUID = uid;
    lastUIDms = millis();

    handleCard(uid);
    showStandby();
  }
}
