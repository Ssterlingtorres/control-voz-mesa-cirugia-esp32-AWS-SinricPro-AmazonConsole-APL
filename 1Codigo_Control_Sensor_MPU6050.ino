#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050.h"

// Objeto para interactuar con el MPU6050
MPU6050 sensor;

// Variables para guardar los valores brutos de aceleración en los 3 ejes
int16_t ax, ay, az;

// Definición de pines para los LEDs de control de posición
const int PIN_LED_X = 2;  // LED para inclinación lateral
const int PIN_LED_Y = 4;  // LED para posición Trendelenburg

void setup() {
  // Inicializamos el monitor serial a 115200 baudios
  Serial.begin(115200);

  // Configuramos los pines I2C en el ESP32 (SDA en pin 21 y SCL en pin 22)
  Wire.begin(21, 22);

  // Configuramos los pines de los LEDs como salida
  pinMode(PIN_LED_X, OUTPUT);
  pinMode(PIN_LED_Y, OUTPUT);

  // Arrancamos el sensor MPU6050
  sensor.initialize();

  // Verificamos si la ESP32 reconoce el sensor correctamente
  if (sensor.testConnection()) {
    Serial.println("Sensor MPU6050 iniciado correctamente");
  } else {
    Serial.println("Error al iniciar el sensor MPU6050");
  }
}

void loop() {
  // Leemos las aceleraciones brutas del sensor
  sensor.getAcceleration(&ax, &ay, &az);

  // Calculamos el ángulo de inclinación en el eje X (Inclinación lateral)
  // Convertimos de radianes a grados usando la constante nativa PI
  float accel_ang_x = atan(ax / sqrt(pow(ay, 2) + pow(az, 2))) * (180.0 / PI);

  // Calculamos el ángulo de inclinación en el eje Y (Trendelenburg)
  float accel_ang_y = atan(ay / sqrt(pow(ax, 2) + pow(az, 2))) * (180.0 / PI);

  // Imprimimos los ángulos actuales para monitorear el movimiento en tiempo real
  Serial.print("Inclinacion en X: ");
  Serial.print(accel_ang_x);
  Serial.print(" | Inclinacion en Y: ");
  Serial.println(accel_ang_y);

  // Lógica para activar el LED de inclinación lateral (Entre 20° y 35°)
  if (accel_ang_x >= 20 && accel_ang_x <= 35) {
    digitalWrite(PIN_LED_X, HIGH);
  } else {
    digitalWrite(PIN_LED_X, LOW);
  }

  // Lógica para activar el LED de posición Trendelenburg (Entre 5° y 15°)
  if (accel_ang_y >= 5 && accel_ang_y <= 15) {
    digitalWrite(PIN_LED_Y, HIGH);
  } else {
    digitalWrite(PIN_LED_Y, LOW);
  }

  // Pequeña pausa de 10ms para no saturar el procesador y estabilizar lecturas
  delay(10);
}