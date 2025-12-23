/*
 * DỰ ÁN: MÁY CHẤM CÔNG THÔNG MINH (ESP32 + RFID + VÂN TAY + FIREBASE)
 * Chức năng:
 * 1. Quét thẻ từ (RFID) hoặc vân tay để điểm danh.
 * 2. Gửi dữ liệu (Thời gian, ID, Hình ảnh - giả lập tên file) lên Firebase Realtime Database.
 * 3. Hiển thị thông tin người dùng lên màn hình TFT.
 * 4. Phát âm thanh thông báo qua loa (DFPlayer Mini).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>            // Thư viện đọc thẻ từ RC522
#include <Adafruit_GFX.h>       // Thư viện đồ họa cốt lõi
#include <Adafruit_ST7735.h>    // Thư viện cho màn hình TFT ST7735
#include "time.h"               // Thư viện lấy giờ qua mạng (NTP)
#include <Adafruit_Fingerprint.h> // Thư viện cảm biến vân tay
#include <Firebase_ESP_Client.h>  // Thư viện kết nối Firebase
#include "DFRobotDFPlayerMini.h"  // Thư viện máy nghe nhạc MP3
#include <SoftwareSerial.h>       // Giao tiếp Serial mềm cho MP3

// ================= 1. CẤU HÌNH WIFI & FIREBASE =================
#define WIFI_SSID "UTC"           // Tên Wifi
#define WIFI_PASS "00000000"      // Mật khẩu Wifi
// Đường dẫn database Firebase
#define DATABASE_URL    "https://quanlychamcong-9dacd-default-rtdb.firebaseio.com/"
// Khóa bí mật (Database Secret) lấy từ Project Settings > Service Accounts
#define DATABASE_SECRET "Wu3ugJvvXLvIESnx4Po40PPLgACeYqeOmpEF1Bsl"

// Cấu hình múi giờ Việt Nam (UTC+7)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200;      // 7 * 3600 = 25200 giây
const int   daylightOffset_sec = 0;

// ================= 2. CẤU HÌNH CHÂN KẾT NỐI (PINOUT) =================
// --- Màn hình TFT ---
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

// --- Thẻ từ RFID (Giao tiếp SPI) ---
#define RFID_SDA 21
#define RFID_RST 22
#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23

// --- Camera (Dự phòng/Mở rộng sau này) ---
#define CAM_TX_PIN 32   
#define CAM_RX_PIN 33   
#define CAM_BAUD   9600

// --- Cảm biến vân tay (Giao tiếp Serial 2) ---
#define FP_RX    16       
#define FP_TX    17       
#define FP_BAUD 57600    

// --- Module MP3 (Giao tiếp SoftwareSerial) ---
#define MP3_RX  25       
#define MP3_TX  26       
SoftwareSerial mySoftwareSerial(MP3_RX, MP3_TX); 
DFRobotDFPlayerMini myDFPlayer;

// --- Các nút nhấn ---
#define BUTTON_FP_PIN    13   // Nút đăng ký vân tay mới
#define BUTTON_MUSIC_PIN 0    // Nút test nhạc/loa

// ================= 3. KHỞI TẠO ĐỐI TƯỢNG =================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
MFRC522 rfid(RFID_SDA, RFID_RST);

// Dùng HardwareSerial số 2 cho vân tay để ổn định hơn
HardwareSerial FingerSerial(2); 
Adafruit_Fingerprint finger(&FingerSerial);

// Đối tượng Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbConfig;

bool isWifiConnected = false;
bool screenNeedsInit = true; // Cờ kiểm tra xem có cần vẽ lại màn hình chờ không

// ================= 4. CÁC HÀM XỬ LÝ ÂM THANH & TIỆN ÍCH =================

// Hàm khởi động module MP3
void setupAudio() {
  mySoftwareSerial.begin(9600); 
  Serial.println(F("Dang khoi tao MP3..."));
  
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(F("Loi: Khong thay MP3!"));
  } else {
    Serial.println(F("MP3 Online."));
    myDFPlayer.volume(30); // Âm lượng tối đa (0-30)
    // Chỉnh EQ (Equalizer) sang chế độ Normal
    myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
    myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  }
}

// Các hàm phát âm thanh theo số thứ tự file trong thư mục /MP3/ trên thẻ nhớ
// Ví dụ: speakThankYou() sẽ chạy file "0001.mp3"
void speakThankYou() { myDFPlayer.playMp3Folder(1); } // Cảm ơn
void speakError()    { myDFPlayer.playMp3Folder(2); } // Lỗi/Thử lại
void playTestMusic() { myDFPlayer.playMp3Folder(3); } // Nhạc test
void speakGuide1()   { myDFPlayer.playMp3Folder(5); } // Hướng dẫn đặt tay lần 1
void speakGuide2()   { myDFPlayer.playMp3Folder(6); } // Hướng dẫn đặt tay lần 2
void speakWelcome()  { myDFPlayer.playMp3Folder(7); } // Lời chào khi khởi động

// Hàm chuyển tiếng Việt có dấu sang không dấu (để hiển thị lên màn hình hoặc log)
String viToEn(String str) {
  String withMarks[] = { "á","à","ả","ã","ạ","ă","ắ","ằ","ẳ","ẵ","ặ","â","ấ","ầ","ẩ","ẫ","ậ","đ","é","è","ẻ","ẽ","ẹ","ê","ế","ề","ể","ễ","ệ","í","ì","ỉ","ĩ","ị","ó","ò","ỏ","õ","ọ","ô","ố","ồ","ổ","ỗ","ộ","ơ","ớ","ờ","ở","ỡ","ợ","ú","ù","ủ","ũ","ụ","ư","ứ","ừ","ử","ữ","ự","ý","ỳ","ỷ","ỹ","ỵ" };
  String withoutMarks[] = { "a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","d","e","e","e","e","e","e","e","e","e","e","e","i","i","i","i","i","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","u","u","u","u","u","u","u","u","u","u","u","y","y","y","y","y" };
  for (int i = 0; i < 67; i++) str.replace(withMarks[i], withoutMarks[i]); 
  return str;
}

// Lấy Unique Device ID từ địa chỉ MAC của ESP32
String deviceId() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[20];
  snprintf(buf, sizeof(buf), "%04X%08X", (uint16_t)(mac >> 32), (uint32_t)mac);
  return String(buf);
}

// Lấy thời gian hiện tại định dạng chuỗi "dd/mm/YYYY HH:MM:SS"
String nowTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return String(millis()); // Nếu lỗi thì trả về millis
  char buf[24];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buf);
}

// Hàm làm trễ không chặn (Non-blocking delay)
bool cooldown(unsigned long &lastMs, uint32_t gapMs) {
  unsigned long m = millis();
  if (m - lastMs < gapMs) return false;
  lastMs = m;
  return true;
}

// ================= 5. GIAO TIẾP FIREBASE =================

// Đẩy yêu cầu đăng ký người mới lên hàng đợi (Queue) trên Firebase
String enqueueNewEnroll(const String &src, const String &value) {
if (!Firebase.ready()) {
     Serial.println("Loi: Firebase chua san sang hoac mat WiFi");
     return "";
  }
  // Ghi đè vào node /new_enroll để web app nhận biết ngay
  Firebase.RTDB.setString(&fbdo, "/new_enroll", value);
  
  // Tạo JSON chi tiết để lưu lịch sử yêu cầu
  FirebaseJson j;
  j.set("src", src);       // Nguồn (CARD hoặc FINGER)
  j.set("value", value);   // Giá trị ID
  j.set("status", "pending");
  j.set("ts", nowTimestamp());
  j.set("device", deviceId());
  
  // Đẩy vào danh sách hàng đợi
  return Firebase.RTDB.pushJSON(&fbdo, "/new_enroll_queue", &j) ? fbdo.pushName() : "";
}

// ================= 6. GIAO DIỆN MÀN HÌNH (UI) =================

// Vẽ màn hình chờ mặc định
void initStandbyScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(15, 20); tft.print("XIN CHAO");
  tft.setCursor(40, 45); tft.print("BAN!");
  screenNeedsInit = false; 
}

// Cập nhật đồng hồ số lên màn hình (chạy mỗi giây)
void updateClock() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  
  // Hiển thị ngày tháng
  char dateStr[20];
  sprintf(dateStr, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK); 
  tft.setTextSize(1);
  tft.setCursor(35, 80); tft.print(dateStr);
  
  // Hiển thị giờ phút giây
  char timeStr[20];
  sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); 
  tft.setTextSize(2);
  tft.setCursor(15, 100); tft.print(timeStr);
}

// ================= 7. XỬ LÝ VÂN TAY =================

// Tìm ID vân tay trống tiếp theo (từ 1 đến 127)
int getFingerprintID() {
  int p = -1;
  for (int i = 1; i < 127; i++) {
    p = finger.loadModel(i);
    if (p != FINGERPRINT_OK) return i; // Nếu ID chưa có model, trả về ID đó
  }
  return -1; // Bộ nhớ đầy
}

// Quy trình đăng ký vân tay mới (Nhấn nút -> Quét lần 1 -> Quét lần 2 -> Lưu)
void enrollNewFinger() {
  int id = getFingerprintID();
  if (id == -1) { speakError(); return; } // Hết bộ nhớ

  // Bước 1: Hướng dẫn đặt tay
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN); tft.setTextSize(2);
  tft.setCursor(10, 10); tft.print("THEM VTAY");
  
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1);
  tft.setCursor(10, 40); tft.print("Dat tay lan 1...");
  tft.setCursor(10, 60); tft.print("ID: " + String(id));
  
  speakGuide1(); 
  delay(2200);   

  // Chờ ngón tay đặt vào
  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    // Nếu nhấn nút lần nữa thì hủy bỏ
    if (digitalRead(BUTTON_FP_PIN) == LOW) { screenNeedsInit = true; return; } 
  }
  finger.image2Tz(1); // Lưu ảnh lần 1 vào bộ đệm 1
  
  // Yêu cầu nhấc tay ra
  tft.setTextColor(ST77XX_GREEN); tft.setCursor(10, 80); tft.print("Lay tay ra...");
  delay(1000);
  while (p != FINGERPRINT_NOFINGER) { p = finger.getImage(); }

  // Bước 2: Xác nhận lại
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN); tft.setTextSize(2);
  tft.setCursor(10, 10); tft.print("XAC NHAN");
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1);
  tft.setCursor(10, 40); tft.print("Dat lai lan 2...");
  
  speakGuide2(); 
  delay(2200);   

  p = -1; while (p != FINGERPRINT_OK) { p = finger.getImage(); }
  finger.image2Tz(2); // Lưu ảnh lần 2 vào bộ đệm 2

  // Tạo model từ 2 lần quét
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    // Lưu model vào Flash bộ nhớ
    p = finger.storeModel(id);
    if (p == FINGERPRINT_OK) {
      tft.fillScreen(ST77XX_GREEN);
      tft.setTextColor(ST77XX_BLACK); tft.setTextSize(2);
      tft.setCursor(10, 40); tft.print("THANH CONG!");
      
      speakThankYou(); 
      
      // Gửi mã vân tay mới lên Firebase để đăng ký User
      struct tm timeinfo; getLocalTime(&timeinfo);
      char uniqueCode[30]; strftime(uniqueCode, 30, "%Y%m%d%H%M%S", &timeinfo);
      Serial1.println("SNAP:FINGER_" + String(id) + "_" + String(uniqueCode)); // Lệnh chụp ảnh (nếu có cam)
      enqueueNewEnroll("FINGER", "FINGER_" + String(id));
    } else {
      tft.fillScreen(ST77XX_RED); tft.print("Loi Flash"); speakError();
    }
  } else {
    tft.fillScreen(ST77XX_RED); tft.setCursor(10,40); tft.print("Khong khop!"); speakError();
  }
  delay(2000);
  screenNeedsInit = true; 
}

// ================= 8. XỬ LÝ LOGIC CHÍNH (ĐIỂM DANH) =================

// Hàm xử lý khi có thẻ từ hoặc vân tay hợp lệ
void handleCard(const String &uid) {
  Serial.println("Card: " + uid);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) Serial.println("Loi time");
  
  // Tạo tên file ảnh dựa trên thời gian
  char dayStr[10]; strftime(dayStr, 10, "%Y%m%d", &timeinfo);
  char uniqueCode[30]; strftime(uniqueCode, 30, "%Y%m%d%H%M%S", &timeinfo);
  String imgFileName = uid + "_" + String(uniqueCode); 

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 50); tft.setTextColor(ST77XX_YELLOW); tft.setTextSize(1);
  tft.print("Dang kiem tra...");

  Serial1.println("SNAP:" + imgFileName); // Gửi lệnh chụp ảnh qua UART

  String name = "";
  String currentStatus = ""; 
  String statusPath = "/daily_check/" + String(dayStr) + "/" + uid; 

  // Truy vấn Firebase để lấy tên người dùng
  if (Firebase.ready()) {
    if (Firebase.RTDB.getString(&fbdo, "/users/" + uid + "/name")) name = fbdo.stringData();
    if (Firebase.RTDB.getString(&fbdo, statusPath)) currentStatus = fbdo.stringData();
  }

  // Đảo trạng thái: Nếu đang IN thì thành OUT, ngược lại
  String newStatus = (currentStatus == "IN") ? "OUT" : "IN";

  // Chuẩn bị JSON dữ liệu chấm công
  FirebaseJson json;
  json.set("id", uid);
  json.set("timestamp", nowTimestamp());
  json.set("type", "RFID");
  json.set("device", deviceId());
  json.set("image", imgFileName + ".jpg");
  json.set("status", newStatus); 

  // Nếu tìm thấy tên (đã đăng ký)
  if (name.length() > 0) {
    tft.fillScreen(ST77XX_GREEN);
    tft.setTextColor(ST77XX_BLACK); 
    tft.setTextSize(2); tft.setCursor(10, 20); tft.print(viToEn(name));
    
    tft.setTextSize(1); tft.setCursor(10, 60);
    if (newStatus == "IN") {
      tft.print("XIN CHAO!");        
      tft.setCursor(10, 80); tft.setTextSize(2); tft.print("VAO LAM");
    } else {
      tft.print("TAM BIET!");        
      tft.setCursor(10, 80); tft.setTextSize(2); tft.print("RA VE");
    }
    
    speakThankYou();
    json.set("name", name);
    // Gửi dữ liệu lên Firebase
    if (Firebase.ready()) {
        Firebase.RTDB.pushJSON(&fbdo, "/attendance", &json); // Lưu lịch sử
        Firebase.RTDB.setString(&fbdo, statusPath, newStatus); // Cập nhật trạng thái ngày
    }
    delay(2500);
  } else {
    // Nếu thẻ lạ (Chưa đăng ký)
    tft.fillScreen(ST77XX_RED);
    tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2);
    tft.setCursor(20, 40); tft.print("THE LA");
    tft.setTextSize(1); tft.setCursor(5, 80); tft.print("ID: " + uid);
    
    speakError();
    json.set("name", "Nguoi La (" + uid + ")");
    // Ghi nhận thẻ lạ lên Firebase để Admin biết
    if (Firebase.ready()) Firebase.RTDB.pushJSON(&fbdo, "/attendance", &json);
    
    // Nếu thẻ này quẹt liên tục thì không gửi lại (chống spam)
    static unsigned long lastCardEnrollMs = 0;
    if (cooldown(lastCardEnrollMs, 2000)) enqueueNewEnroll("CARD", uid);
    delay(2000);
  }
  screenNeedsInit = true; 
}

// Kiểm tra xem có ai đặt tay lên cảm biến vân tay không
void checkFingerprint() {
  static unsigned long lastPoll = 0;
  if (!cooldown(lastPoll, 100)) return; // Chỉ kiểm tra mỗi 100ms

  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return; // Không có ngón tay

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) { return; } 

  // Tìm kiếm vân tay trong bộ nhớ
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    // Nếu tìm thấy: Giả lập một ID dạng "FINGER_ID"
    String val = "FINGER_" + String(finger.fingerID);
    handleCard(val); 
  } else {
    // Vân tay chưa đăng ký
    tft.fillScreen(ST77XX_RED);
    tft.setCursor(10, 50); tft.print("CHUA DANG KY!");
    speakError();
    
    struct tm timeinfo; getLocalTime(&timeinfo);
    char uniqueCode[30]; strftime(uniqueCode, 30, "%Y%m%d%H%M%S", &timeinfo);
    Serial1.println("SNAP:UNKNOWN_" + String(uniqueCode));
    
    delay(1500);
    screenNeedsInit = true;
  }
}

// ================= 9. SETUP (CHẠY 1 LẦN) =================
void setup() {
  Serial.begin(115200);
  // Khởi tạo UART cho Camera
  Serial1.begin(CAM_BAUD, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);
  
  setupAudio(); // Khởi tạo Loa

  // Cấu hình nút nhấn (INPUT_PULLUP: không nhấn = HIGH, nhấn = LOW)
  pinMode(BUTTON_FP_PIN, INPUT_PULLUP);    
  pinMode(BUTTON_MUSIC_PIN, INPUT_PULLUP); 

  // Cấu hình chân CS cho SPI
  pinMode(TFT_CS, OUTPUT);   digitalWrite(TFT_CS, HIGH);
  pinMode(RFID_SDA, OUTPUT); digitalWrite(RFID_SDA, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // Khởi tạo màn hình
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1); // Xoay ngang
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE); tft.setCursor(10, 10); tft.print("Khoi dong WiFi...");

  // Kết nối WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) { 
    delay(300); Serial.print("."); retry++; 
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    tft.setCursor(10, 30); tft.print("WiFi OK!");
    
    // Đồng bộ thời gian thực từ Internet
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.print("Sync Time");
    struct tm timeinfo;
    int retryTime = 0;
    while(!getLocalTime(&timeinfo) && retryTime < 50){
        Serial.print("."); delay(100); retryTime++;
    }
    Serial.println(" OK");
    
    // Cấu hình Firebase
    fbConfig.database_url = DATABASE_URL;
    fbConfig.signer.tokens.legacy_token = DATABASE_SECRET;
    
    // [QUAN TRỌNG] Giảm bộ nhớ đệm SSL để tránh lỗi hết RAM trên ESP32
    fbdo.setBSSLBufferSize(2048, 1024); 
    fbdo.setResponseSize(1024);
    
    // Tăng thời gian chờ để mạng ổn định hơn
    fbConfig.timeout.wifiReconnect = 10000;
    fbConfig.timeout.socketConnection = 30000; 
    fbConfig.timeout.sslHandshake = 30000;     
    
    Firebase.begin(&fbConfig, &auth);
    Firebase.reconnectWiFi(true);
  } else {
    tft.setCursor(10, 30); tft.print("WiFi Failed!");
    delay(1000);
  }

  // Khởi tạo RFID và Vân tay
  rfid.PCD_Init();
  FingerSerial.begin(FP_BAUD, SERIAL_8N1, FP_RX, FP_TX);
  finger.begin(FP_BAUD);
  
  tft.fillScreen(ST77XX_BLACK);
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint: OK");
  } else {
    tft.setTextColor(ST77XX_RED); tft.setCursor(0, 20); tft.print("Loi Van Tay!");
    delay(2000);
  }

  initStandbyScreen(); // Vào màn hình chờ
  
  // Đợi MP3 ổn định rồi phát lời chào
  Serial.println("Cho MP3 on dinh...");
  delay(1500);
  speakWelcome(); 
}

// ================= 10. LOOP (CHẠY LIÊN TỤC) =================
void loop() {
  // Nếu cờ báo cần vẽ lại màn hình (sau khi thông báo xong)
  if (screenNeedsInit) {
    initStandbyScreen();
  }
  
  // Cập nhật đồng hồ mỗi giây
  static unsigned long lastTime = 0;
  if (millis() - lastTime > 1000) {
    updateClock(); 
    lastTime = millis();
  }

  // Xử lý nút nhấn Đăng ký vân tay
  if (digitalRead(BUTTON_FP_PIN) == LOW) {
    delay(50); // Chống dội phím (Debounce)
    if (digitalRead(BUTTON_FP_PIN) == LOW) {
       enrollNewFinger();
       while(digitalRead(BUTTON_FP_PIN) == LOW); // Chờ nhả nút
    }
  }

  // Xử lý nút nhấn Test nhạc
  if (digitalRead(BUTTON_MUSIC_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_MUSIC_PIN) == LOW) {
       playTestMusic();
       delay(500); 
    }
  }

  // Kiểm tra cảm biến vân tay
  checkFingerprint();

  // Kiểm tra thẻ RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    // Đọc UID thẻ
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();

    // Dừng thẻ để tiết kiệm năng lượng
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    // Chống đọc lặp lại thẻ trong 2 giây
    static String lastUID = "";
    static unsigned long lastUIDms = 0;
    if (uid == lastUID && (millis() - lastUIDms) < 2000) return;
    
    lastUID = uid;
    lastUIDms = millis();

    // Xử lý logic điểm danh cho thẻ này
    handleCard(uid);
  }
}
