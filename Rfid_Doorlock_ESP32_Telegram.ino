#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TimeLib.h> // Include Time library
#include <NTPClient.h>
#include <WiFiUdp.h>

// Definisi pin
#define SS_PIN 5        // Pin SDA pada RFID
#define RST_PIN 22      // Pin RST pada RFID
#define SERVO_PIN 13    // Pin untuk servo
#define SCREEN_WIDTH 128 // Lebar OLED
#define SCREEN_HEIGHT 64 // Tinggi OLED

// Definisikan alamat I2C untuk OLED
#define OLED_RESET -1   // Tidak ada pin reset pada I2C OLED
#define SCREEN_ADDRESS 0x3C  // Alamat I2C standar untuk SSD1306

// Pin I2C alternatif
#define SDA_PIN 32  // Pin SDA yang baru untuk OLED
#define SCL_PIN 33  // Pin SCL yang baru untuk OLED

// Ganti dengan kredensial Wi-Fi Anda
const char* ssid = "Rukkhadevata";        // Ganti dengan SSID Wi-Fi Anda
const char* password = "AnakMama"; // Ganti dengan password Wi-Fi Anda

// Ganti dengan token bot dan chat ID Anda
const char* telegramToken = "7316478584:AAFDo6n4t4DuIGIv0OkzEBS-3FRrMcnhpEg"; // Ganti dengan token bot Anda
const char* chatID = "1466178352";             // Chat ID Anda

// Inisialisasi instance RFID, Servo, dan OLED
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo myServo;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// NTP Client setup
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 25200, 60000); // GMT+7

// UID yang diotorisasi (ganti dengan UID kartu Anda)
byte authorizedUID[4] = {0xB3, 0x7B, 0xB2, 0x4F};  // Contoh UID

// Variabel untuk melacak status pintu
bool isDoorOpen = false;  // Mulai dengan pintu dalam keadaan tertutup

void setup() {
  // Inisialisasi komunikasi serial
  Serial.begin(115200);

  // Koneksi Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Menyambung ke WiFi...");
  }
  Serial.println("Terhubung ke WiFi");

  // Start NTP Client
  timeClient.begin();

  // Inisialisasi SPI dan RFID
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Tempatkan kartu RFID...");

  // Inisialisasi servo
  myServo.attach(SERVO_PIN);
  myServo.write(0);  // Posisi servo awal (tertutup)

  // Inisialisasi Wire dengan pin I2C alternatif
  Wire.begin(SDA_PIN, SCL_PIN);

  // Inisialisasi layar OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED tidak ditemukan!");
    while (true);  // Hentikan jika OLED tidak terhubung
  }

  // Membersihkan buffer layar
  display.clearDisplay();
  display.setTextSize(1);      // Ukuran teks 1
  display.setTextColor(WHITE); // Warna teks putih
  display.setCursor(0, 0);
  display.print("Tempatkan kartu RFID...");
  display.display(); // Menampilkan teks di OLED
}

void loop() {
  timeClient.update(); // Update the time

  // Memeriksa apakah ada kartu yang terdeteksi
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Memeriksa apakah kartu dapat dibaca
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Membaca UID kartu
  byte readUID[4];
  for (byte i = 0; i < 4; i++) {
    readUID[i] = mfrc522.uid.uidByte[i];
  }

  // Memeriksa apakah UID yang terbaca sesuai dengan yang diotorisasi
  if (isAuthorized(readUID)) {
    String currentTime = timeClient.getFormattedTime(); // Get the current time
    if (isDoorOpen) {
      // Jika pintu terbuka, tutup pintu
      Serial.println("Kartu terotorisasi, menutup servo...");
      displayMessage("Kartu di", "otorisasi menutup servo");
      sendTelegramMessage("Pintu ditutup at " + currentTime);
      moveServoSmoothly(0);  // Menutup servo secara halus
      isDoorOpen = false;  // Perbarui status pintu
    } else {
      // Jika pintu tertutup, buka pintu
      Serial.println("Kartu terotorisasi, membuka servo...");
      displayMessage("Kartu di", "otorisasi membuka servo");
      sendTelegramMessage("Pintu dibuka at " + currentTime);
      moveServoSmoothly(90);  // Membuka servo secara halus
      isDoorOpen = true;  // Perbarui status pintu
    }
  } else {
    Serial.println("Kartu tidak terotorisasi!");
    displayMessage("Kartu tidak", "terotorisasi!");
    delay(2000); // Tampilkan pesan selama 2 detik
  }

  // Menghentikan pemindaian lebih lanjut sampai kartu dihapus
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

// Fungsi untuk memeriksa apakah UID sesuai dengan yang diotorisasi
bool isAuthorized(byte readUID[]) {
  for (byte i = 0; i < 4; i++) {
    if (readUID[i] != authorizedUID[i]) {
      return false;  // Jika satu byte tidak cocok, kembalikan false
    }
  }
  return true;  // Semua byte cocok, kembalikan true
}

// Fungsi untuk menampilkan pesan di OLED
void displayMessage(String line1, String line2) {
  display.clearDisplay();  // Membersihkan layar
  display.setCursor(0, 0);
  display.print(line1);  // Menampilkan pesan baris pertama
  display.setCursor(0, 10);
  display.print(line2);  // Menampilkan pesan baris kedua
  display.display();     // Menampilkan di layar
}

// Fungsi untuk menggerakkan servo dengan halus
void moveServoSmoothly(int targetAngle) {
  int currentAngle = myServo.read(); // Baca sudut saat ini
  int step = (targetAngle > currentAngle) ? 1 : -1; // Tentukan arah pergerakan

  for (int angle = currentAngle; angle != targetAngle; angle += step) {
    myServo.write(angle); // Set sudut servo
    delay(15); // Delay untuk memberikan waktu pada servo bergerak
  }
}

// Fungsi untuk mengirim pesan ke Telegram
void sendTelegramMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(telegramToken) + "/sendMessage?chat_id=" + String(chatID) + "&text=" + message;
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.println("Pesan terkirim ke Telegram");
    } else {
      Serial.println("Gagal mengirim pesan ke Telegram");
    }
    http.end();
  } else {
    Serial.println("WiFi tidak terhubung");
  }
}
