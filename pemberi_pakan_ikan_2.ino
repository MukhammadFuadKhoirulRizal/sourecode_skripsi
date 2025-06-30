#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Servo.h>
#include <ESP8266WebServer.h>
#include <Firebase_ESP_Client.h>
#include <addons/RTDBHelper.h>

// WiFi
const char* ssid = "Anti polisi";
const char* password = "akulaporkan";
//const char* ssid = "Anti polisi";
//const char* password = "akulaporkan";
// Firebase
#define DATABASE_SECRET "eaiIEBWwG73fALrYC31fw3OLlcVUA5rJ8LDpi6MH"
#define DATABASE_URL "https://test1-947d3-default-rtdb.firebaseio.com/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Waktu
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000);

// Web server
ESP8266WebServer server(80);

// Pin mapping
#define SERVO_PIN   14
#define RADAR_PIN   12
#define BUZZER_PIN  13

Servo servo;

#define FEED_DURATION 60000//pengaturan 20 detik (20*1000 ms)

bool feeding = false;
bool pendeteksi = false;
bool sudahTuangPakan = false;
unsigned long feedStartTime = 0;

#define SERVO_NEUTRAL 0
#define SERVO_FEED  150 //

void setup() {
  Serial.begin(115200);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Menyambung WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung");
  Serial.println(WiFi.localIP());

  // NTP
  timeClient.begin();

  // Servo dan pin
  servo.attach(SERVO_PIN);
  servo.write(SERVO_NEUTRAL);
  pinMode(RADAR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Web server
  server.on("/", []() {
    server.send(200, "text/plain", feeding ? "Feeding" : (pendeteksi ? "Burung terdeteksi" : "Idle"));
  });
  server.begin();
  Serial.println("Server siap...");

  // Firebase

  config.signer.tokens.legacy_token = DATABASE_SECRET;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  timeClient.update();
  server.handleClient();

  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();
  int second = timeClient.getSeconds();

  static int lastSecond = -1;
  if (second != lastSecond) {
    Serial.printf("Jam: %02d:%02d:%02d\n", hour, minute, second);
    lastSecond = second;
  }

  // Otomatis tuang pakan
  if ((hour == 6 || hour == 19) && minute == 10 && second > 0 && second <11 && !feeding && !sudahTuangPakan) {
    Serial.println("[PAKAN] Mulai");// on jam 6  dan 16
    
    feeding = true;
     Firebase.RTDB.setInt(&fbdo, "/feeding", feeding);   
    feedStartTime = millis();
    servo.write(SERVO_FEED);
    sudahTuangPakan = true;
  }

  if (feeding && millis() - feedStartTime >= FEED_DURATION) {
    servo.write(SERVO_NEUTRAL);
    feeding = false;
    Firebase.RTDB.setInt(&fbdo, "/feeding", feeding);
    Serial.println("[PAKAN] Stop");
  }

  if ((hour != 6 && hour != 16) || minute != 00) {
    sudahTuangPakan = false;
  }

  // Deteksi radar
  if (digitalRead(RADAR_PIN) == HIGH){
    Serial.println("[RADAR] Burung terdeteksi");
    pendeteksi = true;
    Firebase.RTDB.setInt(&fbdo, "/pendeteksi", pendeteksi);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    delay(500);
    }else if(digitalRead(RADAR_PIN) == LOW){
    pendeteksi = false;
     Firebase.RTDB.setInt(&fbdo, "/pendeteksi", pendeteksi);
  }

  // Kirim hanya status 'feeding' dan 'pendeteksi' ke Firebase
  if (Firebase.ready()) {
    if (!Firebase.RTDB.setBool(&fbdo, "/feeding", feeding)) {
      Serial.println("Gagal update 'feeding': " + fbdo.errorReason());
    }

    if (!Firebase.RTDB.setBool(&fbdo, "/pendeteksi", pendeteksi)) {
      Serial.println("Gagal update 'pendeteksi': " + fbdo.errorReason());
    }
  }
}
