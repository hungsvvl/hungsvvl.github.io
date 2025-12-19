/*
 * ESP32 MASTER - RFID + TFT + ESP32-CAM + FINGERPRINT + FIREBASE RTDB (QUEUE)
 *
 * - RFID RC522: quét thẻ
 * - ESP32-CAM: chụp ảnh qua UART1 (Serial1)
 * - Fingerprint: UART2 (RX2/TX2)
 * - Firebase:
 *    + /users/<UID>/name  (thẻ hợp lệ)
 *    + /attendance pushJSON (thẻ hợp lệ)
 *    + /new_enroll/queue pushJSON (thẻ lạ hoặc vân tay)
 *    + /new_enroll/latest setJSON  (request mới nhất)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "time.h"

#include <Adafruit_Fingerprint.h>

// Firebase (Mobizt)
#include <Firebase_ESP_Client.h>

// ================= 1) WIFI + FIREBASE =================
#define WIFI_SSID "UTC"
#define WIFI_PASS "00000000"

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

// SPI default (ESP32)
#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23

// --- UART tới ESP32-CAM ---
#define CAM_TX_PIN 32   // Master TX -> Cam RX (U0R)
#define CAM_RX_PIN 33   // Master RX <- Cam TX (U0T)
#define CAM_BAUD   9600

// --- Fingerprint UART2 ---
#define FP_RX   16      // RX2
#define FP_TX   17      // TX2
#define FP_BAUD 57600   // Nếu không nhận, thử 9600

// Nếu bạn muốn Master gửi lệnh dạng "SNAP:ENROLL:<reqId>" (để ảnh khớp req)
// thì ESP32-CAM cũng phải được sửa để hiểu format này.
// 0 = dùng format cũ SNAP:<UID>
#define CAM_SUPPORTS_REQID 0

// Nếu muốn vân tay quét cũng chụp ảnh thì bật 1
#define CAPTURE_ON_FINGER  0

// ================= 3) OBJECTS =================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
MFRC522 rfid(RFID_SDA, RFID_RST);

HardwareSerial FingerSerial(2);
Adafruit_Fingerprint finger(&FingerSerial);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbConfig;

// ================= 4) TIỆN ÍCH =================
String viToEn(String str) {
  String withMarks[] = { "á","à","ả","ã","ạ","ă","ắ","ằ","ẳ","ẵ","ặ","â","ấ","ầ","ẩ","ẫ","ậ","đ",
                         "é","è","ẻ","ẽ","ẹ","ê","ế","ề","ể","ễ","ệ",
                         "í","ì","ỉ","ĩ","ị",
                         "ó","ò","ỏ","õ","ọ","ô","ố","ồ","ổ","ỗ","ộ","ơ","ớ","ờ","ở","ỡ","ợ",
                         "ú","ù","ủ","ũ","ụ","ư","ứ","ừ","ử","ữ","ự",
                         "ý","ỳ","ỷ","ỹ","ỵ",
                         "Á","À","Ả","Ã","Ạ","Ă","Ắ","Ằ","Ẳ","Ẵ","Ặ","Â","Ấ","Ầ","Ẩ","Ẫ","Ậ","Đ",
                         "É","È","Ẻ","Ẽ","Ẹ","Ê","Ế","Ề","Ể","Ễ","Ệ",
                         "Í","Ì","Ỉ","Ĩ","Ị",
                         "Ó","Ò","Ỏ","Õ","Ọ","Ô","Ố","Ồ","Ổ","Ỗ","Ộ","Ơ","Ớ","Ờ","Ở","Ỡ","Ợ",
                         "Ú","Ù","Ủ","Ũ","Ụ","Ư","Ứ","Ừ","Ử","Ữ","Ự",
                         "Ý","Ỳ","Ỷ","Ỹ","Ỵ" };
  String withoutMarks[] = { "a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","d",
                            "e","e","e","e","e","e","e","e","e","e","e",
                            "i","i","i","i","i",
                            "o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o",
                            "u","u","u","u","u","u","u","u","u","u","u",
                            "y","y","y","y","y",
                            "A","A","A","A","A","A","A","A","A","A","A","A","A","A","A","A","A","D",
                            "E","E","E","E","E","E","E","E","E","E","E",
                            "I","I","I","I","I",
                            "O","O","O","O","O","O","O","O","O","O","O","O","O","O","O","O","O",
                            "U","U","U","U","U","U","U","U","U","U","U",
                            "Y","Y","Y","Y","Y" };
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

// ================= 5) Firebase enqueue (KHÔNG GHI ĐÈ) =================
String enqueueNewEnroll(const String &src, const String &value) {
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready -> skip enqueue");
    return "";
  }

  // 1) ✅ GỬI TÍN HIỆU CHO WEB (JS của bạn đang nghe /new_enroll)
  //    Chỉ gửi ID "sạch" (UID hoặc số vân tay), không gửi object
  if (!Firebase.RTDB.setString(&fbdo, "/new_enroll", value)) {
    Serial.println("set /new_enroll FAIL: " + fbdo.errorReason());
  } else {
    Serial.println("/new_enroll = " + value);
  }

  // 2) (TUỲ CHỌN) nếu bạn vẫn muốn lưu lịch sử không ghi đè thì lưu sang node KHÁC
  FirebaseJson j;
  j.set("src", src);
  j.set("value", value);
  j.set("status", "pending");
  j.set("ts", nowTimestamp());
  j.set("device", deviceId());

  // ⚠️ QUAN TRỌNG: KHÔNG dùng /new_enroll/queue nữa
  if (!Firebase.RTDB.pushJSON(&fbdo, "/new_enroll_queue", &j)) {
    Serial.println("push /new_enroll_queue FAIL: " + fbdo.errorReason());
    return "";
  }

  return fbdo.pushName(); // key lịch sử
}

// ================= 6) TFT UI =================
void showStandby() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(15, 20); tft.print("XIN CHAO");
  tft.setCursor(40, 45); tft.print("BAN!");

  char dateStr[20];
  sprintf(dateStr, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(1);
  tft.setCursor(35, 80);
  tft.print(dateStr);

  char timeStr[20];
  sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(15, 100);
  tft.print(timeStr);
}

// ================= 7) CAMERA COMMAND =================
void sendSnapFor(const String &tag, const String &reqOrUid) {
#if CAM_SUPPORTS_REQID
  // Format mới: SNAP:ENROLL:<reqId> hoặc SNAP:ATT:<reqId> ...
  Serial1.println("SNAP:" + tag + ":" + reqOrUid);
#else
  // Format cũ: SNAP:<UID>
  (void)tag;
  Serial1.println("SNAP:" + reqOrUid);
#endif
}

// ================= 8) RFID HANDLE =================
void handleCard(const String &uid) {
  Serial.println("Card Detected: " + uid);

  // Hiển thị đang xử lý
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 50);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(1);
  tft.print("Dang xu ly the...");
  tft.setCursor(10, 65);
  tft.print("Dang chup anh...");

  // Luôn chụp ảnh lưu lại (kể cả thẻ lạ)
  sendSnapFor("CARD", uid);
  Serial.println("-> Sent SNAP to CAM for CARD");

  // Tìm tên trong /users/<uid>/name
  String name = "";
  String path = "/users/" + uid + "/name";

  if (Firebase.ready()) {
    if (Firebase.RTDB.getString(&fbdo, path) && fbdo.dataType() == "string") {
      name = fbdo.stringData();
    }
  }

  if (name.length() > 0) {
    // THẺ HỢP LỆ: chấm công + push attendance
    tft.fillScreen(ST77XX_GREEN);
    tft.setTextColor(ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 30);
    tft.print(viToEn(name));
    tft.setTextSize(1);
    tft.setCursor(10, 70); tft.print("CHAM CONG OK!");
    tft.setCursor(10, 90); tft.print("Anh dang luu...");

    FirebaseJson json;
    json.set("id", uid);
    json.set("name", name);
    json.set("timestamp", nowTimestamp());
    json.set("type", "RFID");
    json.set("device", deviceId());

    if (Firebase.ready()) {
      if (Firebase.RTDB.pushJSON(&fbdo, "/attendance", &json)) {
        Serial.println("-> Attendance pushed");
      } else {
        Serial.println("-> Attendance push FAIL: " + fbdo.errorReason());
      }
    }
    delay(2500);
    return;
  }

  // THẺ LẠ: enqueue /new_enroll/queue (không ghi đè)
  tft.fillScreen(ST77XX_RED);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 40); tft.print("THE LA");
  tft.setTextSize(1);
  tft.setCursor(5, 80); tft.print("ID: " + uid);

  static unsigned long lastCardEnrollMs = 0;
  if (cooldown(lastCardEnrollMs, 1200)) {
    String reqId = enqueueNewEnroll("CARD", uid);

    // Nếu bạn muốn ảnh khớp reqId (khi CAM_SUPPORTS_REQID=1)
#if CAM_SUPPORTS_REQID
    if (reqId.length()) sendSnapFor("ENROLL", reqId);
#endif
  }

  delay(2000);
}

// ================= 9) FINGERPRINT CHECK =================
void checkFingerprint() {
  // throttle đọc cảm biến (đỡ chiếm CPU)
  static unsigned long lastPoll = 0;
  if (!cooldown(lastPoll, 150)) return;

  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return; // không có tay hoặc lỗi nhẹ

  static unsigned long lastFingerEventMs = 0;
  if (!cooldown(lastFingerEventMs, 1500)) return;

  Serial.println("Finger detected");

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    enqueueNewEnroll("FINGER", "READ_FAIL");
#if CAPTURE_ON_FINGER
    sendSnapFor("FINGER", "READ_FAIL");
#endif
    return;
  }

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    String val = String(finger.fingerID);      // đã có ID trong cảm biến
    String reqId = enqueueNewEnroll("FINGER", val);
#if CAPTURE_ON_FINGER
    sendSnapFor("FINGER", val);
#endif
#if CAM_SUPPORTS_REQID
    if (reqId.length()) sendSnapFor("ENROLL", reqId);
#endif
  } else {
    // không khớp: coi như vân tay mới cần app enroll
    String reqId = enqueueNewEnroll("FINGER", "NEW");
#if CAPTURE_ON_FINGER
    sendSnapFor("FINGER", "NEW");
#endif
#if CAM_SUPPORTS_REQID
    if (reqId.length()) sendSnapFor("ENROLL", reqId);
#endif
  }

  // UI báo nhanh
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(10, 35); tft.print("FINGER");
  tft.setTextSize(1);
  tft.setCursor(10, 70); tft.print("Da gui yeu cau");
  delay(800);
}

// ================= 10) SETUP / LOOP =================
void setup() {
  Serial.begin(115200);
  Serial.println("\nMASTER BOOT");

  // UART to CAM
  Serial1.begin(CAM_BAUD, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);

  // SPI init
  pinMode(TFT_CS, OUTPUT);   digitalWrite(TFT_CS, HIGH);
  pinMode(RFID_SDA, OUTPUT); digitalWrite(RFID_SDA, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // TFT init
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print("Khoi dong...");

  // RFID init
  rfid.PCD_Init();
  Serial.println("RFID: OK");

  // WiFi
  tft.setCursor(10, 30);
  tft.print("Ket noi WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());

  // NTP time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Firebase RTDB legacy token
  fbConfig.database_url = DATABASE_URL;
  fbConfig.signer.tokens.legacy_token = DATABASE_SECRET;
  Firebase.begin(&fbConfig, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase: begin OK");

  // Fingerprint init
  FingerSerial.begin(FP_BAUD, SERIAL_8N1, FP_RX, FP_TX);
  finger.begin(FP_BAUD);
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint: OK");
  } else {
    Serial.println("Fingerprint: NOT FOUND (check TX/RX or baud)");
  }

  showStandby();
}

void loop() {
  // Fingerprint
  checkFingerprint();

  // Standby refresh
  static unsigned long lastTime = 0;
  if (millis() - lastTime > 1000) {
    showStandby();
    lastTime = millis();
  }

  // RFID debounce
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

    // bỏ qua nếu quẹt lại quá nhanh
    if (uid == lastUID && (millis() - lastUIDms) < 1800) return;
    lastUID = uid;
    lastUIDms = millis();

    handleCard(uid);
    showStandby();
  }
}
