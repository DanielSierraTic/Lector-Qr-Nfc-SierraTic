#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>

// WIFI
const char* ssid = "TAXIMARINO OFISUR";
const char* password = "taximarino020824";

// BASE URL Y ENDPOINTS
const char* BASE_URL = "https://apisuiteprod.sierratic.com/api";
const char* LOGIN_PATH = "/usuarios/login";
const char* ACCESS_PATH = "/Manilla/access-control/";   // NUEVO endpoint

const int IDPUNTO = 28;

// ------------------- QR Serial -------------------
#define RXD2 26  
#define TXD2 25  
HardwareSerial QRSerial(2);

// Credenciales del login
const char* USER = "TORNIQUETE2";
const char* PASSWORD = "123456789";

// Token JWT
String jwtToken = "";

// Relay
#define RELAY_PIN 27 

// ------------------- WIFI -------------------
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi conectado!");
}

// ------------------- LOGIN -------------------
bool loginAPI() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(BASE_URL) + LOGIN_PATH;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"usuario\":\"" + String(USER) + "\",\"clave\":\"" + String(PASSWORD) + "\"}";
    int httpResponseCode = http.POST(payload);

    if (httpResponseCode >= 200 && httpResponseCode < 400) {
      String response = http.getString();
      Serial.println("✅ Login respuesta: " + response);

      StaticJsonDocument<512> doc;
      if (!deserializeJson(doc, response)) {
        jwtToken = doc["token"].as<String>();
        Serial.println("🔐 JWT Token recibido");
        http.end();
        return true;
      }
    } else {
      Serial.println("❌ Error en POST Login: " + String(httpResponseCode));
    }

    http.end();
  }

  return false;
}

bool login() {
  while (!loginAPI()) {
    Serial.println("Reintentando login...");
    delay(1000);
  }
  return true;
}

// ------------------- ACCESO -------------------
void requestAccess(String code) {
  if (WiFi.status() != WL_CONNECTED || jwtToken == "") {
    Serial.println("❌ No hay conexión WiFi o JWT");
    return;
  }

  HTTPClient http;
  String url = String(BASE_URL) + ACCESS_PATH + code + "?idPunto=" + String(IDPUNTO);

  http.begin(url);
  http.addHeader("Accept", "application/json");
  http.addHeader("Authorization", "Bearer " + jwtToken);

  Serial.println("📡 GET: " + url);
  int httpCode = http.GET();

  if (httpCode >= 200 && httpCode < 400) {
    String response = http.getString();
    Serial.println("🔹 Respuesta: " + response);

    bool accesoPermitido = (response == "true" || response == "1");

    if (accesoPermitido) {
      Serial.println("✅ Acceso autorizado: abriendo torniquete");
      relayOn();
      delay(1000);
      relayOff();
    } else {
      Serial.println("❌ Acceso denegado");
    }

  } else {
    Serial.println("❌ Error GET acceso: " + String(httpCode));
  }

  http.end();
}

// ------------------- RELÉ -------------------
void relayOn() {
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("🔓 Relé abierto");
}

void relayOff() {
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("🔒 Relé cerrado");
}

// ------------------- SETUP -------------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀 Iniciando dispositivo...");

  QRSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  QRSerial.setTimeout(10); // ⚡ Reduce timeout de 1000ms a 10ms
  pinMode(RELAY_PIN, OUTPUT);
  relayOff();

  connectWiFi();
  login();
}

// ------------------- LOOP -------------------
void loop() {
  if (QRSerial.available()) {
    String qrCode = QRSerial.readStringUntil('\n');
    qrCode.trim();

    if (qrCode.length() > 0) {
      Serial.println("📷 Código QR: " + qrCode);
      requestAccess(qrCode);
    }
  }
}