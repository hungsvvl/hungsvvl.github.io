/*
 * ESP32 MASTER - FULL FEATURES (FINAL)
 * - RFID (SPI) + ESP32-CAM (Serial1) + FINGERPRINT (Serial2)
 * - AUDIO: MP3-TF-16P (SoftwareSerial)
 * - DATABASE: Firebase RTDB
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

// --- THƯ VIỆN MP3 & SERIAL MỀM ---
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h> 

// ================= 1) WIFI + FIREBASE =================
#define WIFI_SSID "UTC"
#define WIFI_PASS "00000000"
#define SPLIT_HOUR 13  
#define DATABASE_URL    "https://quanlychamcong-9dacd-default-rtdb.firebaseio.com/"
#define DATABASE_SECRET "Wu3ugJvvXLvIESnx4Po40PPLgACeYqeOmpEF1Bsl"

// Time VN
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200;
const int   daylightOffset_sec = 0;

// ================= 2) PINOUT =================
// --- TFT ST7735 ---
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

// --- RFID RC522 ---
#define RFID_SDA 21
#define RFID_RST 22
#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23

// --- UART ESP32-CAM (Serial1) ---
#define CAM_TX_PIN 32   
#define CAM_RX_PIN 33   
#define CAM_BAUD   9600

// --- FINGERPRINT (Serial2) ---
#define FP_RX   16      // Dây xanh lá (TX cảm biến -> RX2 ESP)
#define FP_TX   17      // Dây trắng (RX cảm biến <- TX2 ESP)
#define FP_BAUD 57600   

// --- MP3 PLAYER (SoftwareSerial) ---
#define MP3_RX  25      // Nối vào TX của Module MP3
#define MP3_TX  26      // Nối vào RX của Module MP3
SoftwareSerial mySoftwareSerial(MP3_RX, MP3_TX); 
DFRobotDFPlayerMini myDFPlayer;

// --- KHÁC ---
#define BUTTON_PIN 0    // Nút BOOT để đăng ký

// ================= 3) OBJECTS =================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
MFRC522 rfid(RFID_SDA, RFID_RST);

HardwareSerial FingerSerial(2); // Dùng Hardware Serial 2 cho vân tay
Adafruit_Fingerprint finger(&FingerSerial);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbConfig;

// ================= 4) TIỆN ÍCH & ÂM THANH MP3 =================

void setupAudio() {
  mySoftwareSerial.begin(9600); // MP3 giao tiếp 9600
  Serial.println(F("Dang khoi tao MP3..."));

  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(F("Loi: Khong thay MP3 hoac The nho loi!"));
    // Không while(true) để hệ thống vẫn chạy dù hỏng loa
  } else {
    Serial.println(F("MP3 Online."));
    myDFPlayer.volume(30);  // Âm lượng 0-30
  }
}

void speakThankYou() {
  // Phát file 0001.mp3 (Cảm ơn bạn)
  Serial.println("Loa: Cam on ban");
  myDFPlayer.play(1); 
}

void speakError() {
  // Phát file 0002.mp3 (Thử lại/Lỗi)
  Serial.println("Loa: Error/Beep");
  myDFPlayer.play(2); 
}

// Thay thế hàm beep cũ bằng MP3 hoặc còi chip (nếu có)
void beep(int times) {
   // Nếu muốn dùng file mp3 số 2 làm tiếng bíp
   if(times > 0) myDFPlayer.play(2);
}

// --- CÁC HÀM TIỆN ÍCH KHÁC GIỮ NGUYÊN ---
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
  if (id == -1) { speakError(); return; }

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
    if (digitalRead(BUTTON_PIN) == LOW) return; 
  }
  finger.image2Tz(1);
  
  tft.setTextColor(ST77XX_GREEN); tft.setCursor(10, 80); tft.print("Lay tay ra...");
  speakError(); // Dùng tiếng bíp nhẹ
  delay(1000);
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
      
      struct tm timeinfo; getLocalTime(&timeinfo);
      char uniqueCode[30]; strftime(uniqueCode, 30, "%Y%m%d%H%M%S", &timeinfo);
      String imgName = "FINGER_" + String(id) + "_" + String(uniqueCode);
      Serial1.println("SNAP:" + imgName);

      enqueueNewEnroll("FINGER", "FINGER_" + String(id));
      
    } else {
      tft.fillScreen(ST77XX_RED); tft.print("Loi Flash"); speakError();
    }
  } else {
    tft.fillScreen(ST77XX_RED); tft.print("Khong khop!"); speakError();
  }
  delay(2000);
}

// ================= 9) RFID HANDLE =================
void handleCard(const String &uid) {
  Serial.println("Card Detected: " + uid);

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) Serial.println("Loi lay gio!");
  
  char dayStr[10]; strftime(dayStr, 10, "%Y%m%d", &timeinfo);
  char uniqueCode[30]; strftime(uniqueCode, 30, "%Y%m%d%H%M%S", &timeinfo);
  String imgFileName = uid + "_" + String(uniqueCode); 

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 50); tft.setTextColor(ST77XX_YELLOW); tft.setTextSize(1);
  tft.print("Dang kiem tra...");

  Serial1.println("SNAP:" + imgFileName);

  String name = "";
  String currentStatus = ""; 
  String statusPath = "/daily_check/" + String(dayStr) + "/" + uid; 

  if (Firebase.ready()) {
    if (Firebase.RTDB.getString(&fbdo, "/users/" + uid + "/name")) name = fbdo.stringData();
    if (Firebase.RTDB.getString(&fbdo, statusPath)) currentStatus = fbdo.stringData();
  }

  String newStatus = "IN"; 
  if (currentStatus == "IN") newStatus = "OUT";

  FirebaseJson json;
  json.set("id", uid);
  json.set("timestamp", nowTimestamp());
  json.set("type", "RFID");
  json.set("device", deviceId());
  json.set("image", imgFileName + ".jpg");
  json.set("status", newStatus); 

  if (name.length() > 0) {
    // >> NGƯỜI QUEN
    tft.fillScreen(ST77XX_GREEN);
    tft.setTextColor(ST77XX_BLACK); 
    
    tft.setTextSize(2); tft.setCursor(10, 20); tft.print(viToEn(name));
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
    
    speakThankYou(); // PHÁT MP3
    
    json.set("name", name);
    if (Firebase.ready()) Firebase.RTDB.pushJSON(&fbdo, "/attendance", &json);
    if (Firebase.ready()) Firebase.RTDB.setString(&fbdo, statusPath, newStatus);
    
    delay(2500);
  } 
  else {
    // >> NGƯỜI LẠ
    tft.fillScreen(ST77XX_RED);
    tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2);
    tft.setCursor(20, 40); tft.print("THE LA");
    tft.setTextSize(1); tft.setCursor(5, 80); tft.print("ID: " + uid);
    
    speakError(); // PHÁT TIẾNG LỖI

    json.set("name", "Nguoi La (" + uid + ")");
    if (Firebase.ready()) Firebase.RTDB.pushJSON(&fbdo, "/attendance", &json);

    static unsigned long lastCardEnrollMs = 0;
    if (cooldown(lastCardEnrollMs, 1200)) enqueueNewEnroll("CARD", uid);
    
    delay(2000);
  }
}

// ================= 10) FINGERPRINT CHECK =================
void checkFingerprint() {
  static unsigned long lastPoll = 0;
  if (!cooldown(lastPoll, 150)) return;

  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) { speakError(); return; } 

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    String val = "FINGER_" + String(finger.fingerID);
    handleCard(val); 
  } else {
    tft.fillScreen(ST77XX_RED);
    tft.setCursor(10, 50); tft.print("CHUA DANG KY!");
    speakError();
    
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
  
  // 1. Khởi tạo Camera Serial
  Serial1.begin(CAM_BAUD, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);

  // 2. Khởi tạo MP3 (SoftwareSerial)
  setupAudio(); 

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TFT_CS, OUTPUT);   digitalWrite(TFT_CS, HIGH);
  pinMode(RFID_SDA, OUTPUT); digitalWrite(RFID_SDA, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1);
  tft.setCursor(10, 10); tft.print("Khoi dong...");

  rfid.PCD_Init();
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  fbConfig.database_url = DATABASE_URL;
  fbConfig.signer.tokens.legacy_token = DATABASE_SECRET;
  fbConfig.timeout.wifiReconnect = 10000;
  fbdo.setResponseSize(2048); 
  Firebase.begin(&fbConfig, &auth);
  Firebase.reconnectWiFi(true);

  // 3. Khởi tạo Vân tay (HardwareSerial 2)
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
  // Nút bấm thêm vân tay
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
       enrollNewFinger();
       showStandby();    
       while(digitalRead(BUTTON_PIN) == LOW); 
    }
  }

  checkFingerprint();

  static unsigned long lastTime = 0;
  if (millis() - lastTime > 1000) {
    showStandby();
    lastTime = millis();
  }

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
