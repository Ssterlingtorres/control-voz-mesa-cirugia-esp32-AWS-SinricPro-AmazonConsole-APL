#include <WiFi.h>
#include <SinricPro.h>
#include <SinricProDevice.h>
#include <Capabilities/RangeController.h> 

#include <I2Cdev.h>
#include <MPU6050.h>
#include <Wire.h>

// -------- WIFI --------
#define WIFI_SSID     "vivo Y33s"
#define WIFI_PASS     "Venom3y4"

// -------- SINRIC CREDENCIALES NUEVAS --------
#define APP_KEY       "adf99e2c-640d-46ab-88e1-002d8e563947"
#define APP_SECRET    "649f7991-aceb-44ff-a7c3-681079b9fc3d-5116ba26-0bb4-4ba6-b5aa-e4c973ee8669"
#define DEVICE_ID     "69cb364d17b32c0941e01015" // ID de "La mesa"

// -------- CREAR LA ESTRUCTURA DEL DISPOSITIVO PERSONALIZADO --------
class MesaCustom : public SinricProDevice, public RangeController<MesaCustom> {
  friend class RangeController<MesaCustom>;
public:
  MesaCustom(const String &deviceId) : SinricProDevice(deviceId, "MesaCustom") {}
};

// -------- SENSOR --------
MPU6050 sensor;
int16_t ax, ay, az;

// -------- LED --------
int ledX = 2;

// -------- CONTROL --------
float angulo_objetivo = 0;
float margen = 3.0; 
unsigned long tiempoUltimaImpresion = 0;

// -------- CALLBACK ALEXA (CUSTOM DEVICE) --------
bool onRangeValue(const String &deviceId, const String& instance, float &rangeValue) {
  
  if (instance == "angulo") {
    angulo_objetivo = rangeValue;

    Serial.println("\n=========================================");
    // Ahora imprimirá correctamente los valores negativos (ej: -90.0)
    Serial.printf("🗣️ ALEXA: Orden recibida -> %.1f grados\n", angulo_objetivo);
    Serial.println("=========================================\n");
    
    return true; 
  }
  return false;
}

// -------- SETUP WIFI --------
void setupWiFi() {
  Serial.print("Conectando a WiFi");
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
  pinMode(ledX, OUTPUT);

  // Inicializar I2C y sensor
  Wire.begin(21, 22);
  sensor.initialize();

  if (sensor.testConnection()) {
    Serial.println("✅ MPU6050 conectado correctamente");
  } else {
    Serial.println("❌ Error al conectar MPU6050");
  }

  // WiFi
  setupWiFi();

  // INICIALIZAR EL DISPOSITIVO PERSONALIZADO
  MesaCustom &miMesa = SinricPro[DEVICE_ID];
  miMesa.onRangeValue("angulo", onRangeValue);

  SinricPro.onConnected([](){ Serial.println("✅ Conectado a Alexa"); });
  SinricPro.onDisconnected([](){ Serial.println("❌ Desconectado de Alexa"); });

  SinricPro.begin(APP_KEY, APP_SECRET);
}

// -------- LOOP --------
void loop() {
  SinricPro.handle();

  // Leer sensor
  sensor.getAcceleration(&ax, &ay, &az);

  // Cálculo del ángulo físico
  float accel_ang_x = atan(ax / sqrt(pow(ay,2) + pow(az,2))) * (180.0 / 3.1416);

  // Comparación dinámica (funciona perfecto con negativos)
  if (accel_ang_x >= (angulo_objetivo - margen) && accel_ang_x <= (angulo_objetivo + margen)) {
    digitalWrite(ledX, HIGH);
  } else {
    digitalWrite(ledX, LOW);
  }

  if (millis() - tiempoUltimaImpresion > 500) {
    Serial.print("Angulo X fisico: ");
    Serial.print(accel_ang_x, 1);
    Serial.print(" | Objetivo Alexa: ");
    Serial.println(angulo_objetivo, 1);
    
    tiempoUltimaImpresion = millis();
  }

  delay(10);
}
