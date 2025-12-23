#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "time.h"
#include <Adafruit_Fingerprint.h>
#include <Firebase_ESP_Client.h>
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h> 

#define WIFI_SSID "UTC"
#define WIFI_PASS "00000000"
#define DATABASE_URL    "https://quanlychamcong-9dacd-default-rtdb.firebaseio.com/"
#define DATABASE_SECRET "Wu3ugJvvXLvIESnx4Po40PPLgACeYqeOmpEF1Bsl"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200;
const int   daylightOffset_sec = 0;

#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

#define RFID_SDA 21
#define RFID_RST 22
#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23

#define CAM_TX_PIN 32   
#define CAM_RX_PIN 33   
#define CAM_BAUD   9600

#define FP_RX    16       
#define FP_TX    17       
#define FP_BAUD 57600    

#define MP3_RX  25       
#define MP3_TX  26       
SoftwareSerial mySoftwareSerial(MP3_RX, MP3_TX); 
DFRobotDFPlayerMini myDFPlayer;

#define BUTTON_FP_PIN    13   
#define BUTTON_MUSIC_PIN 0    

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
MFRC522 rfid(RFID_SDA, RFID_RST);

HardwareSerial FingerSerial(2); 
Adafruit_Fingerprint finger(&FingerSerial);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbConfig;

bool isWifiConnected = false;
bool screenNeedsInit = true; 

void setupAudio() {
  mySoftwareSerial.begin(9600); 
  Serial.println(F("Dang khoi tao MP3..."));
  
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(F("Loi: Khong thay MP3!"));
  } else {
    Serial.println(F("MP3 Online."));
    myDFPlayer.volume(30); 
    myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
    myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  }
}

void speakThankYou() { myDFPlayer.playMp3Folder(1); }
void speakError()    { myDFPlayer.playMp3Folder(2); }
void playTestMusic() { myDFPlayer.playMp3Folder(3); }
void speakGuide1()   { myDFPlayer.playMp3Folder(5); }
void speakGuide2()   { myDFPlayer.playMp3Folder(6); }
void speakWelcome()  { myDFPlayer.playMp3Folder(7); }

String viToEn(String str) {
  String withMarks[] = { "á","à","ả","ã","ạ","ă","ắ","ằ","ẳ","ẵ","ặ","â","ấ","ầ","ẩ","ẫ","ậ","đ","é","è","ẻ","ẽ","ẹ","ê","ế","ề","ể","ễ","ệ","í","ì","ỉ","ĩ","ị","ó","ò","ỏ","õ","ọ","ô","ố","ồ","ổ","ỗ","ộ","ơ","ớ","ờ","ở","ỡ","ợ","ú","ù","ủ","ũ","ụ","ư","ứ","ừ","ử","ữ","ự","ý","ỳ","ỷ","ỹ","ỵ" };
  String withoutMarks[] = { "a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","d","e","e","e","e","e","e","e","e","e","e","e","i","i","i","i","i","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","u","u","u","u","u","u","u","u","u","u","u","y","y","y","y","y" };
  for (int i = 0; i < 67; i++) str.replace(withMarks[i], withoutMarks[i]); 
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

String enqueueNewEnroll(const String &src, const String &value) {
if (!Firebase.ready()) {
     Serial.println("Loi: Firebase chua san sang hoac mat WiFi");
     return "";
  }
  Firebase.RTDB.setString(&fbdo, "/new_enroll", value);
  FirebaseJson j;
  j.set("src", src);
  j.set("value", value);
  j.set("status", "pending");
  j.set("ts", nowTimestamp());
  j.set("device", deviceId());
  return Firebase.RTDB.pushJSON(&fbdo, "/new_enroll_queue", &j) ? fbdo.pushName() : "";
}

void initStandbyScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(15, 20); tft.print("XIN CHAO");
  tft.setCursor(40, 45); tft.print("BAN!");
  screenNeedsInit = false; 
}

void updateClock() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  char dateStr[20];
  sprintf(dateStr, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK); 
  tft.setTextSize(1);
  tft.setCursor(35, 80); tft.print(dateStr);
  char timeStr[20];
  sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); 
  tft.setTextSize(2);
  tft.setCursor(15, 100); tft.print(timeStr);
}

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
  tft.setCursor(10, 10); tft.print("THEM VTAY");
  
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1);
  tft.setCursor(10, 40); tft.print("Dat tay lan 1...");
  tft.setCursor(10, 60); tft.print("ID: " + String(id));
  
  speakGuide1(); 
  delay(2200);   

  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (digitalRead(BUTTON_FP_PIN) == LOW) { screenNeedsInit = true; return; } 
  }
  finger.image2Tz(1);
  
  tft.setTextColor(ST77XX_GREEN); tft.setCursor(10, 80); tft.print("Lay tay ra...");
  delay(1000);
  while (p != FINGERPRINT_NOFINGER) { p = finger.getImage(); }

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN); tft.setTextSize(2);
  tft.setCursor(10, 10); tft.print("XAC NHAN");
  tft.setTextColor(ST77XX_WHITE); tft.setTextSize(1);
  tft.setCursor(10, 40); tft.print("Dat lai lan 2...");
  
  speakGuide2(); 
  delay(2200);   

  p = -1; while (p != FINGERPRINT_OK) { p = finger.getImage(); }
  finger.image2Tz(2);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    p = finger.storeModel(id);
    if (p == FINGERPRINT_OK) {
      tft.fillScreen(ST77XX_GREEN);
      tft.setTextColor(ST77XX_BLACK); tft.setTextSize(2);
      tft.setCursor(10, 40); tft.print("THANH CONG!");
      
      speakThankYou(); 
      
      struct tm timeinfo; getLocalTime(&timeinfo);
      char uniqueCode[30]; strftime(uniqueCode, 30, "%Y%m%d%H%M%S", &timeinfo);
      Serial1.println("SNAP:FINGER_" + String(id) + "_" + String(uniqueCode));
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

void handleCard(const String &uid) {
  Serial.println("Card: " + uid);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) Serial.println("Loi time");
  
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

  String newStatus = (currentStatus == "IN") ? "OUT" : "IN";

  FirebaseJson json;
  json.set("id", uid);
  json.set("timestamp", nowTimestamp());
  json.set("type", "RFID");
  json.set("device", deviceId());
  json.set("image", imgFileName + ".jpg");
  json.set("status", newStatus); 

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
    if (Firebase.ready()) {
        Firebase.RTDB.pushJSON(&fbdo, "/attendance", &json);
        Firebase.RTDB.setString(&fbdo, statusPath, newStatus);
    }
    delay(2500);
  } else {
    tft.fillScreen(ST77XX_RED);
    tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2);
    tft.setCursor(20, 40); tft.print("THE LA");
    tft.setTextSize(1); tft.setCursor(5, 80); tft.print("ID: " + uid);
    
    speakError();
    json.set("name", "Nguoi La (" + uid + ")");
    if (Firebase.ready()) Firebase.RTDB.pushJSON(&fbdo, "/attendance", &json);
    static unsigned long lastCardEnrollMs = 0;
    if (cooldown(lastCardEnrollMs, 2000)) enqueueNewEnroll("CARD", uid);
    delay(2000);
  }
  screenNeedsInit = true; 
}

void checkFingerprint() {
  static unsigned long lastPoll = 0;
  if (!cooldown(lastPoll, 100)) return; 

  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) { return; } 

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
    Serial1.println("SNAP:UNKNOWN_" + String(uniqueCode));
    
    delay(1500);
    screenNeedsInit = true;
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(CAM_BAUD, SERIAL_8N1, CAM_RX_PIN, CAM_TX_PIN);
  
  setupAudio(); 

  pinMode(BUTTON_FP_PIN, INPUT_PULLUP);    
  pinMode(BUTTON_MUSIC_PIN, INPUT_PULLUP); 

  pinMode(TFT_CS, OUTPUT);   digitalWrite(TFT_CS, HIGH);
  pinMode(RFID_SDA, OUTPUT); digitalWrite(RFID_SDA, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE); tft.setCursor(10, 10); tft.print("Khoi dong WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) { 
    delay(300); Serial.print("."); retry++; 
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    tft.setCursor(10, 30); tft.print("WiFi OK!");
    
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.print("Sync Time");
    struct tm timeinfo;
    int retryTime = 0;
    while(!getLocalTime(&timeinfo) && retryTime < 50){
        Serial.print("."); delay(100); retryTime++;
    }
    Serial.println(" OK");
    
    fbConfig.database_url = DATABASE_URL;
    fbConfig.signer.tokens.legacy_token = DATABASE_SECRET;
    
    fbdo.setBSSLBufferSize(2048, 1024); 
    
    fbdo.setResponseSize(1024);
    
    fbConfig.timeout.wifiReconnect = 10000;
    fbConfig.timeout.socketConnection = 30000; 
    fbConfig.timeout.sslHandshake = 30000;     
    
    Firebase.begin(&fbConfig, &auth);
    Firebase.reconnectWiFi(true);
  } else {
    tft.setCursor(10, 30); tft.print("WiFi Failed!");
    delay(1000);
  }

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

  initStandbyScreen(); 
  
  Serial.println("Cho MP3 on dinh...");
  delay(1500);
  speakWelcome(); 
}

void loop() {
  if (screenNeedsInit) {
    initStandbyScreen();
  }
  
  static unsigned long lastTime = 0;
  if (millis() - lastTime > 1000) {
    updateClock(); 
    lastTime = millis();
  }

  if (digitalRead(BUTTON_FP_PIN) == LOW) {
    delay(50); 
    if (digitalRead(BUTTON_FP_PIN) == LOW) {
       enrollNewFinger();
       while(digitalRead(BUTTON_FP_PIN) == LOW); 
    }
  }

  if (digitalRead(BUTTON_MUSIC_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_MUSIC_PIN) == LOW) {
       playTestMusic();
       delay(500); 
    }
  }

  checkFingerprint();

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    static String lastUID = "";
    static unsigned long lastUIDms = 0;
    if (uid == lastUID && (millis() - lastUIDms) < 2000) return;
    
    lastUID = uid;
    lastUIDms = millis();

    handleCard(uid);
  }
}
