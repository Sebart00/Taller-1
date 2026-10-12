  #include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
 
// ---------- Configuración WiFi (cambiar) ----------
const char* WIFI_SSID     = "S24Seb";
const char* WIFI_PASSWORD = "00000000";
 
// ---------- Configuración MQTT ----------
const char* MQTT_SERVER = "10.68.137.120";   // IPv4 de tu computadora (ipconfig)
const int   MQTT_PORT   = 1883;
 
const char* TOPIC_TEMPERATURA = "clima/esp32/temperatura";
const char* TOPIC_HUMEDAD     = "clima/esp32/humedad";
const char* TOPIC_LUZ         = "clima/esp32/luz";
 
const unsigned long INTERVALO_ENVIO = 2000;  // ms
unsigned long ultimoEnvio = 0;
 
// ---------- Configuración de sensores (Estudiante) ----------
#define DHT_PIN   4
#define DHT_TYPE  DHT11
#define LDR_PIN   32
 
// Si al TAPAR la LDR el porcentaje SUBE, tu divisor está al revés: cambia a true
const bool INVERTIR_LUZ = false;
 
DHT dht(DHT_PIN, DHT_TYPE);
 
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
 
void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("Conectando a WiFi ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi conectado. IP ESP32: ");
  Serial.println(WiFi.localIP());
}
 
void conectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a MQTT ");
    Serial.print(MQTT_SERVER);
    Serial.print(":");
    Serial.println(MQTT_PORT);
    String clientId = "esp32-clima-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("MQTT conectado");
    } else {
      Serial.print("Fallo, estado=");
      Serial.print(mqttClient.state());
      Serial.println(". Reintentando en 2 s");
      delay(2000);
    }
  }
}
 
// ---------- Lectura y publicación (Estudiante) ----------
void publicarSensores() {
  float temperatura = dht.readTemperature();   // °C
  float humedad     = dht.readHumidity();      // %
 
  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("Error leyendo el DHT11 (revisa cableado/pin)");
    return;
  }
 
  // ADC del ESP32: 12 bits -> 0..4095. Convertimos a 0..100 %
  int lecturaLDR = analogRead(LDR_PIN);
  int luz = map(lecturaLDR, 0, 4095, 0, 100);
  if (INVERTIR_LUZ) luz = 100 - luz;
  luz = constrain(luz, 0, 100);
 
  char bufTemp[8], bufHum[8], bufLuz[8];
  dtostrf(temperatura, 1, 1, bufTemp);
  dtostrf(humedad, 1, 0, bufHum);
  snprintf(bufLuz, sizeof(bufLuz), "%d", luz);
 
  // retained = true: el dashboard recibe el último valor al recargar
  mqttClient.publish(TOPIC_TEMPERATURA, bufTemp, true);
  mqttClient.publish(TOPIC_HUMEDAD, bufHum, true);
  mqttClient.publish(TOPIC_LUZ, bufLuz, true);
 
  Serial.print("Temp: ");      Serial.print(bufTemp);
  Serial.print(" °C | Hum: "); Serial.print(bufHum);
  Serial.print(" % | Luz: ");  Serial.print(bufLuz);
  Serial.print(" % (ADC=");    Serial.print(lecturaLDR);
  Serial.println(")");
}
 
void setup() {
  Serial.begin(115200);
  delay(500);
 
  // ---------- Inicialización (Estudiante) ----------
  dht.begin();
  analogReadResolution(12);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);  // rango completo 0..3.3V
 
  conectarWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}
 
void loop() {
  if (WiFi.status() != WL_CONNECTED) conectarWiFi();
  if (!mqttClient.connected()) conectarMQTT();
  mqttClient.loop();
 
  unsigned long ahora = millis();
  if (ahora - ultimoEnvio >= INTERVALO_ENVIO) {
    ultimoEnvio = ahora;
    publicarSensores();
  }
}