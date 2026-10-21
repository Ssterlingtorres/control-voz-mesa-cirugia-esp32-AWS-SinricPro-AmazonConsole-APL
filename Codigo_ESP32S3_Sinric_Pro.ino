#include <WiFi.h>                                   // Librería para conectar la ESP32 al WiFi
#include <SinricPro.h>                              // Librería principal de Sinric Pro
#include <SinricProDevice.h>                        // Permite crear dispositivos propios en Sinric Pro
#include <Capabilities/RangeController.h>           // Capacidad para recibir valores numéricos desde Alexa
#include <Capabilities/PowerStateController.h>      // Capacidad para recibir encendido y apagado desde Alexa

#include <I2Cdev.h>                                 // Manejo de la comunicación I2C del sensor
#include <MPU6050.h>                                // Librería del sensor MPU6050
#include <Wire.h>                                   // Bus I2C de la ESP32

// WIFI
// Datos de la red WiFi, se cambian por los de la red que se vaya a usar
#define WIFI_SSID     "NOMBRE_DE_SU_RED"            // Nombre de la red WiFi
#define WIFI_PASS     "CONTRASENA_DE_SU_RED"        // Contraseña de la red WiFi

// SINRIC CREDENCIALES
// Credenciales de Sinric Pro, son privadas y no se deben publicar
#define APP_KEY       "SU_APP_KEY"                  // Llave de la cuenta en Sinric Pro
#define APP_SECRET    "SU_APP_SECRET"               // Clave secreta de la cuenta en Sinric Pro

// DEVICE IDs ACTUALIZADOS
// ID de cada dispositivo creado en Sinric Pro
#define DEVICE_ID_TREND  "SU_DEVICE_ID_TREND"       // Dispositivo de Trendelenburg
#define DEVICE_ID_TIL    "SU_DEVICE_ID_TILT"        // Dispositivo de tilt
#define DEVICE_ID_ALTURA "SU_DEVICE_ID_ALTURA"      // Dispositivo de altura

// DISPOSITIVO CUSTOM
class MesaCustom : public SinricProDevice,          // Se crea el dispositivo mesa a partir de Sinric Pro
                   public RangeController<MesaCustom>,  // con control de rango (valores numéricos)
                   public PowerStateController<MesaCustom> {  // y con encendido y apagado
  friend class RangeController<MesaCustom>;         // Deja que el control de rango acceda a la clase
  friend class PowerStateController<MesaCustom>;    // Deja que el encendido y apagado acceda a la clase
public:                                             // Lo siguiente se puede usar desde afuera
  MesaCustom(const String &deviceId) : SinricProDevice(deviceId, "MesaCustom") {}  // Constructor, enlaza la mesa con su ID
};                                                  // Fin de la clase

// SENSORES
MPU6050 sensor;                                     // Objeto del sensor MPU6050
int16_t ax, ay, az;                                 // Lecturas del acelerómetro en X, Y y Z

// Pines para el Ultrasónico
#define TRIG_PIN 13                                 // Pin que dispara el pulso del ultrasónico
#define ECHO_PIN 14                                 // Pin que recibe el eco del ultrasónico

// PINES ACTUADORES
int trendSubir = 2;                                 // Salida para subir Trendelenburg
int trendBajar = 4;                                 // Salida para bajar Trendelenburg

int tilIzq = 18;                                    // Salida para tilt izquierdo
int tilDer = 19;                                    // Salida para tilt derecho

int alturaSubir = 25;                               // Salida para subir la altura
int alturaBajar = 26;                               // Salida para bajar la altura

// CONTROL DE METAS
float objetivoX = 0;                                // Ángulo pedido de Trendelenburg
float objetivoY = 0;                                // Ángulo pedido de tilt
float objetivoAltura = 0;                           // Altura pedida

float margenAngulo = 3.0;                           // Tolerancia en grados
float margenAltura = 2.0;                           // Tolerancia en centímetros

bool enMovimientoX = false;                         // Si Trendelenburg se está moviendo
bool enMovimientoY = false;                         // Si el tilt se está moviendo
bool enMovimientoAltura = false;                    // Si la altura se está moviendo

float alturaActual = 0;                             // Altura medida por el ultrasónico

unsigned long tiempoUltimoEco = 0;                  // Última vez que se midió con el ultrasónico
unsigned long tiempoUltimaImpresion = 0;            // Última vez que se imprimió en el monitor serial

// CALLBACKS
bool onPowerState(const String &deviceId, bool &state) {  // Se ejecuta cuando Alexa manda encender o apagar
  Serial.printf("Power State %s -> %s\n", deviceId.c_str(), state ? "ON" : "OFF");  // Muestra qué dispositivo y si es ON u OFF
  return true;                                      // Le confirma a Sinric Pro que se recibió bien
}

bool onRangeValue(const String &deviceId, const String& instance, float &value) {  // Se ejecuta cuando Alexa manda un valor

  // TREND
  if (deviceId == DEVICE_ID_TREND) {                // Si el valor es para Trendelenburg
    objetivoX = value;                              // Guarda el ángulo pedido
    enMovimientoX = true;                           // Arranca el movimiento
    Serial.printf("TREND solicitado -> %.1f°\n", value);  // Muestra la orden recibida
    return true;                                    // Confirma a Sinric Pro
  }

  // TIL
  if (deviceId == DEVICE_ID_TIL) {                  // Si el valor es para el tilt
    objetivoY = value;                              // Guarda el ángulo pedido
    enMovimientoY = true;                           // Arranca el movimiento
    Serial.printf("TIL solicitado -> %.1f°\n", value);  // Muestra la orden recibida
    return true;                                    // Confirma a Sinric Pro
  }

  // ALTURA
  if (deviceId == DEVICE_ID_ALTURA) {               // Si el valor es para la altura
    objetivoAltura = value;                         // Guarda la altura pedida
    enMovimientoAltura = true;                      // Arranca el movimiento
    Serial.printf("ALTURA solicitada -> %.1f cm\n", value);  // Muestra la orden recibida
    return true;                                    // Confirma a Sinric Pro
  }

  return false;                                     // Si no es ningún dispositivo conocido, no se hace nada
}

// SETUP WIFI
void setupWiFi() {                                  // Función para conectar al WiFi
  Serial.print("Conectando WiFi");                  // Mensaje de inicio de conexión
  WiFi.begin(WIFI_SSID, WIFI_PASS);                 // Intenta conectarse con la red y la clave
  while (WiFi.status() != WL_CONNECTED) {           // Espera hasta que se conecte
    Serial.print(".");                              // Imprime un punto mientras espera
    delay(500);                                     // Espera medio segundo entre intentos
  }
  Serial.println("\nWiFi conectado");               // Avisa que ya hay conexión
}

// SETUP
void setup() {                                      // Se ejecuta una sola vez al encender
  Serial.begin(115200);                             // Inicia el monitor serial
  Serial.println("\n--- Iniciando Sistema Quirúrgico ---");  // Mensaje de arranque

  pinMode(trendSubir, OUTPUT);                      // Salida para subir Trendelenburg
  pinMode(trendBajar, OUTPUT);                      // Salida para bajar Trendelenburg
  pinMode(tilIzq, OUTPUT);                          // Salida para tilt izquierdo
  pinMode(tilDer, OUTPUT);                          // Salida para tilt derecho
  pinMode(alturaSubir, OUTPUT);                     // Salida para subir la altura
  pinMode(alturaBajar, OUTPUT);                     // Salida para bajar la altura

  pinMode(TRIG_PIN, OUTPUT);                        // El TRIG del ultrasónico es salida
  pinMode(ECHO_PIN, INPUT);                         // El ECHO del ultrasónico es entrada

  Wire.begin(21, 22);                               // I2C en los pines 21 (SDA) y 22 (SCL)
  sensor.initialize();                              // Arranca el MPU6050

  if (sensor.testConnection()) {                    // Revisa si el sensor responde
    Serial.println("MPU6050 OK");                   // El sensor está bien conectado
  } else {                                          // Si no responde
    Serial.println("MPU6050 ERROR");                // Avisa que hay un problema con el sensor
  }

  setupWiFi();                                      // Conecta la ESP32 al WiFi

  // CREAR DISPOSITIVOS
  MesaCustom &trend  = SinricPro[DEVICE_ID_TREND];  // Crea el dispositivo de Trendelenburg
  MesaCustom &til    = SinricPro[DEVICE_ID_TIL];    // Crea el dispositivo de tilt
  MesaCustom &altura = SinricPro[DEVICE_ID_ALTURA];  // Crea el dispositivo de altura

  trend.onRangeValue("angulo", onRangeValue);       // Cuando llegue un ángulo de Trendelenburg, llama a onRangeValue
  til.onRangeValue("angulo", onRangeValue);         // Cuando llegue un ángulo de tilt, llama a onRangeValue
  altura.onRangeValue("altura", onRangeValue);      // Cuando llegue una altura, llama a onRangeValue

  trend.onPowerState(onPowerState);                 // Encendido y apagado de Trendelenburg
  til.onPowerState(onPowerState);                   // Encendido y apagado de tilt
  altura.onPowerState(onPowerState);                // Encendido y apagado de altura

  SinricPro.onConnected([](){ Serial.println("Conectado a Servidores SinricPro/Alexa"); });  // Mensaje cuando se conecta
  SinricPro.onDisconnected([](){ Serial.println("Desconectado de SinricPro"); });  // Mensaje cuando se desconecta

  SinricPro.begin(APP_KEY, APP_SECRET);             // Inicia Sinric Pro con las credenciales
}

// LOOP
void loop() {                                       // Se repite todo el tiempo
  SinricPro.handle();                               // Mantiene la conexión y recibe órdenes de Alexa

  // LEER ACELERÓMETRO
  sensor.getAcceleration(&ax, &ay, &az);            // Lee la aceleración en los tres ejes
  float angX = atan(ax / sqrt(pow((float)ay, 2) + pow((float)az, 2))) * (180.0 / 3.1416);  // Inclinación en X (Trendelenburg) en grados
  float angY = atan(ay / sqrt(pow((float)ax, 2) + pow((float)az, 2))) * (180.0 / 3.1416);  // Inclinación en Y (tilt) en grados

  // LEER SENSOR ULTRASÓNICO (cada 50 ms)
  if (millis() - tiempoUltimoEco > 50) {            // Mide cada 50 ms
    digitalWrite(TRIG_PIN, LOW);                    // Deja el TRIG en bajo
    delayMicroseconds(2);                           // Espera corta para limpiar
    digitalWrite(TRIG_PIN, HIGH);                   // Manda el pulso
    delayMicroseconds(10);                          // El pulso dura 10 microsegundos
    digitalWrite(TRIG_PIN, LOW);                    // Termina el pulso

    long duracion = pulseIn(ECHO_PIN, HIGH, 20000);  // Mide cuánto tarda en volver el eco, máximo 20 ms
    if (duracion > 0) {                             // Si llegó el eco
      alturaActual = duracion * 0.034 / 2;          // Pasa el tiempo a centímetros (ida y vuelta)
    }
    tiempoUltimoEco = millis();                     // Guarda cuándo se midió
  }

  // CONTROL TREND
  bool enRangoX = (angX >= (objetivoX - margenAngulo) && angX <= (objetivoX + margenAngulo));  // Revisa si el ángulo está dentro del margen

  if (enMovimientoX) {                              // Si hay una orden de Trendelenburg
    if (enRangoX) {                                 // Si ya llegó
      digitalWrite(trendSubir, LOW);                // Apaga subir
      digitalWrite(trendBajar, LOW);                // Apaga bajar
      enMovimientoX = false;                        // Termina el movimiento
      Serial.println("Posición Trendelenburg alcanzada");  // Avisa que llegó
    } else if (angX < objetivoX) {                  // Si le falta
      digitalWrite(trendSubir, HIGH);               // Sube
      digitalWrite(trendBajar, LOW);                // Asegura que bajar esté apagado
    } else {                                        // Si se pasó
      digitalWrite(trendSubir, LOW);                // Asegura que subir esté apagado
      digitalWrite(trendBajar, HIGH);               // Baja
    }
  } else {                                          // Si no hay orden
    digitalWrite(trendSubir, LOW);                  // Todo apagado
    digitalWrite(trendBajar, LOW);                  // Todo apagado
  }

  // CONTROL TIL
  bool enRangoY = (angY >= (objetivoY - margenAngulo) && angY <= (objetivoY + margenAngulo));  // Revisa si el ángulo está dentro del margen

  if (enMovimientoY) {                              // Si hay una orden de tilt
    if (enRangoY) {                                 // Si ya llegó
      digitalWrite(tilIzq, LOW);                    // Apaga izquierda
      digitalWrite(tilDer, LOW);                    // Apaga derecha
      enMovimientoY = false;                        // Termina el movimiento
      Serial.println("Posición Til alcanzada");     // Avisa que llegó
    } else if (angY < objetivoY) {                  // Si le falta
      digitalWrite(tilIzq, HIGH);                   // Inclina a la izquierda
      digitalWrite(tilDer, LOW);                    // Asegura que derecha esté apagado
    } else {                                        // Si se pasó
      digitalWrite(tilIzq, LOW);                    // Asegura que izquierda esté apagado
      digitalWrite(tilDer, HIGH);                   // Inclina a la derecha
    }
  } else {                                          // Si no hay orden
    digitalWrite(tilIzq, LOW);                      // Todo apagado
    digitalWrite(tilDer, LOW);                      // Todo apagado
  }

  // CONTROL ALTURA
  bool enRangoAltura = (alturaActual >= (objetivoAltura - margenAltura) && alturaActual <= (objetivoAltura + margenAltura));  // Revisa si la altura está dentro del margen

  if (enMovimientoAltura && alturaActual > 0) {     // Si hay orden de altura y el sensor tiene lectura
    if (enRangoAltura) {                            // Si ya llegó
      digitalWrite(alturaSubir, LOW);               // Apaga subir
      digitalWrite(alturaBajar, LOW);               // Apaga bajar
      enMovimientoAltura = false;                   // Termina el movimiento
      Serial.println("Posición Altura alcanzada");  // Avisa que llegó
    } else if (alturaActual < objetivoAltura) {     // Si le falta
      digitalWrite(alturaSubir, HIGH);              // Sube
      digitalWrite(alturaBajar, LOW);               // Asegura que bajar esté apagado
    } else {                                        // Si se pasó
      digitalWrite(alturaSubir, LOW);               // Asegura que subir esté apagado
      digitalWrite(alturaBajar, HIGH);              // Baja
    }
  } else {                                          // Si no hay orden
    digitalWrite(alturaSubir, LOW);                 // Todo apagado
    digitalWrite(alturaBajar, LOW);                 // Todo apagado
  }

  // MONITOR SERIAL (datos en vivo)
  if (millis() - tiempoUltimaImpresion > 500) {     // Imprime cada medio segundo
    Serial.printf("X: %.1f° (Obj: %.1f°) | Y: %.1f° (Obj: %.1f°) | Altura: %.1f cm (Obj: %.1f cm)\n",  // Formato del mensaje con valores medidos y pedidos
                  angX, objetivoX, angY, objetivoY, alturaActual, objetivoAltura);  // Valores que se imprimen
    tiempoUltimaImpresion = millis();               // Guarda el momento de esta impresión
  }

  delay(10);                                        // Pausa corta para no saturar la ESP32
}
