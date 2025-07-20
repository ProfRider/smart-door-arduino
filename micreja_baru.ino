#include <WiFi.h> 
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ✅ Inisialisasi kredensial Firebase dan WiFi
#define API_KEY "AIzaSyAkkO4xMqMBmZU4zryYjlmEjx8Nnurb9aw"
#define USER_EMAIL "admin@gmail.com"
#define USER_PASSWORD "123456"
#define DATABASE_URL "https://skripsi-reza-fe867-default-rtdb.firebaseio.com"
#define DOOR_STATUS_PATH "/pintu/kondisi" // Path di Firebase untuk status pintu

const char* ssid = "Reza";         // Nama WiFi (SSID)
const char* password = "12345678"; // Password WiFi

// ✅ Definisi pin dan variabel kontrol hardware
#define LED 2                     // Pin LED indikator
#define manual 13                 // Pin push button manual
#define SENSOR_PIN 17             // Pin sensor magnetik MC-38
#define RELAY 26                  // Pin relay untuk solenoid lock
#define knockSensor 36            // Pin input sensor piezoelectric

bool isDoorUnlocked = false;      // Status apakah pintu sedang terbuka
unsigned long unlockStartTime = 0; // Waktu mulai pintu terbuka
unsigned long unlockDuration = 5000; // Lama pintu terbuka (default 5 detik)

// ✅ Parameter deteksi knock code
int threshold = 100;               // Batas minimal untuk mendeteksi ketukan
const int rejectValue = 25;        // Batas toleransi kesalahan
const int averageRejectValue = 15; // Rata-rata toleransi
const int knockFadeTime = 150;     // Delay antara ketukan
const int lockOperateTime = 3000;  // Waktu operasi kunci pintu (3 detik)
const int maximumKnocks = 20;      // Maksimum jumlah ketukan dalam satu pola
const int knockComplete = 1200;    // Waktu maksimal selesainya pola ketukan

// ✅ Variabel untuk pengecekan koneksi dan Firebase secara periodik
unsigned long lastCheck = 0;
const unsigned long interval = 5000; // Interval pengecekan Firebase (5 detik)
unsigned long lastCheckTime = 0; // Waktu terakhir pengecekan koneksi
const unsigned long checkInterval = 30000; // Interval pengecekan koneksi (30 detik)
unsigned long lastWiFiCheck = 0; // Waktu terakhir pengecekan WiFi
const unsigned long wifiCheckInterval = 10000; // Interval pengecekan WiFi (10 detik)

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
String uid;
int lastState = -1; // Menyimpan status terakhir pintu (terbuka/tertutup)

// ✅ Array untuk menyimpan pola knock code rahasia dan pembacaan knock
byte secretCode[maximumKnocks] = {50, 100, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Pola default
int knockReadings[maximumKnocks];    // Menyimpan hasil pembacaan ketukan
int knockSensorValue = 0;            // Nilai analog pembacaan sensor piezo
boolean programModeActive = false;   // Mode pemrograman knock code

// ✅ Fungsi koneksi WiFi
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting WLAN ");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(". Done, device connected.");
}

// ✅ Fungsi untuk menjaga koneksi WiFi tetap terhubung
void maintainWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus, mencoba reconnect...");
    WiFi.disconnect(true);
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi tersambung kembali.");
    } else {
      Serial.println("\nGagal reconnect WiFi.");
    }
  }
}

// ✅ Fungsi koneksi Firebase
void connectFirebase() {
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;
  Firebase.begin(&config, &auth);

  // Mendapatkan UID user setelah login berhasil
  Serial.println("Mendapatkan User UID...");
  while (auth.token.uid == "") {
    Serial.print('.');
    delay(1000);
  }
  uid = auth.token.uid.c_str();
  Serial.println("\nUser UID: " + uid);
}

// ✅ Fungsi untuk membuka pintu dengan durasi tertentu
void doorUnlock(unsigned long duration) {
  digitalWrite(LED, HIGH);
  digitalWrite(RELAY, HIGH); // Aktifkan relay (solenoid lock terbuka)
  isDoorUnlocked = true;
  unlockStartTime = millis();
  unlockDuration = duration;
  Serial.println("Pintu Terbuka (non-blocking)");
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);  
  
  pinMode(LED, OUTPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(manual, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  digitalWrite(RELAY, LOW); // Relay OFF saat awal (aktif HIGH)

  connectWiFi();       // Sambungkan WiFi
  digitalWrite(LED, LOW);
  connectFirebase();   // Sambungkan Firebase
}

// ✅ Fungsi membaca knock code dari sensor piezoelectric
void listenToSecretKnock(){
  // Reset pembacaan sebelumnya
  for (int i=0; i < maximumKnocks; i++){
    knockReadings[i] = 0;
  }
  
  int currentKnockNumber = 0; // Jumlah ketukan yang terdeteksi
  int startTime = millis();   // Waktu awal ketukan pertama
  int now = millis();   
 
  do { 
    knockSensorValue = analogRead(knockSensor);
    if (knockSensorValue >= threshold){                  
      now = millis();
      knockReadings[currentKnockNumber] = now - startTime;
      currentKnockNumber ++; // Tambah jumlah ketukan
      startTime = now;  
      
      // LED indikator aktif saat ketukan
      digitalWrite(LED, programModeActive ? LOW : HIGH);
      knockDelay();
      digitalWrite(LED, programModeActive ? HIGH : LOW);
    }
    now = millis();
  } while ((now-startTime < knockComplete) && (currentKnockNumber < maximumKnocks));
  
  // Validasi knock code jika bukan program mode
  if (!programModeActive){         
    if (validateKnock()){
      String kunciPintu = getKunciFirebase("/pintu/kondisi/kunci");
      if (kunciPintu == "TIDAK" && !isDoorUnlocked){
        doorUnlock(lockOperateTime);
      } else {
        Serial.println("Kunci Di Kunci Oleh Aplikasi");
      }
    } else {
      Serial.println("Pintu Tidak Terbuka");
      // Blink LED 4x saat gagal
      for (int i=0; i < 4; i++){
        digitalWrite(LED, HIGH);
        delay(50);
        digitalWrite(LED, LOW);
        delay(50);
      }
    }
  } else {
    validateKnock(); // Program mode: update secret code
  }
}

// ✅ Fungsi validasi knock code dibanding secret code
boolean validateKnock(){
  int currentKnockCount = 0;
  int secretKnockCount = 0;
  int maxKnockInterval = 0;

  // Hitung ketukan dan durasi maksimum
  for (int i=0; i<maximumKnocks; i++){
    if (knockReadings[i] > 0) currentKnockCount++;
    if (secretCode[i] > 0) secretKnockCount++;
    if (knockReadings[i] > maxKnockInterval) maxKnockInterval = knockReadings[i];
  }

  if (programModeActive){
    // Jika program mode aktif, update secret code
    for (int i=0; i < maximumKnocks; i++){
      secretCode[i] = map(knockReadings[i], 0, maxKnockInterval, 0, 100);
    }
    programModeActive = false;
    playbackKnock(maxKnockInterval);
    return false;
  }

  if (currentKnockCount != secretKnockCount){
    return false;
  }

  // Hitung perbedaan waktu setiap ketukan
  int totaltimeDifferences = 0;
  for (int i=0; i < maximumKnocks; i++){
    knockReadings[i] = map(knockReadings[i], 0, maxKnockInterval, 0, 100);
    int timeDiff = abs(knockReadings[i] - secretCode[i]);
    if (timeDiff > rejectValue){
      return false;
    }
    totaltimeDifferences += timeDiff;
  }

  if (totaltimeDifferences / secretKnockCount > averageRejectValue){
    return false; 
  }

  return true; // Knock code valid
}

// ✅ Fungsi memutar ulang pola ketukan via LED
void playbackKnock(int maxKnockInterval){
  digitalWrite(LED, LOW);
  delay(1000);
  digitalWrite(LED, HIGH);
  for (int i = 0; i < maximumKnocks ; i++){
    digitalWrite(LED, LOW);
    if (secretCode[i] > 0){
      delay(map(secretCode[i], 0, 100, 0, maxKnockInterval));
      digitalWrite(LED, HIGH);
    }
  }
  digitalWrite(LED, LOW);
}

// ✅ Delay di antara ketukan
void knockDelay(){
  int itterations = (knockFadeTime / 20);
  for (int i=0; i < itterations; i++){
    delay(10);
    analogRead(knockSensor);
    delay(10);
  } 
}

// ✅ Fungsi mengirim status pintu ke Firebase
void sendDoorStatusToFirebase() {
  FirebaseJson jsonData;

  if (lastState == HIGH) {
    // Jika pintu TERBUKA
    jsonData.set("status", "TERBUKA");
    jsonData.set("timestamp/.sv", "timestamp");
    if (Firebase.RTDB.updateNode(&fbdo, DOOR_STATUS_PATH, &jsonData)) {
      Serial.println("Status TERBUKA + timestamp berhasil dikirim.");
    } else {
      Serial.println("Gagal kirim TERBUKA: " + fbdo.errorReason());
    }
  } else {
    // Jika pintu TERTUTUP
    jsonData.set("status", "TERTUTUP");
    if (Firebase.RTDB.updateNode(&fbdo, DOOR_STATUS_PATH, &jsonData)) {
      Serial.println("Status TERTUTUP berhasil dikirim tanpa menghapus timestamp.");
    } else {
      Serial.println("Gagal kirim TERTUTUP: " + fbdo.errorReason());
    }
  }
}

// ✅ Fungsi mengambil data kunci pintu dari Firebase
String getKunciFirebase(String path) {
  Serial.print("Mengambil kunci dari Firebase, Path: ");
  Serial.println(path);

  if (Firebase.RTDB.getString(&fbdo, path)) {
    String kunci = fbdo.stringData();
    Serial.print("Kunci berhasil diambil: ");
    Serial.println(kunci);
    return kunci;
  } else {
    Serial.println("Gagal mengambil kunci!");
    Serial.println(fbdo.errorReason());
    return "";  // Kembalikan string kosong jika gagal
  }
}

// ✅ Fungsi utama loop program
void loop() {
  unsigned long currentMillis = millis();  // Waktu saat ini

  // 🔷 1. Cek dan perbaiki koneksi WiFi jika putus
  if (millis() - lastWiFiCheck > wifiCheckInterval) {
    lastWiFiCheck = millis();
    maintainWiFiConnection();
  }

  // 🔷 2. Cek push button manual dengan debounce
  static int lastButtonState = HIGH;
  static int currentButtonReading;
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;

  currentButtonReading = digitalRead(manual);
  if (currentButtonReading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (currentButtonReading == LOW && !isDoorUnlocked) {
      doorUnlock(3000);
      Serial.println("Tombol ditekan!");
    }
  }
  lastButtonState = currentButtonReading;

  // 🔷 3. Otomatis kunci kembali jika durasi habis
  if (isDoorUnlocked && millis() - unlockStartTime >= unlockDuration) {
    digitalWrite(RELAY, LOW); // Matikan relay
    digitalWrite(LED, LOW);
    isDoorUnlocked = false;
    Serial.println("Pintu otomatis terkunci kembali.");
  }

  // 🔷 4. Cek status pintu dari sensor magnetic MC-38
  int sensorState = digitalRead(SENSOR_PIN);
  if (sensorState != lastState) {
    lastState = sensorState;
    Serial.println(sensorState == HIGH ? "Pintu TERBUKA!" : "Pintu TERTUTUP.");
    sendDoorStatusToFirebase();
  }

  // 🔷 5. Cek ketukan dari sensor piezoelectric
  knockSensorValue = analogRead(knockSensor);
  if (knockSensorValue >= threshold) {
    Serial.println(knockSensorValue);
    digitalWrite(LED, programModeActive ? LOW : HIGH);
    knockDelay();
    digitalWrite(LED, programModeActive ? HIGH : LOW);
    listenToSecretKnock();
  }
}
