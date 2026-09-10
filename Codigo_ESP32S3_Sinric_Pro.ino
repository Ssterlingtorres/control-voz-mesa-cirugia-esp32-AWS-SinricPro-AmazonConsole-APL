#include <WiFi.h>
#include <SinricPro.h>
#include <SinricProDevice.h>
#include <Capabilities/RangeController.h> 
#include <Capabilities/PowerStateController.h>

#include <I2Cdev.h>
#include <MPU6050.h>
#include <Wire.h>

// -------- WIFI --------
#define WIFI_SSID     "vivo Y33s"
#define WIFI_PASS     "Venom3y4"

// -------- SINRIC CREDENCIALES --------
#define APP_KEY       "adf99e2c-640d-46ab-88e1-002d8e563947"
#define APP_SECRET    "649f7991-aceb-44ff-a7c3-681079b9fc3d-5116ba26-0bb4-4ba6-b5aa-e4c973ee8669"

// -------- DEVICE IDs ACTUALIZADOS --------
#define DEVICE_ID_TREND  "69cb364d17b32c0941e01015"
#define DEVICE_ID_TIL    "69cf3c50dafb005af4e2d875"
#define DEVICE_ID_ALTURA "69cf22f9db0f6e0b15ac29ca"

// -------- DISPOSITIVO CUSTOM --------
class MesaCustom : public SinricProDevice, 
                   public RangeController<MesaCustom>,
                   public PowerStateController<MesaCustom> {
  friend class RangeController<MesaCustom>;
  friend class PowerStateController<MesaCustom>;
public:
  MesaCustom(const String &deviceId) : SinricProDevice(deviceId, "MesaCustom") {}
};

// -------- SENSORES --------
MPU6050 sensor;
int16_t ax, ay, az;

// Pines para el Ultrasónico
#define TRIG_PIN 13 
#define ECHO_PIN 14 

// -------- PINES ACTUADORES --------
int trendSubir = 2;
int trendBajar = 4;

int tilIzq = 18;
int tilDer = 19;

int alturaSubir = 25; 
int alturaBajar = 26; 

// -------- CONTROL DE METAS --------
float objetivoX = 0;
float objetivoY = 0;
float objetivoAltura = 0;

float margenAngulo = 3.0; // Grados
float margenAltura = 2.0; // Centímetros

bool enMovimientoX = false;
bool enMovimientoY = false;
bool enMovimientoAltura = false;

float alturaActual = 0;

unsigned long tiempoUltimoEco = 0;
unsigned long tiempoUltimaImpresion = 0;

// -------- CALLBACKS --------
bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("⚡ Power State %s -> %s\n", deviceId.c_str(), state ? "ON" : "OFF");
  return true; 
}

bool onRangeValue(const String &deviceId, const String& instance, float &value) {

  // -------- TREND --------
  if (deviceId == DEVICE_ID_TREND) {
    objetivoX = value;
    enMovimientoX = true;
    Serial.printf("🗣️ TREND solicitado -> %.1f°\n", value);
    return true;
  }

  // -------- TIL --------
  if (deviceId == DEVICE_ID_TIL) {
    objetivoY = value;
    enMovimientoY = true;
    Serial.printf("🗣️ TIL solicitado -> %.1f°\n", value);
    return true;
  }

  // -------- ALTURA --------
  if (deviceId == DEVICE_ID_ALTURA) {
    objetivoAltura = value;
    enMovimientoAltura = true;
    Serial.printf("🗣️ ALTURA solicitada -> %.1f cm\n", value);
    return true;
  }

  return false;
}

// -------- SETUP WIFI --------
void setupWiFi() {
  Serial.print("Conectando WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ WiFi conectado");
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200); 
  Serial.println("\n--- Iniciando Sistema Quirúrgico ---");

  pinMode(trendSubir, OUTPUT);
  pinMode(trendBajar, OUTPUT);
  pinMode(tilIzq, OUTPUT);
  pinMode(tilDer, OUTPUT);
  pinMode(alturaSubir, OUTPUT);
  pinMode(alturaBajar, OUTPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Wire.begin(21, 22);
  sensor.initialize();

  if (sensor.testConnection()) {
    Serial.println("✅ MPU6050 OK");
  } else {
    Serial.println("❌ MPU6050 ERROR");
  }

  setupWiFi();

  // -------- CREAR DISPOSITIVOS --------
  MesaCustom &trend  = SinricPro[DEVICE_ID_TREND];
  MesaCustom &til    = SinricPro[DEVICE_ID_TIL];
  MesaCustom &altura = SinricPro[DEVICE_ID_ALTURA];

  trend.onRangeValue("angulo", onRangeValue);
  til.onRangeValue("angulo", onRangeValue);
  altura.onRangeValue("altura", onRangeValue); 
  
  trend.onPowerState(onPowerState);
  til.onPowerState(onPowerState);
  altura.onPowerState(onPowerState);

  SinricPro.onConnected([](){ Serial.println("✅ Conectado a Servidores SinricPro/Alexa"); });
  SinricPro.onDisconnected([](){ Serial.println("❌ Desconectado de SinricPro"); });

  SinricPro.begin(APP_KEY, APP_SECRET);
}

// -------- LOOP --------
void loop() {
  SinricPro.handle();

  // -------- LEER ACELERÓMETRO --------
  sensor.getAcceleration(&ax, &ay, &az);
  float angX = atan(ax / sqrt(pow((float)ay, 2) + pow((float)az, 2))) * (180.0 / 3.1416);
  float angY = atan(ay / sqrt(pow((float)ax, 2) + pow((float)az, 2))) * (180.0 / 3.1416);

  // -------- LEER SENSOR ULTRASÓNICO (cada 50ms) --------
  if (millis() - tiempoUltimoEco > 50) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duracion = pulseIn(ECHO_PIN, HIGH, 20000); 
    if (duracion > 0) {
      alturaActual = duracion * 0.034 / 2;
    }
    tiempoUltimoEco = millis();
  }

  // -------- CONTROL TREND --------
  bool enRangoX = (angX >= (objetivoX - margenAngulo) && angX <= (objetivoX + margenAngulo));

  if (enMovimientoX) {
    if (enRangoX) {
      digitalWrite(trendSubir, LOW);
      digitalWrite(trendBajar, LOW);
      enMovimientoX = false;
      Serial.println("✅ Posición Trendelenburg alcanzada");
    } else if (angX < objetivoX) {
      digitalWrite(trendSubir, HIGH);
      digitalWrite(trendBajar, LOW);
    } else {
      digitalWrite(trendSubir, LOW);
      digitalWrite(trendBajar, HIGH);
    }
  } else {
    digitalWrite(trendSubir, LOW);
    digitalWrite(trendBajar, LOW);
  }

  // -------- CONTROL TIL --------
  bool enRangoY = (angY >= (objetivoY - margenAngulo) && angY <= (objetivoY + margenAngulo));

  if (enMovimientoY) {
    if (enRangoY) {
      digitalWrite(tilIzq, LOW);
      digitalWrite(tilDer, LOW);
      enMovimientoY = false;
      Serial.println("✅ Posición Til alcanzada");
    } else if (angY < objetivoY) {
      digitalWrite(tilIzq, HIGH);
      digitalWrite(tilDer, LOW);
    } else {
      digitalWrite(tilIzq, LOW);
      digitalWrite(tilDer, HIGH);
    }
  } else {
    digitalWrite(tilIzq, LOW);
    digitalWrite(tilDer, LOW);
  }

  // -------- CONTROL ALTURA --------
  bool enRangoAltura = (alturaActual >= (objetivoAltura - margenAltura) && alturaActual <= (objetivoAltura + margenAltura));

  if (enMovimientoAltura && alturaActual > 0) { 
    if (enRangoAltura) {
      digitalWrite(alturaSubir, LOW);
      digitalWrite(alturaBajar, LOW);
      enMovimientoAltura = false;
      Serial.println("✅ Posición Altura alcanzada");
    } else if (alturaActual < objetivoAltura) {
      digitalWrite(alturaSubir, HIGH); 
      digitalWrite(alturaBajar, LOW);
    } else {
      digitalWrite(alturaSubir, LOW);
      digitalWrite(alturaBajar, HIGH); 
    }
  } else {
    digitalWrite(alturaSubir, LOW);
    digitalWrite(alturaBajar, LOW);
  }

  // -------- MONITOR SERIAL (Datos en vivo) --------
  if (millis() - tiempoUltimaImpresion > 500) {
    Serial.printf("X: %.1f° (Obj: %.1f°) | Y: %.1f° (Obj: %.1f°) | Altura: %.1f cm (Obj: %.1f cm)\n", 
                  angX, objetivoX, angY, objetivoY, alturaActual, objetivoAltura);
    tiempoUltimaImpresion = millis();
  }

  delay(10);
}