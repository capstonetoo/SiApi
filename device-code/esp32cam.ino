#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "esp_camera.h"   
#include <base64.h>

// ======================= Konfigurasi Kamera (Sesuaikan dengan modul kamera Anda) ========================

#define CAMERA_MODEL_AI_THINKER

#if defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

#define BPS_LED_GPIO_NUM 4
#endif

WiFiServer server(80);
Preferences preferences;

String header;
String ssid = "";
String password = "";
String name = "";
String email = "";
String note = "";
String lat = "";
String longi = "";
String endpoint = "";
String uniqueCode = "AL-119"; 
bool hasStoredCreds = false;
String storedIP = "";

const char *setupSSID = "Setup Alat";
const char *setupPassword = ""; 

float lastSmoke = NAN;
float lastTemp = NAN;
float lastHum = NAN;
float lastFlame = NAN;
bool isThereFlame = false;
String picture = "";


#define RXD1 3 // UART RX pin
#define TXD1 1 // UART TX pin

// Tambahkan variabel global untuk melacak mode saat ini
enum WiFiModeState {
  MODE_STA,
  MODE_AP
};
WiFiModeState currentWiFiMode;

// Variabel untuk mengontrol frekuensi fetch request
unsigned long lastFetchTime = 0;
const long fetchInterval = 60 * 1000; // 1 menit dalam milidetik

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; 
  config.pixel_format = PIXFORMAT_JPEG; // Menggunakan JPEG untuk kompresi

  config.frame_size = FRAMESIZE_HVGA; // Jika tanpa PSRAM, pakai resolusi rendah dan 1 buffer
  config.jpeg_quality = 10;
  config.fb_count = 2;
  // }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  
  sensor_t *s = esp_camera_sensor_get();
  if (s == NULL) {
    Serial.println("Sensor kamera tidak ditemukan!");
    while (1); // Hentikan program jika sensor tidak ditemukan
  }

  // Mengatur vflip
  s->set_vflip(s, 1); // 1 = aktifkan vflip, 0 = nonaktifkan

  Serial.println("Kamera siap!");
  return true;
}

// Fungsi untuk mengonversi byte array ke Base64 String
String encodeBase64(const uint8_t *data, size_t len) {
    // Ubah ke String agar bisa diproses oleh base64::encode(String)
    String rawStr = "";
    for (size_t i = 0; i < len; i++) {
        rawStr += (char)data[i];
    }

    return base64::encode(rawStr);
}


String inputDecode(String input) {
  String decoded = "";
  char temp[] = "0x00";  // Temp untuk heksadesimal

  for (unsigned int i = 0; i < input.length(); i++) {
    if (input[i] == '%') {
      if (i + 2 < input.length()) {
        temp[2] = input[i + 1];
        temp[3] = input[i + 2];
        decoded += (char) strtol(temp, NULL, 16); // Ubah heksadesimal jadi char
        i += 2;
      }
    } else if (input[i] == '+') {
      decoded += ' ';
    } else {
      decoded += input[i];
    }
  }

  return decoded;
}


// ============================ FUNGSI WIFI ============================
void loadWiFiCredentials() {
  preferences.begin("wifi-creds", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  name = preferences.getString("name", "");
  email = preferences.getString("email", "");
  note = preferences.getString("note", "");
  lat = preferences.getString("lat", "");    // Memuat latitude
  longi = preferences.getString("longi", ""); // Memuat longitude
  endpoint = preferences.getString("endpoint", ""); 
  preferences.end();
  hasStoredCreds = ssid.length() > 0;
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  Serial.print("Menghubungkan ke ");
  Serial.print(ssid);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  Serial.println(); // Newline after dots

  if (WiFi.status() == WL_CONNECTED) {
    storedIP = WiFi.localIP().toString();
    server.begin();
    currentWiFiMode = MODE_STA; // Set mode saat berhasil terhubung
    Serial.println("Terhubung ke WiFi!");
    Serial.println("IP address: " + storedIP);
  } else {
    storedIP = "Gagal terhubung!";
    Serial.println("Gagal terhubung ke WiFi."); // Tambah log
    WiFi.disconnect(true); // Pastikan putuskan koneksi STA sebelumnya
    startSetupAP(); // Beralih ke mode AP
  }
}

void startSetupAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(setupSSID, setupPassword);
  storedIP = WiFi.softAPIP().toString();
  server.begin(); // Mulai server di AP juga
  currentWiFiMode = MODE_AP; // Set mode saat di AP
  Serial.println("Memulai Access Point."); // Tambah log
  Serial.println("AP IP address: " + storedIP); // Tambah log
}

void setup() {
  Serial.begin(9600, SERIAL_8N1, RXD1, TXD1); // Inisialisasi Serial2 untuk komunikasi UART

  delay(2500);
  loadWiFiCredentials();

  if (hasStoredCreds) {
    connectToWiFi();
  } else {
    startSetupAP();
  }

  // Inisialisasi kamera
  if (!initCamera()) {
    Serial.println("Gagal inisialisasi kamera! Restarting...");
    delay(5000);
    ESP.restart();
  }

  pinMode(BPS_LED_GPIO_NUM, OUTPUT); // Inisialisasi pin LED Flash
  digitalWrite(BPS_LED_GPIO_NUM, LOW); // Matikan LED Flash pada awalnya
}

void loop() {

  if (currentWiFiMode == MODE_STA && WiFi.status() != WL_CONNECTED) {
    Serial.println("Mendeteksi koneksi STA terputus, mencoba lagi...");
    connectToWiFi(); // Coba hubungkan kembali ke STA
  } else if (currentWiFiMode == MODE_AP && hasStoredCreds) {
      // Jika saat ini di mode AP, tetapi ada kredensial tersimpan, coba lagi koneksi STA
      // Ini akan membuat perangkat mencoba kembali ke STA setelah beberapa waktu
      static unsigned long lastAttemptTime = 0;
      const unsigned long reconnectInterval = 30000; // Coba setiap 30 detik
      if (millis() - lastAttemptTime >= reconnectInterval) {
          Serial.println("Mencoba kembali ke jaringan tersimpan dari mode AP...");
          connectToWiFi();
          lastAttemptTime = millis();
      }
  }

  // Cetak IP hanya jika berubah (untuk mengurangi spam)
  delay(100);
  Serial.println("IP: " + storedIP + "#");
  delay(100);
  
  // Penanganan klien HTTP (Web Server)
  WiFiClient client = server.available();
  if (client) {
    unsigned long currentTime = millis();
    unsigned long previousTime = currentTime;
    String currentLine = "";

    while (client.connected() && currentTime - previousTime <= 2000) {
      currentTime = millis();
      if (client.available()) {
        char c = client.read();
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            if (header.indexOf("GET /?ssid=") >= 0) {
              int ssidStart = header.indexOf("ssid=") + 5;
              int ssidEnd = header.indexOf("&", ssidStart);
              int passStart = header.indexOf("password=") + 9;
              int passEnd = header.indexOf("&", passStart);
              int nameStart = header.indexOf("name=") + 5;
              int nameEnd = header.indexOf("&", nameStart);
              int emailStart = header.indexOf("email=") + 6;
              int emailEnd = header.indexOf("&", emailStart);
              int latStart = header.indexOf("lat=") + 4;
              int latEnd = header.indexOf("&", latStart);
              int longiStart = header.indexOf("longi=") + 6;
              int longiEnd = header.indexOf("&", longiStart);
              int endpointStart = header.indexOf("endpoint=") + 9;
              int endpointEnd = header.indexOf("&", endpointStart);
              int noteStart = header.indexOf("note=") + 5;
              int noteEnd = header.indexOf(" ", noteStart);
              
              String newSSID = header.substring(ssidStart, ssidEnd);
              String newPASS = header.substring(passStart, passEnd);
              String newNAME = header.substring(nameStart, nameEnd);
              String newEMAIL = header.substring(emailStart, emailEnd);
              String newNOTE = header.substring(noteStart, noteEnd);
              String newLAT = header.substring(latStart, latEnd);
              String newLONGI = header.substring(longiStart, longiEnd);
              String newENDPOINT = header.substring(endpointStart, endpointEnd);

              newSSID.replace("+", " ");
              newPASS.replace("+", " ");
              newNAME.replace("+", " ");
              newEMAIL.replace("+", " ");
              newNOTE.replace("+", " ");
              newLAT.replace("+", " ");
              newLONGI.replace("+", " ");
              newENDPOINT.replace("+", " ");

              preferences.begin("wifi-creds", false);
              preferences.putString("ssid", newSSID);
              preferences.putString("password", newPASS);
              preferences.putString("name", newNAME);
              preferences.putString("email", inputDecode(newEMAIL));
              preferences.putString("note", newNOTE);
              preferences.putString("lat", newLAT);
              preferences.putString("longi", newLONGI);
              preferences.putString("endpoint", inputDecode(newENDPOINT));
              preferences.end();

              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html\n");
              client.println("<html>");
              client.println("<head><style>h2{text-align:center; margin-top: 75px; } body { height: 100vh; padding: 0 16px 0 16px;}</style></head>"); // Ubah 1vr ke 100vh
              client.println("<body><h2>Setup disimpan.</h2></body>");
              client.println("</html>");
              delay(2000);
              ESP.restart();
              return;
            }

            // Tampilkan halaman form konfigurasi
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html\n");
            client.println("<html>");
            client.println("<head>");
            client.println("<title>Setup Alat</title>");
            client.println("<style>");
            client.println("h2{text-align:center; margin-top: 75px; margin-bottom: 16px; }");
            client.println(".desc { display: flex; flex-direction: column; max-width: 750px; margin: auto; margin-top: 16px; margin-bottom: 16px; text-align: center;}");
            client.println("form { display: flex; flex-direction: column; max-width: 1000px; margin: auto; gap: 16px; }");
            client.println(".btn-submit { background-color: tomato; color: white; font-weight: bold; border: none; }");
            client.println(".btn-submit:hover { background-color: tomato; opacity: 0.85; }");
            client.println(".btn-submit, input { padding: 16px; border-radius: 8px; }");
            client.println("input { width: 100%; }");
            client.println("body { min-height: 100vh; font-family: sans-serif; padding: 0 16px 75px 16px;}"); // Menambahkan font-family
            client.println("</style>");
            client.println("</head>");
            client.println("<body>");
            
            client.println("<h2>Setup Alat (" + uniqueCode + ") </h2>");
            client.println("<div class='desc'>Demi menjaga kerahasiaan data Anda, aplikasi ini dirancang untuk menyimpan seluruh informasi yang Anda masukkan ke dalam formulir ini secara eksklusif pada penyimpanan lokal perangkat Anda. Data tidak akan dikirimkan ke server eksternal.</div>");
            
            client.println("<form action='/' method='get'>");
            client.println("<div><p>SSID: </p><input type='text' name='ssid' placeholder='Masukkan SSID' value='" + ssid +"'></div>");
            client.println("<div><p>Password: </p><input type='password' name='password' placeholder='Masukkan Password' value='" + password + "'></div>");
            client.println("<div><p>Nama Pengguna: </p><input type='text' name='name' placeholder='Masukkan Nama Pengguna' value='" + name + "'></div>");
            client.println("<div><p>Email Pengguna: </p><input type='email' name='email' placeholder='Masukkan Email Pengguna' value='" + email + "'></div>");
            client.println("<div><p>Latitude: </p><input type='text' name='lat' placeholder='Masukkan Latitude' value='" + lat +"'></div>");
            client.println("<div><p>Longitude: </p><input type='text' name='longi' placeholder='Masukkan Longitude' value='" + longi +"'></div>");
            client.println("<div><p>Endpoint: </p><input type='text' name='endpoint' placeholder='Masukkan Endpoint' value='" + endpoint + "'></div>");
            client.println("<div><p>Catatan: </p><input type='text' name='note' placeholder='Masukkan Catatan' value='" + note + "'></div>");
            client.println("<input type='submit' value='Simpan' class='btn-submit'>");
            client.println("</form>");
            client.println("</body>");
            client.println("</html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
  }

  // Ambil data dari UART
  if (Serial.available()) {
    String jsonData = Serial.readStringUntil('\n');
    
    // Create a JSON document to parse the data
    StaticJsonDocument<200> doc; // Sesuaikan ukuran jika payload UART lebih besar
    
    // Parse the JSON data
    DeserializationError error = deserializeJson(doc, jsonData);
    
    // Check for parsing errors
    if (!error) {
      lastTemp = doc["temp"];
      lastHum = doc["hum"];
      lastSmoke = doc["smoke"];
      lastFlame = doc["flame"]; 
      isThereFlame = doc["is_there_flame"]; 

      if (isThereFlame) {
        if (WiFi.status() == WL_CONNECTED && millis() - lastFetchTime >= fetchInterval) {
            digitalWrite(BPS_LED_GPIO_NUM, HIGH);
            delay(200); 

            camera_fb_t *fb = nullptr;

            for (int i = 0; i < 2; i++) {
              fb = esp_camera_fb_get();
              if (fb) esp_camera_fb_return(fb);
              delay(100); 
            }

            fb = esp_camera_fb_get();
            digitalWrite(BPS_LED_GPIO_NUM, LOW); 


            String encodedImage = "";
            if (fb) {
                encodedImage = encodeBase64(fb->buf, fb->len);
                esp_camera_fb_return(fb); 
            }

            HTTPClient http;
            String serverUrl = endpoint; 
            
            http.begin(serverUrl);
            http.addHeader("Content-Type", "application/json");

            DynamicJsonDocument jsonDoc(4096); 
            jsonDoc["temp"] = isnan(lastTemp) ? 0.0 : lastTemp;
            jsonDoc["hum"] = isnan(lastHum) ? 0.0 : lastHum;
            jsonDoc["smoke"] = isnan(lastSmoke) ? 0.0 : lastSmoke;
            jsonDoc["is_there_flame"] = isThereFlame;
            jsonDoc["img"] = encodedImage; 
            jsonDoc["name"] = name;
            jsonDoc["email"] = email;
            jsonDoc["note"] = note;
            jsonDoc["unique_code"] = uniqueCode;
            jsonDoc["lat"] = lat;
            jsonDoc["long"] = longi;
            picture = encodedImage;

            String payload;
            serializeJson(jsonDoc, payload); 

            http.POST(payload);

            http.end(); 

            lastFetchTime = millis(); 
        }
      }
    }

    Serial.read();
  }
  
  // Small delay to prevent overwhelming the Serial buffer
  delay(500);
}