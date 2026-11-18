#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// Definisi Pin
#define TRIG_PIN   5
#define ECHO_PIN   18
#define SERVO_PIN  13
#define BUTTON_PIN 33
#define OLED_SDA   21
#define OLED_SCL   22

// Konfigurasi OLED
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Konfigurasi Servo & Wadah
Servo servoPakan;
const float TINGGI_WADAH = 30.0;
const int SUDUT_TUTUP    = 0;
const int SUDUT_BUKA     = 90;

bool statusServo = false;

// Variabel Kecepatan & Posisi Gerak Halus Servo
int posisiSekarang = SUDUT_TUTUP;
int targetSudut    = SUDUT_TUTUP;
unsigned long waktuGerakServo = 0;
const unsigned long KECEPATAN_SERVO = 20; // Jeda per 1 derajat (ms). Semakin besar = semakin pelan

// Variabel Debounce Button
int kondisiButtonTerakhir = HIGH;
int pembacaanSebelumnya  = HIGH;
unsigned long waktuDebounce = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// Timer Pembacaan Sensor
unsigned long waktuSensor = 0;
const unsigned long INTERVAL_SENSOR = 1000;

// Fungsi Membaca Jarak Ultrasonik
float bacaJarak() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long durasi = pulseIn(ECHO_PIN, HIGH, 30000);
  if (durasi == 0) {
    return -1;
  }
  return durasi * 0.0343 / 2;
}

// Fungsi Mengontrol Tombol (Menentukan Target Sudut)
void bacaButton() {
  int pembacaan = digitalRead(BUTTON_PIN);

  if (pembacaan != pembacaanSebelumnya) {
    waktuDebounce = millis();
  }

  if ((millis() - waktuDebounce) > DEBOUNCE_DELAY) {
    if (pembacaan != kondisiButtonTerakhir) {
      kondisiButtonTerakhir = pembacaan;

      // Saat tombol ditekan (HIGH -> LOW)
      if (kondisiButtonTerakhir == LOW) {
        statusServo = !statusServo;

        if (statusServo) {
          targetSudut = SUDUT_BUKA;
          Serial.println("WADAH MEMBUKA...");
        } else {
          targetSudut = SUDUT_TUTUP;
          Serial.println("WADAH MENUTUP...");
        }
      }
    }
  }

  pembacaanSebelumnya = pembacaan;
}

// Fungsi Menggerakkan Servo Secara Perlahan (Non-Blocking)
void gerakkanServoPelan() {
  if (posisiSekarang != targetSudut) {
    if (millis() - waktuGerakServo >= KECEPATAN_SERVO) {
      waktuGerakServo = millis();

      if (posisiSekarang < targetSudut) {
        posisiSekarang++;
      } else {
        posisiSekarang--;
      }

      servoPakan.write(posisiSekarang);
    }
  }
}

// Fungsi Memperbarui Tampilan Layar OLED
void updateTampilanOLED(float jarak) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 3);
  display.println("MONITORING PAKAN");
  display.drawLine(0, 15, 128, 15, SSD1306_WHITE);

  if (jarak < 0) {
    Serial.println("Objek tidak terdeteksi");
    display.setCursor(5, 23);
    display.println("Objek tidak terdeteksi");
  } else {
    float persen = constrain((1.0 - jarak / TINGGI_WADAH) * 100.0, 0, 100);

    Serial.printf("Jarak: %.1f cm | Pakan: %.0f%%\n", jarak, persen);

    display.setCursor(5, 20);
    display.print("Jarak: ");
    display.print(jarak, 1);
    display.println(" cm");

    display.setCursor(5, 32);
    display.print("Pakan: ");
    display.print(persen, 0);
    display.println("%");

    // Progress bar kapasitas pakan
    display.drawRect(5, 55, 118, 8, SSD1306_WHITE);
    display.fillRect(6, 56, (int)(116 * persen / 100), 6, SSD1306_WHITE);
  }

  // Status servo
  display.setCursor(5, 44);
  display.print("Servo: ");
  display.println(statusServo ? "TERBUKA" : "TERTUTUP");

  display.display();
}

void setup() {
  Serial.begin(115200);

  // Inisialisasi Pin
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Inisialisasi OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED tidak ditemukan!");
    while (true);
  }

  // Inisialisasi Servo & Timer PWM ESP32
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servoPakan.setPeriodHertz(50);
  servoPakan.attach(SERVO_PIN, 500, 2400);
  servoPakan.write(SUDUT_TUTUP);

  // Tampilan Awal OLED
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(16, 25);
  display.println("MONITORING PAKAN");
  display.display();

  Serial.println("Sistem Monitoring Pakan Aktif");
  Serial.println("Tekan button untuk membuka/menutup wadah");
  delay(1500);
}

void loop() {
  bacaButton();
  gerakkanServoPelan();

  if (millis() - waktuSensor >= INTERVAL_SENSOR) {
    waktuSensor = millis();
    float jarak = bacaJarak();
    updateTampilanOLED(jarak);
  }
}