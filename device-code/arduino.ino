#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

#define DHTPIN 2
#define DHTTYPE DHT22
#define MQ2PIN A0
#define FLAMEPIN A1
const int BUZZPIN = 7;
const int LEDPIN = 8;

// Set the LCD address to 0x27 or 0x3F for a 16x2 LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change 0x27 to 0x3F if your LCD doesn't work
SoftwareSerial espSerial(10, 11); // rx tx

DHT dht(DHTPIN, DHTTYPE);

String stringIP = "";

void buzzWiFi() {
  digitalWrite(BUZZPIN, HIGH);
  digitalWrite(LEDPIN, HIGH);

  delay(500);

  digitalWrite(LEDPIN, LOW);
  digitalWrite(BUZZPIN, LOW);
  delay(500);
}

void buzzFlame() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZPIN, HIGH);
    digitalWrite(LEDPIN, HIGH);

    delay(500);
  
    digitalWrite(LEDPIN, LOW);
    digitalWrite(BUZZPIN, LOW);

    delay(500);
  }
}

void setup() {
  Serial.begin(9600); // Serial Monitor (USB)
  espSerial.begin(9600); // Serial Monitor (USB)
  
  pinMode(LEDPIN, OUTPUT);
  pinMode(BUZZPIN, OUTPUT);

  // Initial LED and Buzzer indication

  dht.begin();

  // Initialize the LCD (only once!)
  lcd.init();
  lcd.backlight(); // Turn on the backlight

  float temperature = dht.readTemperature();
  int smokeValue = analogRead(MQ2PIN);
  int flameValue = analogRead(FLAMEPIN);

  if (temperature && smokeValue && flameValue) { // Sesuaikan ambang batas ini setelah kalibrasi
    digitalWrite(LEDPIN, HIGH);
    digitalWrite(BUZZPIN, HIGH);
    delay(250);
    digitalWrite(BUZZPIN, LOW);
  }

  delay(500); // Beri waktu user membaca pesan awal
}

void loop() {
  digitalWrite(LEDPIN, HIGH);

  if (espSerial.available() && stringIP == "") {
    String incomingIP = espSerial.readStringUntil('#');
    incomingIP.trim(); // bersihkan newline/spasi

    if (incomingIP.startsWith("IP: ")) {
      stringIP = incomingIP;
      Serial.println(stringIP);
      buzzWiFi();
    }

    lcd.setCursor(0,0);
    lcd.print(stringIP.substring(4));
  }

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int smokeValue = analogRead(MQ2PIN);
  int flameValue = analogRead(FLAMEPIN);
  bool isThereFlame = false;

  // LOGIKA DETEKSI API YANG LEBIH BAIK
  // Asumsi: Api terdeteksi jika flameValue rendah ATAU smokeValue tinggi
  if (temperature > 45 || smokeValue > 400 || flameValue < 500) { // Sesuaikan ambang batas ini setelah kalibrasi
    isThereFlame = true;
  }

  // Check if DHT reading failed
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    delay(500);
  }

  // Format data for Serial Monitor
  String data = "{ \"temp\":" + String(isnan(temperature) ? 0.00 : temperature) +
                  ", \"hum\":" + String(isnan(humidity) ? 0.00 : humidity) +
                  ", \"smoke\":" + String(isnan(smokeValue) ? 0.00 : smokeValue) +
                  ", \"is_there_flame\":" + String(isThereFlame) +
                  " }";                   

  Serial.println(isThereFlame);

  if (isThereFlame) {
    espSerial.println(data);
    Serial.println(data);
    buzzFlame();
  }

  delay(500);
}