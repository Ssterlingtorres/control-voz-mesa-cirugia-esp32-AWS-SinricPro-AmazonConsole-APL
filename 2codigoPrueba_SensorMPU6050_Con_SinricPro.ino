#include <WiFi.h>                         // Librería para conectar la ESP32 al WiFi
#include <SinricPro.h>                    // Librería principal de Sinric Pro
#include <SinricProDevice.h>              // Permite crear dispositivos propios en Sinric Pro
#include <Capabilities/RangeController.h> // Capacidad para recibir valores numéricos desde Alexa

#include <I2Cdev.h>                       // Manejo de la comunicación I2C del sensor
#include <MPU6050.h>                      // Librería del sensor MPU6050
#include <Wire.h>                         // Bus I2C de la ESP32

// WIFI
// Datos de la red WiFi, se cambian por los de la red que se vaya a usar
#define WIFI_SSID     "NOMBRE_DE_SU_RED"      // Nombre de la red WiFi
#define WIFI_PASS     "CONTRASENA_DE_SU_RED"  // Contraseña de la red WiFi

// SINRIC CREDENCIALES NUEVAS 
// Credenciales de Sinric Pro, son privadas y no se deben publicar
#define APP_KEY       "SU_APP_KEY"      // Llave de la cuenta en Sinric Pro
#define APP_SECRET    "SU_APP_SECRET"   // Clave secreta de la cuenta en Sinric Pro
#define DEVICE_ID     "SU_DEVICE_ID"    // ID del dispositivo "La mesa" en Sinric Pro

// CREAR LA ESTRUCTURA DEL DISPOSITIVO PERSONALIZADO 
class MesaCustom : public SinricProDevice, public RangeController<MesaCustom> { // Se crea el dispositivo mesa con control de rango
  friend class RangeController<MesaCustom>;                                     // Deja que el control de rango acceda a la clase
public:                                                                          // Lo siguiente se puede usar desde afuera
  MesaCustom(const String &deviceId) : SinricProDevice(deviceId, "MesaCustom") {} // Constructor, enlaza la mesa con su ID
};                                                                               // Fin de la clase

// SENSOR
MPU6050 sensor;      // Objeto del sensor MPU6050
int16_t ax, ay, az;  // Lecturas del acelerómetro en X, Y y Z

// LED
int ledX = 2;        // Pin del LED que indica si llegó al ángulo

// CONTROL 
float angulo_objetivo = 0;                // Ángulo que pide Alexa
float margen = 3.0;                       // Tolerancia en grados para dar por bueno el ángulo
unsigned long tiempoUltimaImpresion = 0;  // Guarda cuándo se imprimió por última vez

// CALLBACK ALEXA (CUSTOM DEVICE) 
bool onRangeValue(const String &deviceId, const String& instance, float &rangeValue) { // Se ejecuta cuando Alexa manda un valor
  
  if (instance == "angulo") {             // Revisa que el valor sea del ángulo
    angulo_objetivo = rangeValue;         // Guarda el ángulo que pidió Alexa

    Serial.println("\n=========================================");                    // Línea separadora en el monitor serial
    // Ahora imprimirá correctamente los valores negativos (ej: -90.0)
    Serial.printf("ALEXA: Orden recibida -> %.1f grados\n", angulo_objetivo);       // Muestra la orden recibida
    Serial.println("=========================================\n");                  // Cierra el separador
    
    return true;                          // Le confirma a Sinric Pro que se recibió bien
  }
  return false;                           // Si no es el ángulo, no se hace nada
}

// SETUP WIFI
void setupWiFi() {                        // Función para conectar al WiFi
  Serial.print("Conectando a WiFi");      // Mensaje de inicio de conexión
  WiFi.begin(WIFI_SSID, WIFI_PASS);       // Intenta conectarse con la red y la clave

  while (WiFi.status() != WL_CONNECTED) { // Espera hasta que se conecte
    Serial.print(".");                    // Imprime un punto mientras espera
    delay(500);                           // Espera medio segundo entre intentos
  }
  Serial.println("\n WiFi conectado");  // Avisa que ya hay conexión
}

// SETUP 
void setup() {                            // Se ejecuta una sola vez al encender
  Serial.begin(115200);                   // Inicia el monitor serial
  pinMode(ledX, OUTPUT);                  // Pone el pin del LED como salida

  // Inicializar I2C y sensor
  Wire.begin(21, 22);                     // I2C en los pines 21 (SDA) y 22 (SCL)
  sensor.initialize();                    // Arranca el MPU6050

  if (sensor.testConnection()) {                              // Revisa si el sensor responde
    Serial.println(" MPU6050 conectado correctamente");     // El sensor está bien conectado
  } else {                                                    // Si no responde
    Serial.println(" Error al conectar MPU6050");           // Avisa que hay un problema con el sensor
  }

  // WiFi
  setupWiFi();                            // Conecta la ESP32 al WiFi

  // INICIALIZAR EL DISPOSITIVO PERSONALIZADO
  MesaCustom &miMesa = SinricPro[DEVICE_ID];        // Crea la mesa en Sinric Pro con su ID
  miMesa.onRangeValue("angulo", onRangeValue);      // Cuando llegue un ángulo, llama a onRangeValue

  SinricPro.onConnected([](){ Serial.println(" Conectado a Alexa"); });      // Mensaje cuando se conecta
  SinricPro.onDisconnected([](){ Serial.println(" Desconectado de Alexa"); }); // Mensaje cuando se desconecta

  SinricPro.begin(APP_KEY, APP_SECRET);   // Inicia Sinric Pro con las credenciales
}

// LOOP 
void loop() {                             // Se repite todo el tiempo
  SinricPro.handle();                     // Mantiene la conexión y recibe órdenes de Alexa

  // Leer sensor
  sensor.getAcceleration(&ax, &ay, &az);  // Lee la aceleración en los tres ejes

  // Cálculo del ángulo físico
  float accel_ang_x = atan(ax / sqrt(pow(ay,2) + pow(az,2))) * (180.0 / 3.1416); // Calcula la inclinación en X y la pasa a grados

  // Comparación dinámica (funciona perfecto con negativos)
  if (accel_ang_x >= (angulo_objetivo - margen) && accel_ang_x <= (angulo_objetivo + margen)) { // Revisa si el ángulo está dentro del margen
    digitalWrite(ledX, HIGH);             // Llegó al ángulo, prende el LED
  } else {                                // Si todavía no llega
    digitalWrite(ledX, LOW);              // Apaga el LED
  }

  if (millis() - tiempoUltimaImpresion > 500) {   // Imprime cada medio segundo
    Serial.print("Angulo X fisico: ");            // Texto antes del ángulo medido
    Serial.print(accel_ang_x, 1);                 // Ángulo real del sensor con un decimal
    Serial.print(" | Objetivo Alexa: ");          // Separador y texto del ángulo pedido
    Serial.println(angulo_objetivo, 1);           // Ángulo que pidió Alexa con un decimal
    
    tiempoUltimaImpresion = millis();             // Guarda el momento de esta impresión
  }

  delay(10);                              // Pausa corta para no saturar la ESP32
}
