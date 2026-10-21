#include <WiFiClientSecure.h>                               // Conexión WiFi segura con certificados (TLS)
#include <PubSubClient.h>                                   // Cliente MQTT para hablar con AWS IoT Core
#include <ArduinoJson.h>                                    // Para armar y leer mensajes en formato JSON
#include <WiFi.h>                                           // Librería para conectar la ESP32 al WiFi
#include <Wire.h>                                           // Bus I2C de la ESP32
#include <I2Cdev.h>                                         // Manejo de la comunicación I2C de los sensores
#include <MPU6050.h>                                        // Librería de los sensores de inclinación MPU6050
#include <VL53L0X.h>                                        // Librería de los sensores de distancia ToF VL53L0X

// WIFI
// Datos de la red WiFi, se cambian por los de la red que se vaya a usar
const char* WIFI_SSID = "NOMBRE_DE_SU_RED";                 // Nombre de la red WiFi
const char* WIFI_PASS = "CONTRASENA_DE_SU_RED";             // Contraseña de la red WiFi

// AWS IOT CORE
const char* AWS_IOT_ENDPOINT      = "SU_ENDPOINT-ats.iot.us-east-1.amazonaws.com";  // Dirección del servidor de AWS IoT, sale en Configuración de AWS IoT
const char* AWS_IOT_TOPIC         = "quirofano/mesa/control";  // Tópico por donde llegan los comandos desde Lambda
const char* AWS_IOT_SHADOW_UPDATE = "$aws/things/Mesa_Quirurgica/shadow/update";  // Tópico donde se publica el estado de la mesa (Shadow)

// CERTIFICADOS
// Certificado raíz de Amazon (Amazon Root CA 1), es público y es el mismo para todos
const char AWS_CERT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

// Certificado del dispositivo, se descarga de AWS IoT al crear el objeto (archivo certificate.pem.crt)
// Es privado, se pega aquí solo en la copia que se carga a la ESP32
const char AWS_CERT_CRT[] PROGMEM = R"EOF(
PEGAR AQUI EL CERTIFICADO DEL DISPOSITIVO
)EOF";

// Llave privada del dispositivo (archivo private.pem.key)
// Nunca se publica, quien la tenga puede hacerse pasar por la mesa en AWS
const char AWS_CERT_PRIVATE[] PROGMEM = R"EOF(
PEGAR AQUI LA LLAVE PRIVADA DEL DISPOSITIVO
)EOF";

// RELÉS
#define RELE_ENCENDIDO LOW                                  // Los relés se activan con nivel bajo
#define RELE_APAGADO   HIGH                                 // Con nivel alto el relé queda apagado

// PINES I2C
const int I2C0_SDA = 8;                                     // SDA del primer bus I2C (sensores MPU6050)
const int I2C0_SCL = 9;                                     // SCL del primer bus I2C
const int I2C1_SDA = 1;                                     // SDA del segundo bus I2C (sensores ToF)
const int I2C1_SCL = 2;                                     // SCL del segundo bus I2C

// PINES XSHUT DE LOS ToF
const int XSHUT_ALTURA = 16;                                // Pin para prender o apagar el ToF de altura
const int XSHUT_LONG   = 17;                                // Pin para prender o apagar el ToF longitudinal

// PINES DE LOS RELÉS DE CADA MOVIMIENTO
const int sol_Subir       = 4;                              // Relé para subir la mesa
const int sol_Bajar       = 5;                              // Relé para bajar la mesa
const int sol_Tren        = 6;                              // Relé para Trendelenburg
const int sol_AntiTren    = 7;                              // Relé para anti Trendelenburg
const int sol_TiltDer     = 10;                             // Relé para tilt derecho
const int sol_TiltIzq     = 11;                             // Relé para tilt izquierdo
const int sol_FowlerSubir = 12;                             // Relé para subir el espaldar
const int sol_FowlerBajar = 13;                             // Relé para bajar el espaldar
const int sol_Cefalico    = 14;                             // Relé para movimiento cefálico
const int sol_Caudal      = 15;                             // Relé para movimiento caudal

// PINES DEL DRIVER KINCO
const int PIN_PULSO     = 42;                               // Pulsos que mueven el servomotor
const int PIN_DIRECCION = 41;                               // Sentido de giro del servomotor
const int RELE_ENABLE   = 39;                               // Relé que habilita el driver
const int PIN_FRENO     = 38;                               // Relé que suelta el freno del motor

// PARÁMETROS DE MOVIMIENTO
const float VALOR_UN_POCO_MM = 25.0;                        // Valor que se usa cuando se pide "un poco"
const float MS_POR_MILIMETRO_Z = 119.78;                    // Tiempo que tarda la altura en moverse un milímetro
const float AVANCE_POR_VUELTA_MM = 5.0;                     // Milímetros que avanza el tornillo por vuelta
const long PULSOS_POR_VUELTA = 10000;                       // Pulsos que necesita el motor para dar una vuelta
const int VELOCIDAD_MICROSEGUNDOS = 50;                     // Tiempo base entre pulsos
const float TOLERANCIA_MM = 1.0;                            // Margen de error permitido en milímetros
const float TOLERANCIA_GRADOS = 0.2;                        // Margen de error permitido en grados
const unsigned long TIMEOUT_SEGURIDAD_MS = 25000;           // Tiempo máximo de un movimiento, 25 segundos

// LÍMITES DE RECORRIDO (máximos y mínimos de cada movimiento)
// Al llegar a estos valores el relé o el motor se apaga aunque no se haya
// completado la cantidad pedida. Si ya está en el extremo, no se activa.
const float LIMITE_ALTURA_MAX_MM        = 345.0;            // Altura máxima, 34,5 cm
const float LIMITE_ALTURA_MIN_MM        = 73.0;             // Altura mínima, 7,3 cm
const float LIMITE_TREN_MAX_GRADOS      = 26.0;             // Máximo de Trendelenburg
const float LIMITE_ANTITREN_MIN_GRADOS  = -5.4;             // Máximo de anti Trendelenburg
const float LIMITE_TILT_IZQ_MAX_GRADOS  = 18.0;             // Máximo de tilt lateral izquierdo
const float LIMITE_TILT_DER_MIN_GRADOS  = -22.0;            // Máximo de tilt lateral derecho
const float LIMITE_CEFALICO_MIN_MM      = 55.0;             // Extremo cefálico medido con el ToF longitudinal
const float LIMITE_CAUDAL_MAX_MM        = 187.0;            // Extremo caudal medido con el ToF longitudinal
const float LIMITE_FOWLER_SUBIR_GRADOS  = -19.6;            // Máximo del espaldar subiendo
const float LIMITE_FOWLER_BAJAR_GRADOS  = 28.3;             // Máximo del espaldar bajando

// FILTROS AJUSTADOS PARA RESPUESTA RÁPIDA
bool primeraLectura = true;                                 // Indica si es la primera vez que se leen los sensores
const float ALPHA_MPU = 0.25;                               // Suavizado de los MPU6050, más bajo es más suave
const float ALPHA_TOF = 0.50;                               // Suavizado de los ToF, la mediana ya quita los picos

// CONFIGURACIÓN DE PRECISIÓN DE LOS SENSORES ToF (VL53L0X)
// Altura: modo de largo alcance (hasta 2 m aprox.) para no perder la medida por encima de 180 mm.
// Longitudinal: modo normal, que es más exacto en distancias cortas y medias.
// Más tiempo de medida (timing budget) significa menos ruido.
// Cada muestra se valida con el registro de estado del sensor y las malas se descartan.
// Se usa un filtro de mediana de 5 muestras para quitar saltos y picos.
// La lectura no se queda esperando, solo lee cuando hay un dato nuevo,
// así no se frenan los pulsos del motor ni el control.
const uint32_t TOF_BUDGET_ALTURA_US = 66000;                // Tiempo de medida del ToF de altura, unas 15 medidas por segundo
const uint32_t TOF_BUDGET_LONG_US   = 50000;                // Tiempo de medida del ToF longitudinal, unas 20 medidas por segundo
const uint16_t TOF_MIN_VALIDO_MM = 20;                      // Por debajo de esto el sensor no es confiable
const uint16_t TOF_MAX_VALIDO_MM = 2000;                    // Por encima se toma como fuera de rango
const unsigned long TOF_TIMEOUT_DATO_MS = 500;              // Si no hay medida buena en este tiempo se marca error (-1)
#define TOF_VENTANA_MEDIANA 5                               // Cantidad de muestras para la mediana

// Calibración con regla o flexómetro (por defecto sin corrección)
// medida real = medida del sensor * ESCALA + OFFSET
const float TOF_ALTURA_ESCALA    = 1.0;                     // Escala del ToF de altura
const float TOF_ALTURA_OFFSET_MM = 0.0;                     // Corrección fija del ToF de altura
const float TOF_LONG_ESCALA      = 1.0;                     // Escala del ToF longitudinal
const float TOF_LONG_OFFSET_MM   = 0.0;                     // Corrección fija del ToF longitudinal

// Registros internos del VL53L0X (los mismos que usa la librería Pololu)
const uint8_t TOF_REG_SYSTEM_INTERRUPT_CLEAR  = 0x0B;       // Registro para limpiar el aviso de dato nuevo
const uint8_t TOF_REG_RESULT_INTERRUPT_STATUS = 0x13;       // Registro que dice si hay dato nuevo
const uint8_t TOF_REG_RESULT_RANGE_STATUS     = 0x14;       // Registro con el estado y la distancia medida

struct EstadoToF {                                          // Guarda todo lo que necesita cada ToF para filtrar
  uint16_t muestras[TOF_VENTANA_MEDIANA];                   // Últimas muestras válidas
  uint8_t indice;                                           // Posición donde se guarda la siguiente muestra
  uint8_t cantidad;                                         // Cuántas muestras hay guardadas
  bool hayDato;                                             // Si ya llegó al menos una medida buena
  bool filtroIniciado;                                      // Si el filtro ya arrancó
  unsigned long tiempoUltimaValida;                         // Momento de la última medida buena
};

EstadoToF estadoToFAltura = {{0}, 0, 0, false, false, 0};   // Estado del ToF de altura, arranca en cero
EstadoToF estadoToFLong   = {{0}, 0, 0, false, false, 0};   // Estado del ToF longitudinal, arranca en cero

float filtroAltura = 0;                                     // Altura filtrada
float filtroLongitudinal = 0;                               // Posición longitudinal filtrada
float filtroTren = 0;                                       // Ángulo de Trendelenburg filtrado
float filtroTilt = 0;                                       // Ángulo de tilt filtrado
float filtroEspalda = 0;                                    // Ángulo del espaldar filtrado

float estableAltura = 0;                                    // Último valor estable de altura
float estableLongitudinal = 0;                              // Último valor estable longitudinal
float estableTren = 0;                                      // Último valor estable de Trendelenburg
float estableTilt = 0;                                      // Último valor estable de tilt
float estableFowler = 0;                                    // Último valor estable de Fowler

// Variables de memoria para detectar saltos intencionales
unsigned long tiempoCheckMov = 0;                           // Última vez que se revisó si la mesa se movió
float memAlt = 0, memLong = 0, memTren = 0, memTilt = 0, memFow = 0;  // Valores anteriores para comparar

unsigned long tiempoAnteriorMonitor = 0;                    // Última vez que se imprimió en el monitor serial
unsigned long tiempoUltimoShadow = 0;                       // Última vez que se mandó el estado a AWS
unsigned long tiempoUltimoMovimiento = 0;                   // Última vez que se detectó movimiento
bool mesaEnMovimiento = false;                              // Indica si la mesa se está moviendo

// VARIABLE GLOBAL DE PARO DE EMERGENCIA
volatile bool paroActivo = false;                           // Se pone en true cuando se pide paro de emergencia

WiFiClientSecure net;                                       // Conexión segura que usa MQTT
PubSubClient client(net);                                   // Cliente MQTT sobre la conexión segura

MPU6050 mpuBase(0x68);                                      // MPU6050 de la base, mide Trendelenburg y tilt
MPU6050 mpuEspalda(0x69);                                   // MPU6050 del espaldar, mide Fowler

VL53L0X tofAltura;                                          // Sensor ToF que mide la altura
VL53L0X tofLongitudinal;                                    // Sensor ToF que mide el desplazamiento longitudinal

// LECTURA VALIDADA DEL VL53L0X (sin esperar, con validación y mediana)
// Devuelve true solo cuando llega una muestra nueva y válida, y entrega en
// medianaMm la mediana de las últimas muestras válidas.
bool tofObtenerMuestra(VL53L0X &sensor, EstadoToF &est, float &medianaMm) {  // Lee el ToF y calcula la mediana
  uint8_t estadoInt = sensor.readReg(TOF_REG_RESULT_INTERRUPT_STATUS);  // Pregunta si hay dato nuevo
  if (sensor.last_status != 0 || (estadoInt & 0x07) == 0) return false;  // Error I2C o sin dato nuevo

  uint8_t estadoRango = (sensor.readReg(TOF_REG_RESULT_RANGE_STATUS) & 0x78) >> 3;  // Saca el estado de la medida
  uint16_t rango = sensor.readReg16Bit(TOF_REG_RESULT_RANGE_STATUS + 10);  // Lee la distancia en milímetros
  sensor.writeReg(TOF_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);    // Limpia el aviso para la siguiente medida
  if (sensor.last_status != 0) return false;                // Si falló el I2C, se descarta

  // 11 = medida completa y válida según el propio sensor
  if (estadoRango != 11 || rango < TOF_MIN_VALIDO_MM || rango > TOF_MAX_VALIDO_MM) return false;  // Descarta medidas malas o fuera de rango

  est.muestras[est.indice] = rango;                         // Guarda la muestra
  est.indice = (est.indice + 1) % TOF_VENTANA_MEDIANA;      // Pasa a la siguiente posición y vuelve al inicio
  if (est.cantidad < TOF_VENTANA_MEDIANA) est.cantidad++;   // Cuenta muestras hasta llenar la ventana
  est.hayDato = true;                                       // Ya hay al menos un dato bueno
  est.tiempoUltimaValida = millis();                        // Guarda cuándo llegó

  uint16_t orden[TOF_VENTANA_MEDIANA];                      // Copia para ordenar las muestras
  for (uint8_t i = 0; i < est.cantidad; i++) orden[i] = est.muestras[i];  // Copia las muestras
  for (uint8_t i = 1; i < est.cantidad; i++) {              // Ordena de menor a mayor (inserción)
    uint16_t clave = orden[i];                              // Valor que se va a ubicar
    int8_t j = i - 1;                                       // Empieza a comparar con el anterior
    while (j >= 0 && orden[j] > clave) { orden[j + 1] = orden[j]; j--; }  // Corre los mayores a la derecha
    orden[j + 1] = clave;                                   // Pone el valor en su lugar
  }

  if (est.cantidad % 2 == 1) {                              // Si hay cantidad impar de muestras
    medianaMm = orden[est.cantidad / 2];                    // La mediana es la del medio
  } else {                                                  // Si es par
    medianaMm = (orden[est.cantidad / 2 - 1] + orden[est.cantidad / 2]) / 2.0;  // Promedio de las dos del medio
  }
  return true;                                              // Hubo muestra nueva y válida
}

// PROTOTIPOS DE FUNCIONES
void publicarShadow(bool forzar);                           // Envía el estado de la mesa a AWS
float leerSensorAltura();                                   // Devuelve la altura en mm
float leerSensorLongitudinal();                             // Devuelve la posición longitudinal en mm
float leerAnguloTilt();                                     // Devuelve el ángulo de tilt
float leerAnguloTren();                                     // Devuelve el ángulo de Trendelenburg
float leerAnguloFowler();                                   // Devuelve el ángulo del espaldar

// FUNCIÓN DE PARO DE EMERGENCIA
void ejecutarParoEmergencia() {                             // Apaga todo de inmediato
  Serial.println("\nPARO DE EMERGENCIA");                   // Aviso en el monitor serial
  Serial.println("Deteniendo todos los actuadores...");     // Indica que se están apagando los actuadores

  digitalWrite(sol_Subir,       RELE_APAGADO);              // Apaga subir
  digitalWrite(sol_Bajar,       RELE_APAGADO);              // Apaga bajar
  digitalWrite(sol_Tren,        RELE_APAGADO);              // Apaga Trendelenburg
  digitalWrite(sol_AntiTren,    RELE_APAGADO);              // Apaga anti Trendelenburg
  digitalWrite(sol_TiltDer,     RELE_APAGADO);              // Apaga tilt derecho
  digitalWrite(sol_TiltIzq,     RELE_APAGADO);              // Apaga tilt izquierdo
  digitalWrite(sol_FowlerSubir, RELE_APAGADO);              // Apaga subir espaldar
  digitalWrite(sol_FowlerBajar, RELE_APAGADO);              // Apaga bajar espaldar
  digitalWrite(sol_Cefalico,    RELE_APAGADO);              // Apaga cefálico
  digitalWrite(sol_Caudal,      RELE_APAGADO);              // Apaga caudal

  digitalWrite(RELE_ENABLE, RELE_APAGADO);                  // Deshabilita el driver Kinco
  digitalWrite(PIN_FRENO, RELE_APAGADO);                    // Deja el freno puesto
  digitalWrite(PIN_PULSO, LOW);                             // Deja de mandar pulsos

  mesaEnMovimiento = false;                                 // La mesa ya no se mueve
  paroActivo = true;                                        // Queda activo el paro

  publicarShadow(true);                                     // Manda el estado a AWS de una vez

  Serial.println("Sistema detenido. Esperando reactivación.");  // Avisa que todo quedó quieto
  Serial.println("=========================================\n");  // Línea separadora en el monitor serial
}

// FUNCIÓN DE DESPERTAR (umbrales bajos para detectar rápido)
void detectarDespertarMatematico() {                        // Revisa si algún sensor cambió, o sea si la mesa se movió
  if (millis() - tiempoCheckMov >= 300) {                   // Revisa cada 300 ms
    float difAlt = abs(filtroAltura - memAlt);              // Cambio en la altura
    float difLong = abs(filtroLongitudinal - memLong);      // Cambio en la posición longitudinal
    float difTren = abs(filtroTren - memTren);              // Cambio en Trendelenburg
    float difTilt = abs(filtroTilt - memTilt);              // Cambio en tilt
    float difFow = abs(filtroEspalda - memFow);             // Cambio en el espaldar

    if (difAlt > 1.0 || difLong > 1.0 || difTren > 0.3 || difTilt > 0.3 || difFow > 0.3) {  // Si algo cambió más del umbral
      tiempoUltimoMovimiento = millis();                    // Se marca que hubo movimiento
    }

    memAlt = filtroAltura;                                  // Guarda la altura actual para la próxima comparación
    memLong = filtroLongitudinal;                           // Guarda la posición longitudinal
    memTren = filtroTren;                                   // Guarda Trendelenburg
    memTilt = filtroTilt;                                   // Guarda tilt
    memFow = filtroEspalda;                                 // Guarda el espaldar
    tiempoCheckMov = millis();                              // Guarda cuándo se revisó
  }
}

bool actualizandoSensores() {                               // Dice si los valores se deben actualizar
  detectarDespertarMatematico();                            // Revisa si hubo movimiento

  if (mesaEnMovimiento) {                                   // Si hay un comando en curso
    tiempoUltimoMovimiento = millis();                      // Se toma como movimiento
  }

  return (millis() - tiempoUltimoMovimiento) < 1000;        // Actualiza hasta 1 segundo después del último movimiento
}

float leerSensorAltura() {                                  // Lee la altura con el ToF
  float medianaMm;                                          // Aquí queda la mediana del sensor
  if (tofObtenerMuestra(tofAltura, estadoToFAltura, medianaMm)) {  // Si llegó una muestra buena
    float distancia = (medianaMm * TOF_ALTURA_ESCALA) + TOF_ALTURA_OFFSET_MM;  // Aplica la calibración

    if (primeraLectura || !estadoToFAltura.filtroIniciado) {  // Si es la primera lectura
      filtroAltura = distancia;                             // El filtro arranca con el valor medido
      estableAltura = distancia;                            // Y el valor estable también
      estadoToFAltura.filtroIniciado = true;                // Ya se inició el filtro
    }

    filtroAltura = (ALPHA_TOF * distancia) + ((1.0 - ALPHA_TOF) * filtroAltura);  // Filtro de suavizado
  }
  else if (!estadoToFAltura.hayDato || (millis() - estadoToFAltura.tiempoUltimaValida) > TOF_TIMEOUT_DATO_MS) {  // Sin datos buenos hace rato
    return -1.0;                                            // Devuelve error
  }

  if (actualizandoSensores()) {                             // Si la mesa se está moviendo
    estableAltura = round(filtroAltura);                    // Actualiza el valor estable redondeado
  }

  return estableAltura;                                     // Devuelve la altura
}

float leerSensorLongitudinal() {                            // Lee la posición longitudinal con el ToF
  float medianaMm;                                          // Aquí queda la mediana del sensor
  if (tofObtenerMuestra(tofLongitudinal, estadoToFLong, medianaMm)) {  // Si llegó una muestra buena
    float distancia = (medianaMm * TOF_LONG_ESCALA) + TOF_LONG_OFFSET_MM;  // Aplica la calibración

    if (primeraLectura || !estadoToFLong.filtroIniciado) {  // Si es la primera lectura
      filtroLongitudinal = distancia;                       // El filtro arranca con el valor medido
      estableLongitudinal = distancia;                      // Y el valor estable también
      estadoToFLong.filtroIniciado = true;                  // Ya se inició el filtro
    }

    filtroLongitudinal = (ALPHA_TOF * distancia) + ((1.0 - ALPHA_TOF) * filtroLongitudinal);  // Filtro de suavizado
  }
  else if (!estadoToFLong.hayDato || (millis() - estadoToFLong.tiempoUltimaValida) > TOF_TIMEOUT_DATO_MS) {  // Sin datos buenos hace rato
    return -1.0;                                            // Devuelve error
  }

  if (actualizandoSensores()) {                             // Si la mesa se está moviendo
    estableLongitudinal = round(filtroLongitudinal);        // Actualiza el valor estable redondeado
  }

  return estableLongitudinal;                               // Devuelve la posición longitudinal
}

float leerAnguloTren() {                                    // Lee el ángulo de Trendelenburg
  int16_t ax, ay, az;                                       // Lecturas crudas del acelerómetro
  mpuBase.getAcceleration(&ax, &ay, &az);                   // Lee el MPU de la base
  float axf = ax / 16384.0;                                 // Pasa X a unidades de gravedad
  float ayf = ay / 16384.0;                                 // Pasa Y a unidades de gravedad
  float azf = az / 16384.0;                                 // Pasa Z a unidades de gravedad
  float pitchBruto = atan2(axf, sqrt(ayf * ayf + azf * azf)) * 180.0 / PI;  // Calcula la inclinación en grados

  if (primeraLectura) {                                     // Si es la primera lectura
    filtroTren = pitchBruto;                                // El filtro arranca con este valor
    estableTren = pitchBruto;                               // Y el valor estable también
  }

  filtroTren = (ALPHA_MPU * pitchBruto) + ((1.0 - ALPHA_MPU) * filtroTren);  // Filtro de suavizado

  if (actualizandoSensores()) {                             // Si la mesa se está moviendo
    estableTren = filtroTren;                               // Actualiza el valor estable
  }

  return estableTren;                                       // Devuelve el ángulo
}

float leerAnguloTilt() {                                    // Lee el ángulo de tilt lateral
  int16_t ax, ay, az;                                       // Lecturas crudas del acelerómetro
  mpuBase.getAcceleration(&ax, &ay, &az);                   // Lee el MPU de la base
  float axf = ax / 16384.0;                                 // Pasa X a unidades de gravedad
  float ayf = ay / 16384.0;                                 // Pasa Y a unidades de gravedad
  float azf = az / 16384.0;                                 // Pasa Z a unidades de gravedad
  float rollBruto = atan2(ayf, sqrt(axf * axf + azf * azf)) * 180.0 / PI;  // Calcula la inclinación lateral en grados

  if (primeraLectura) {                                     // Si es la primera lectura
    filtroTilt = rollBruto;                                 // El filtro arranca con este valor
    estableTilt = rollBruto;                                // Y el valor estable también
  }

  filtroTilt = (ALPHA_MPU * rollBruto) + ((1.0 - ALPHA_MPU) * filtroTilt);  // Filtro de suavizado

  if (actualizandoSensores()) {                             // Si la mesa se está moviendo
    estableTilt = filtroTilt;                               // Actualiza el valor estable
  }

  return estableTilt;                                       // Devuelve el ángulo
}

float leerAnguloFowler() {                                  // Lee el ángulo del espaldar
  int16_t ax, ay, az;                                       // Lecturas crudas del acelerómetro
  mpuEspalda.getAcceleration(&ax, &ay, &az);                // Lee el MPU del espaldar
  float axf = ax / 16384.0;                                 // Pasa X a unidades de gravedad
  float ayf = ay / 16384.0;                                 // Pasa Y a unidades de gravedad
  float azf = az / 16384.0;                                 // Pasa Z a unidades de gravedad
  float pitchEspaldaBruto = atan2(axf, sqrt(ayf * ayf + azf * azf)) * 180.0 / PI;  // Calcula la inclinación del espaldar

  if (primeraLectura) {                                     // Si es la primera lectura
    filtroEspalda = pitchEspaldaBruto;                      // El filtro arranca con este valor
    estableFowler = pitchEspaldaBruto;                      // Y el valor estable también
  }

  filtroEspalda = (ALPHA_MPU * pitchEspaldaBruto) + ((1.0 - ALPHA_MPU) * filtroEspalda);  // Filtro de suavizado

  if (actualizandoSensores()) {                             // Si la mesa se está moviendo
    estableFowler = filtroEspalda;                          // Actualiza el valor estable
  }

  return (estableFowler - filtroTren);                      // Le resta la inclinación de la base para dar el ángulo propio del espaldar
}

void publicarShadow(bool forzar = false) {                  // Manda el estado de la mesa al Shadow de AWS
  unsigned long intervalo = (mesaEnMovimiento || forzar) ? 800 : 2000;  // Cada 0,8 s si se mueve, cada 2 s si está quieta

  if (millis() - tiempoUltimoShadow >= intervalo) {         // Si ya pasó el intervalo
    tiempoUltimoShadow = millis();                          // Guarda cuándo se publicó

    StaticJsonDocument<512> shadowDoc;                      // Documento JSON del Shadow
    JsonObject reported = shadowDoc.createNestedObject("state").createNestedObject("reported");  // Arma la parte state.reported

    float altura = leerSensorAltura();                      // Lee la altura
    float longitudinal = leerSensorLongitudinal();          // Lee la posición longitudinal

    if (altura != -1.0) reported["tofAltura"] = (int)altura;  // Solo la manda si no hay error
    if (longitudinal != -1.0) reported["tofLong"] = (int)longitudinal;  // Solo la manda si no hay error

    reported["mpuTilt"] = round(leerAnguloTilt() * 10.0) / 10.0;  // Tilt con un decimal
    reported["mpuTren"] = round(leerAnguloTren() * 10.0) / 10.0;  // Trendelenburg con un decimal
    reported["mpuFowler"] = round(leerAnguloFowler() * 10.0) / 10.0;  // Fowler con un decimal

    char shadowBuffer[512];                                 // Espacio para el texto del JSON
    serializeJson(shadowDoc, shadowBuffer);                 // Convierte el JSON a texto

    client.publish(AWS_IOT_SHADOW_UPDATE, shadowBuffer);    // Lo publica en AWS IoT
  }
}

void actualizarMonitor() {                                  // Imprime los valores de los sensores en el monitor serial
  if (millis() - tiempoAnteriorMonitor >= 500) {            // Cada medio segundo
    tiempoAnteriorMonitor = millis();                       // Guarda cuándo se imprimió

    float anguloTren = leerAnguloTren();                    // Lee Trendelenburg
    float anguloTilt = leerAnguloTilt();                    // Lee tilt
    float anguloFowler = leerAnguloFowler();                // Lee Fowler
    float distZ = leerSensorAltura();                       // Lee la altura
    float distX = leerSensorLongitudinal();                 // Lee la posición longitudinal

    Serial.print("-> [TELEMETRIA] Tren: "); Serial.print(anguloTren, 1);  // Imprime Trendelenburg
    Serial.print("° | Tilt: "); Serial.print(anguloTilt, 1);  // Imprime tilt
    Serial.print("° | Fowler: "); Serial.print(anguloFowler, 1);  // Imprime Fowler

    Serial.print("° | ToF Alt: ");                          // Texto de la altura
    if(distZ != -1.0) { Serial.print((int)distZ); Serial.print(" mm"); }  // Imprime la altura si hay dato
    else { Serial.print("Error"); }                         // Si no, imprime error

    Serial.print(" | ToF Long: ");                          // Texto de la posición longitudinal
    if(distX != -1.0) { Serial.print((int)distX); Serial.println(" mm"); }  // Imprime la posición si hay dato
    else { Serial.println("Error"); }                       // Si no, imprime error
  }
}

void liberarTodosLosSolenoides() {                          // Apaga todos los relés y el motor
  digitalWrite(sol_Subir, RELE_APAGADO);                    // Apaga subir
  digitalWrite(sol_Bajar, RELE_APAGADO);                    // Apaga bajar
  digitalWrite(sol_Tren, RELE_APAGADO);                     // Apaga Trendelenburg
  digitalWrite(sol_AntiTren, RELE_APAGADO);                 // Apaga anti Trendelenburg
  digitalWrite(sol_TiltDer, RELE_APAGADO);                  // Apaga tilt derecho
  digitalWrite(sol_TiltIzq, RELE_APAGADO);                  // Apaga tilt izquierdo
  digitalWrite(sol_FowlerSubir, RELE_APAGADO);              // Apaga subir espaldar
  digitalWrite(sol_FowlerBajar, RELE_APAGADO);              // Apaga bajar espaldar
  digitalWrite(sol_Cefalico, RELE_APAGADO);                 // Apaga cefálico
  digitalWrite(sol_Caudal, RELE_APAGADO);                   // Apaga caudal

  digitalWrite(PIN_FRENO, RELE_APAGADO);                    // Deja el freno puesto
  digitalWrite(RELE_ENABLE, RELE_APAGADO);                  // Deshabilita el driver Kinco
}

// CONTROL EN LAZO CERRADO DEL MOVIMIENTO LONGITUDINAL CON EL SENSOR ToF
// El motor se detiene cuando el ToF longitudinal detecta que se llegó a la
// distancia pedida. El conteo de pulsos queda como límite de seguridad
// por si el ToF falla o hay ruido.
void moverDistancia_mm(float distancia_mm, bool haciaCaudal) {  // Mueve la mesa hacia la cabeza o hacia los pies

  Serial.println("\n[MOTOR KINCO] Arrancando con control por sensor ToF...");  // Aviso de arranque
  Serial.print("Distancia objetivo: "); Serial.print(distancia_mm); Serial.println(" mm.");  // Muestra cuánto se va a mover

  // 1. Leer posición inicial con el sensor ToF longitudinal
  float pos_inicial = leerSensorLongitudinal();             // Posición antes de mover
  if (pos_inicial == -1.0) {                                // Si el sensor dio error
    delay(50);                                              // Espera un poco
    pos_inicial = leerSensorLongitudinal();                 // Vuelve a intentar
  }
  if (pos_inicial == -1.0) {                                // Si sigue con error
    Serial.println("ERROR: ToF longitudinal no responde. Abortando movimiento.");  // Avisa el error
    return;                                                 // No se mueve
  }
  Serial.print("Posicion inicial ToF: "); Serial.print(pos_inicial); Serial.println(" mm");  // Muestra la posición inicial

  // Límite de recorrido: si ya está en el extremo, no se arranca el motor
  if ((haciaCaudal && pos_inicial >= LIMITE_CAUDAL_MAX_MM) ||  // Ya está en el extremo caudal
      (!haciaCaudal && pos_inicial <= LIMITE_CEFALICO_MIN_MM)) {  // O ya está en el extremo cefálico
    return;                                                 // No se mueve
  }

  // 2. Calcular pulsos teóricos (se usan solo como límite de seguridad)
  long total_pulsos_teoricos = (distancia_mm / AVANCE_POR_VUELTA_MM) * PULSOS_POR_VUELTA;  // Pulsos que en teoría necesita
  long total_pulsos_max = total_pulsos_teoricos * 2;        // Se deja el doble de margen por si el ToF tarda

  // 3. Configurar dirección y habilitar motor
  if (haciaCaudal) {                                        // Si va hacia los pies
    digitalWrite(PIN_DIRECCION, HIGH);                      // Gira en un sentido
  } else {                                                  // Si va hacia la cabeza
    digitalWrite(PIN_DIRECCION, LOW);                       // Gira en el otro sentido
  }
  delay(10);                                                // Espera a que el driver tome la dirección

  digitalWrite(RELE_ENABLE, RELE_ENCENDIDO);                // Habilita el driver
  delay(20);                                                // Espera corta
  digitalWrite(PIN_FRENO, RELE_ENCENDIDO);                  // Suelta el freno
  delay(50);                                                // Espera a que el freno suelte

  mesaEnMovimiento = true;                                  // La mesa empieza a moverse

  // 4. Parámetros de rampa de velocidad
  const float VELOCIDAD_INICIAL_MM_S = 0.30;                // Velocidad al arrancar
  const float VELOCIDAD_OBJETIVO_MM_S = 5.90;               // Velocidad máxima
  const float ACELERACION_MM_S2 = 1.20;                     // Qué tan rápido sube la velocidad
  const float MM_POR_PULSO = AVANCE_POR_VUELTA_MM / (float)PULSOS_POR_VUELTA;  // Milímetros que avanza cada pulso
  unsigned long tiempo_inicio_mov = millis();               // Momento en que arranca

  // 5. Variables de control
  long pulsos_dados = 0;                                    // Pulsos enviados hasta ahora
  bool objetivo_alcanzado = false;                          // Si ya llegó a la distancia pedida
  unsigned long tiempo_ultimo_tof = 0;                      // Última lectura del ToF
  const unsigned long INTERVALO_TOF_MS = 50;                // Lee el ToF cada 50 ms

  // 6. Bucle principal controlado por el sensor ToF
  while (!objetivo_alcanzado && pulsos_dados < total_pulsos_max) {  // Sigue hasta llegar o pasar el límite de pulsos

    // 6.1 Verificar paro de emergencia
    if (paroActivo) {                                       // Si pidieron paro
      Serial.println("PARO detectado en bucle Kinco. Abortando.");  // Avisa
      break;                                                // Sale del bucle
    }

    // 6.2 Verificar tiempo máximo de seguridad
    if (millis() - tiempo_inicio_mov > TIMEOUT_SEGURIDAD_MS) {  // Si ya pasaron 25 s
      Serial.println("Error: Timeout motor Kinco.");        // Avisa
      break;                                                // Sale del bucle
    }

    // 6.3 Calcular velocidad actual según la rampa
    float t_seg = (millis() - tiempo_inicio_mov) / 1000.0;  // Segundos desde que arrancó
    float velocidad_actual = VELOCIDAD_INICIAL_MM_S + (ACELERACION_MM_S2 * t_seg);  // La velocidad sube con el tiempo
    if (velocidad_actual > VELOCIDAD_OBJETIVO_MM_S) {       // Si se pasa de la máxima
      velocidad_actual = VELOCIDAD_OBJETIVO_MM_S;           // Se deja en la máxima
    }
    if (velocidad_actual < 0.05) velocidad_actual = 0.05;   // Velocidad mínima para no dividir por cero

    float periodo_us = (MM_POR_PULSO / velocidad_actual) * 1000000.0;  // Tiempo de cada pulso en microsegundos
    unsigned int half_delay = (unsigned int)(periodo_us / 2.0);  // Mitad en alto y mitad en bajo
    if (half_delay < 1) half_delay = 1;                     // Mínimo un microsegundo

    // 6.4 Generar un pulso para el motor
    digitalWrite(PIN_PULSO, HIGH);                          // Pulso en alto
    delayMicroseconds(half_delay);                          // Espera media parte
    digitalWrite(PIN_PULSO, LOW);                           // Pulso en bajo
    delayMicroseconds(half_delay);                          // Espera la otra mitad

    pulsos_dados++;                                         // Cuenta el pulso

    // 6.5 Leer el ToF cada INTERVALO_TOF_MS, no en cada pulso para no saturar el I2C
    if (millis() - tiempo_ultimo_tof >= INTERVALO_TOF_MS) {  // Si ya toca leer
      tiempo_ultimo_tof = millis();                         // Guarda cuándo se leyó

      float pos_actual = leerSensorLongitudinal();          // Posición actual

      if (pos_actual != -1.0) {                             // Si la lectura es buena
        float distancia_recorrida = abs(pos_actual - pos_inicial);  // Cuánto se ha movido

        // Mensaje de avance cada 5000 pulsos aprox.
        if (pulsos_dados % 5000 == 0) {                     // Cada 5000 pulsos
          Serial.print("[KINCO] Pulsos: "); Serial.print(pulsos_dados);  // Imprime los pulsos
          Serial.print(" | ToF: "); Serial.print(pos_actual);  // Imprime la posición
          Serial.print(" mm | Recorrido: "); Serial.print(distancia_recorrida, 1);  // Imprime lo recorrido
          Serial.print(" / "); Serial.print(distancia_mm); Serial.println(" mm");  // Contra lo pedido
        }

        // 6.6 Condición principal: detener cuando el ToF detecta el objetivo
        if (distancia_recorrida >= (distancia_mm - TOLERANCIA_MM)) {  // Si ya llegó a lo pedido
          objetivo_alcanzado = true;                        // Termina el movimiento
          Serial.println("Objetivo alcanzado (detectado por sensor ToF).");  // Avisa
          Serial.print("   Recorrido final: "); Serial.print(distancia_recorrida, 1);  // Imprime lo que se movió
          Serial.print(" mm (objetivo: "); Serial.print(distancia_mm); Serial.println(" mm)");  // Y lo que se pidió
        }

        // 6.6b Límite de recorrido: detener en el extremo cefálico o caudal
        if ((haciaCaudal && pos_actual >= LIMITE_CAUDAL_MAX_MM) ||  // Llegó al extremo caudal
            (!haciaCaudal && pos_actual <= LIMITE_CEFALICO_MIN_MM)) {  // O al extremo cefálico
          objetivo_alcanzado = true;                        // Se detiene
        }
      }

      // 6.7 Mantener viva la conexión MQTT y publicar telemetría
      publicarShadow();                                     // Manda el estado a AWS
      client.loop();                                        // Atiende los mensajes MQTT
    }
  }

  // 7. Diagnóstico final de la parada
  if (objetivo_alcanzado) {                                 // Si paró porque llegó
    Serial.println("Parada por sensor ToF (lazo cerrado).");  // Avisa
  } else if (pulsos_dados >= total_pulsos_max) {            // Si paró por pulsos
    Serial.println("Parada por límite de pulsos (el ToF no detectó el objetivo).");  // Avisa
    Serial.print("   Pulsos dados: "); Serial.println(pulsos_dados);  // Imprime los pulsos dados
  } else if (paroActivo) {                                  // Si paró por emergencia
    Serial.println("Parada por PARO DE EMERGENCIA.");       // Avisa
  } else {                                                  // Si no, fue por tiempo
    Serial.println("Parada por TIMEOUT de seguridad.");     // Avisa
  }

  // 8. Detener el motor
  digitalWrite(PIN_FRENO, RELE_APAGADO);                    // Pone el freno
  digitalWrite(RELE_ENABLE, RELE_APAGADO);                  // Deshabilita el driver
  mesaEnMovimiento = false;                                 // La mesa queda quieta
  publicarShadow(true);                                     // Manda el estado final a AWS

  Serial.println("Arco longitudinal completo.\n");          // Fin del movimiento
}

void procesarComandoAWS(char* topic, byte* payload, unsigned int length) {  // Se ejecuta cuando llega un comando de AWS
  StaticJsonDocument<256> doc;                              // Documento para leer el JSON
  deserializeJson(doc, payload, length);                    // Convierte el mensaje a JSON

  const char* instancia = doc["comando"];                   // Qué movimiento se pidió
  float cantidad_a_mover = abs((float)doc["valor"]);        // Cuánto se debe mover, siempre positivo

  if (strcmp(instancia, "paro") == 0) {                     // Si es paro de emergencia
    ejecutarParoEmergencia();                               // Detiene todo
    return;                                                 // No hace nada más
  }

  paroActivo = false;                                       // Un comando nuevo quita el paro

  if (cantidad_a_mover <= 0.5) {                            // Si no dijeron valor
    cantidad_a_mover = VALOR_UN_POCO_MM;                    // Usa el valor de "un poco"
  }

  Serial.printf("\n[COMANDO] Accion: %s | Valor: %.1f\n", instancia, cantidad_a_mover);  // Muestra el comando recibido

  unsigned long tiempo_inicio = millis();                   // Momento en que arranca el comando
  bool limite_alcanzado = false;                            // Si ya se llegó a lo pedido o a un límite

  mesaEnMovimiento = true;                                  // La mesa empieza a moverse

  // ALTURA
  if (strcmp(instancia, "subir") == 0 || strcmp(instancia, "bajar") == 0) {  // Si es subir o bajar
    int pin_activo = (strcmp(instancia, "subir") == 0) ? sol_Subir : sol_Bajar;  // Escoge el relé
    float pos_inicial_altura = leerSensorAltura();          // Altura antes de mover

    if(pos_inicial_altura == -1.0) {                        // Si el sensor dio error
      delay(10);                                            // Espera un poco
      pos_inicial_altura = leerSensorAltura();              // Vuelve a leer
    }

    // Límite de recorrido: si ya está en el extremo, no se activa el relé
    bool subiendo = (strcmp(instancia, "subir") == 0);      // Si va hacia arriba
    if (pos_inicial_altura != -1.0 &&                       // Si hay lectura
        ((subiendo && pos_inicial_altura >= LIMITE_ALTURA_MAX_MM) ||  // y ya está arriba del todo
         (!subiendo && pos_inicial_altura <= LIMITE_ALTURA_MIN_MM))) {  // o abajo del todo
      limite_alcanzado = true;                              // No se mueve
    }

    if (!limite_alcanzado) digitalWrite(pin_activo, RELE_ENCENDIDO);  // Prende el relé si se puede mover

    while (!limite_alcanzado) {                             // Se mueve hasta llegar
      if (paroActivo) {                                     // Si pidieron paro
        Serial.println("PARO detectado en altura. Abortando.");  // Avisa
        break;                                              // Sale
      }

      float pos_actual_altura = leerSensorAltura();         // Altura actual

      // Límite de recorrido: detener en la altura máxima o mínima
      if (pos_actual_altura != -1.0 &&                      // Si hay lectura
          ((subiendo && pos_actual_altura >= LIMITE_ALTURA_MAX_MM) ||  // y llegó arriba
           (!subiendo && pos_actual_altura <= LIMITE_ALTURA_MIN_MM))) {  // o llegó abajo
        limite_alcanzado = true;                            // Se detiene
      }

      if(pos_actual_altura != -1.0 && pos_inicial_altura != -1.0) {  // Si las dos lecturas son buenas
        float distancia_recorrida_real = abs(pos_actual_altura - pos_inicial_altura);  // Cuánto ha subido o bajado
        if (distancia_recorrida_real >= (cantidad_a_mover - TOLERANCIA_MM)) {  // Si ya llegó a lo pedido
          limite_alcanzado = true;                          // Se detiene
        }
      }

      if (millis() - tiempo_inicio > TIMEOUT_SEGURIDAD_MS) {  // Si ya pasaron 25 s
        Serial.println("Error: Timeout actuador altura.");  // Avisa
        break;                                              // Sale
      }

      actualizarMonitor();                                  // Imprime los sensores
      publicarShadow();                                     // Manda el estado a AWS
      client.loop();                                        // Atiende MQTT
      delay(30);                                            // Pausa corta
    }
    digitalWrite(pin_activo, RELE_APAGADO);                 // Apaga el relé
  }

  // CEFALOCAUDAL
  else if (strcmp(instancia, "cefalico") == 0 || strcmp(instancia, "caudal") == 0) {  // Si es cefálico o caudal
    bool haciaCaudal = (strcmp(instancia, "caudal") == 0);  // Si va hacia los pies
    moverDistancia_mm(cantidad_a_mover, haciaCaudal);       // Mueve con el motor Kinco
    limite_alcanzado = true;                                // Movimiento terminado
  }

  // TRENDELENBURG
  else if (strcmp(instancia, "tren") == 0 || strcmp(instancia, "antitren") == 0) {  // Si es Trendelenburg o anti
    int pin_activo = (strcmp(instancia, "tren") == 0) ? sol_Tren : sol_AntiTren;  // Escoge el relé
    float pos_inicial = leerAnguloTren();                   // Ángulo antes de mover

    // Límite de recorrido: si ya está en el extremo, no se activa el relé
    bool haciaTren = (strcmp(instancia, "tren") == 0);      // Si va hacia Trendelenburg
    if ((haciaTren && pos_inicial >= LIMITE_TREN_MAX_GRADOS) ||  // Ya está en el máximo
        (!haciaTren && pos_inicial <= LIMITE_ANTITREN_MIN_GRADOS)) {  // o en el mínimo
      limite_alcanzado = true;                              // No se mueve
    }

    if (!limite_alcanzado) digitalWrite(pin_activo, RELE_ENCENDIDO);  // Prende el relé si se puede mover
    while (!limite_alcanzado && (millis() - tiempo_inicio < TIMEOUT_SEGURIDAD_MS)) {  // Se mueve hasta llegar o hasta 25 s
      if (paroActivo) {                                     // Si pidieron paro
        Serial.println("PARO detectado en Trendelenburg. Abortando.");  // Avisa
        break;                                              // Sale
      }

      float pos_actual = leerAnguloTren();                  // Ángulo actual

      // Límite de recorrido: detener en 26° (Tren) o -5,4° (Anti Tren)
      if ((haciaTren && pos_actual >= LIMITE_TREN_MAX_GRADOS) ||  // Llegó al máximo
          (!haciaTren && pos_actual <= LIMITE_ANTITREN_MIN_GRADOS)) {  // o al mínimo
        limite_alcanzado = true;                            // Se detiene
      }
      float distancia_recorrida = abs(pos_actual - pos_inicial);  // Grados que se ha movido

      if (distancia_recorrida >= (cantidad_a_mover - TOLERANCIA_GRADOS)) {  // Si ya llegó a lo pedido
        limite_alcanzado = true;                            // Se detiene
      }
      actualizarMonitor();                                  // Imprime los sensores
      publicarShadow();                                     // Manda el estado a AWS
      client.loop(); delay(10);                             // Atiende MQTT y hace una pausa corta
    }
    digitalWrite(pin_activo, RELE_APAGADO);                 // Apaga el relé
  }

  // TILT LATERAL
  else if (strcmp(instancia, "tilt_der") == 0 || strcmp(instancia, "tilt_izq") == 0) {  // Si es tilt derecho o izquierdo
    int pin_activo = (strcmp(instancia, "tilt_der") == 0) ? sol_TiltIzq : sol_TiltDer;  // Escoge el relé (van cruzados por el montaje)
    float pos_inicial = leerAnguloTilt();                   // Ángulo antes de mover

    // Límite de recorrido: si ya está en el extremo, no se activa el relé
    bool haciaIzquierda = (strcmp(instancia, "tilt_izq") == 0);  // Si va hacia la izquierda
    if ((haciaIzquierda && pos_inicial >= LIMITE_TILT_IZQ_MAX_GRADOS) ||  // Ya está al máximo izquierdo
        (!haciaIzquierda && pos_inicial <= LIMITE_TILT_DER_MIN_GRADOS)) {  // o al máximo derecho
      limite_alcanzado = true;                              // No se mueve
    }

    if (!limite_alcanzado) digitalWrite(pin_activo, RELE_ENCENDIDO);  // Prende el relé si se puede mover
    while (!limite_alcanzado && (millis() - tiempo_inicio < TIMEOUT_SEGURIDAD_MS)) {  // Se mueve hasta llegar o hasta 25 s
      if (paroActivo) {                                     // Si pidieron paro
        Serial.println("PARO detectado en Tilt. Abortando.");  // Avisa
        break;                                              // Sale
      }

      float pos_actual = leerAnguloTilt();                  // Ángulo actual

      // Límite de recorrido: detener en 18° (izquierdo) o -22° (derecho)
      if ((haciaIzquierda && pos_actual >= LIMITE_TILT_IZQ_MAX_GRADOS) ||  // Llegó al máximo izquierdo
          (!haciaIzquierda && pos_actual <= LIMITE_TILT_DER_MIN_GRADOS)) {  // o al máximo derecho
        limite_alcanzado = true;                            // Se detiene
      }
      float distancia_recorrida = abs(pos_actual - pos_inicial);  // Grados que se ha movido

      if (distancia_recorrida >= (cantidad_a_mover - TOLERANCIA_GRADOS)) {  // Si ya llegó a lo pedido
        limite_alcanzado = true;                            // Se detiene
      }
      actualizarMonitor();                                  // Imprime los sensores
      publicarShadow();                                     // Manda el estado a AWS
      client.loop(); delay(10);                             // Atiende MQTT y hace una pausa corta
    }
    digitalWrite(pin_activo, RELE_APAGADO);                 // Apaga el relé
  }

  // FOWLER
  else if (strcmp(instancia, "fowler") == 0 || strcmp(instancia, "bajar_fowler") == 0) {  // Si es subir o bajar el espaldar
    int pin_activo = (strcmp(instancia, "fowler") == 0) ? sol_FowlerSubir : sol_FowlerBajar;  // Escoge el relé
    float pos_inicial = leerAnguloFowler();                 // Ángulo antes de mover

    // Límite de recorrido: si ya está en el extremo, no se activa el relé
    bool subiendoFowler = (strcmp(instancia, "fowler") == 0);  // Si va hacia arriba
    if ((subiendoFowler && pos_inicial <= LIMITE_FOWLER_SUBIR_GRADOS) ||  // Ya está arriba del todo
        (!subiendoFowler && pos_inicial >= LIMITE_FOWLER_BAJAR_GRADOS)) {  // o abajo del todo
      limite_alcanzado = true;                              // No se mueve
    }

    if (!limite_alcanzado) digitalWrite(pin_activo, RELE_ENCENDIDO);  // Prende el relé si se puede mover
    while (!limite_alcanzado && (millis() - tiempo_inicio < TIMEOUT_SEGURIDAD_MS)) {  // Se mueve hasta llegar o hasta 25 s
      if (paroActivo) {                                     // Si pidieron paro
        Serial.println("PARO detectado en Fowler. Abortando.");  // Avisa
        break;                                              // Sale
      }

      float pos_actual = leerAnguloFowler();                // Ángulo actual

      // Límite de recorrido: detener en -19,6° (subir) o 28,3° (bajar)
      if ((subiendoFowler && pos_actual <= LIMITE_FOWLER_SUBIR_GRADOS) ||  // Llegó arriba
          (!subiendoFowler && pos_actual >= LIMITE_FOWLER_BAJAR_GRADOS)) {  // o llegó abajo
        limite_alcanzado = true;                            // Se detiene
      }
      float distancia_recorrida = abs(pos_actual - pos_inicial);  // Grados que se ha movido

      if (distancia_recorrida >= (cantidad_a_mover - TOLERANCIA_GRADOS)) {  // Si ya llegó a lo pedido
        limite_alcanzado = true;                            // Se detiene
      }
      actualizarMonitor();                                  // Imprime los sensores
      publicarShadow();                                     // Manda el estado a AWS
      client.loop(); delay(10);                             // Atiende MQTT y hace una pausa corta
    }
    digitalWrite(pin_activo, RELE_APAGADO);                 // Apaga el relé
  }

  liberarTodosLosSolenoides();                              // Por seguridad apaga todo al terminar
  mesaEnMovimiento = false;                                 // La mesa queda quieta
  publicarShadow(true);                                     // Manda el estado final a AWS
}

void conectarAWS() {                                        // Conecta al WiFi y luego a AWS IoT Core
  WiFi.setSleep(false);                                     // Evita que el WiFi se duerma, así responde más rápido
  WiFi.mode(WIFI_STA);                                      // La ESP32 se conecta como cliente a la red
  WiFi.disconnect();                                        // Corta cualquier conexión anterior
  delay(100);                                               // Espera corta

  Serial.print("[WiFi] Conectando a ");                     // Mensaje de conexión
  Serial.print(WIFI_SSID);                                  // Muestra el nombre de la red
  WiFi.begin(WIFI_SSID, WIFI_PASS);                         // Intenta conectarse
  while (WiFi.status() != WL_CONNECTED) {                   // Espera hasta que conecte
    delay(500);                                             // Medio segundo entre intentos
    Serial.print(".");                                      // Punto mientras espera
  }
  Serial.println(" CONECTADO");                             // Ya hay WiFi
  Serial.print("[WiFi] Direccion IP: ");                    // Texto de la IP
  Serial.println(WiFi.localIP());                           // Muestra la IP de la ESP32

  net.setCACert(AWS_CERT_CA);                               // Carga el certificado de Amazon
  net.setCertificate(AWS_CERT_CRT);                         // Carga el certificado del dispositivo
  net.setPrivateKey(AWS_CERT_PRIVATE);                      // Carga la llave privada

  client.setServer(AWS_IOT_ENDPOINT, 8883);                 // Servidor de AWS IoT, puerto seguro de MQTT
  client.setCallback(procesarComandoAWS);                   // Función que atiende los comandos que llegan
  client.setBufferSize(1024);                               // Espacio para mensajes más grandes

  String clientId = "ESP32_Mesa_" + String(random(0xffff), HEX);  // Nombre de conexión con un número al azar

  Serial.print("[MQTT] Conectando a AWS IoT Core");         // Mensaje de conexión a AWS
  while (!client.connect(clientId.c_str())) {               // Intenta hasta que conecte
    delay(2000);                                            // Dos segundos entre intentos
    Serial.print(".");                                      // Punto mientras espera
  }

  Serial.println(" CONECTADO");                             // Ya está conectado a AWS
  Serial.print("[MQTT] Suscrito al topico: ");              // Texto del tópico
  Serial.println(AWS_IOT_TOPIC);                            // Muestra el tópico
  Serial.println("[MQTT] Sistema listo\n");                 // Todo listo

  client.subscribe(AWS_IOT_TOPIC);                          // Empieza a escuchar los comandos
}

// SETUP: PRIMERO WiFi Y AWS, LUEGO SENSORES
void setup() {                                              // Se ejecuta una sola vez al encender
  Serial.begin(115200);                                     // Inicia el monitor serial
  delay(3000);                                              // Espera 3 s para alcanzar a abrir el monitor

  // PASO 1: Configurar primero los pines de los relés (seguridad)
  pinMode(sol_Subir, OUTPUT); pinMode(sol_Bajar, OUTPUT);   // Relés de altura como salida
  pinMode(sol_Tren, OUTPUT); pinMode(sol_AntiTren, OUTPUT);  // Relés de Trendelenburg como salida
  pinMode(sol_TiltDer, OUTPUT); pinMode(sol_TiltIzq, OUTPUT);  // Relés de tilt como salida
  pinMode(sol_FowlerSubir, OUTPUT); pinMode(sol_FowlerBajar, OUTPUT);  // Relés de Fowler como salida
  pinMode(sol_Cefalico, OUTPUT); pinMode(sol_Caudal, OUTPUT);  // Relés de cefalocaudal como salida

  pinMode(RELE_ENABLE, OUTPUT);                             // Relé de habilitación del driver
  pinMode(PIN_FRENO, OUTPUT);                               // Relé del freno
  pinMode(PIN_PULSO, OUTPUT);                               // Pin de pulsos
  pinMode(PIN_DIRECCION, OUTPUT);                           // Pin de dirección

  liberarTodosLosSolenoides();                              // Arranca con todo apagado
  digitalWrite(PIN_PULSO, LOW);                             // Sin pulsos
  digitalWrite(PIN_DIRECCION, LOW);                         // Dirección en bajo

  // PASO 2: Conectar primero WiFi y AWS IoT
  conectarAWS();                                            // Conecta a internet y a AWS

  // PASO 3: Iniciar después los buses I2C y los sensores
  Wire.begin(I2C0_SDA, I2C0_SCL);                           // Inicia el bus I2C de los MPU6050
  Wire1.begin(I2C1_SDA, I2C1_SCL);                          // Inicia el bus I2C de los ToF
  Wire1.setClock(100000);                                   // Velocidad del bus de los ToF, 100 kHz

  mpuBase.initialize();                                     // Arranca el MPU de la base
  if (mpuBase.testConnection()) {                           // Si responde
    Serial.println("MPU Base OK");                          // Avisa
    mpuBase.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);     // Rango de 2 g, más preciso para inclinación
    mpuBase.setFullScaleGyroRange(MPU6050_GYRO_FS_250);     // Rango del giroscopio de 250 °/s
    mpuBase.setDLPFMode(MPU6050_DLPF_BW_20);                // Filtro interno para quitar vibración
  }

  mpuEspalda.initialize();                                  // Arranca el MPU del espaldar
  if (mpuEspalda.testConnection()) {                        // Si responde
    Serial.println("MPU Espalda OK");                       // Avisa
    mpuEspalda.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);  // Rango de 2 g
    mpuEspalda.setFullScaleGyroRange(MPU6050_GYRO_FS_250);  // Rango del giroscopio de 250 °/s
    mpuEspalda.setDLPFMode(MPU6050_DLPF_BW_20);             // Filtro interno para quitar vibración
  }

  pinMode(XSHUT_ALTURA, OUTPUT);                            // Pin XSHUT del ToF de altura como salida
  pinMode(XSHUT_LONG, OUTPUT);                              // Pin XSHUT del ToF longitudinal como salida

  digitalWrite(XSHUT_ALTURA, LOW);                          // Apaga el ToF de altura
  digitalWrite(XSHUT_LONG, LOW);                            // Apaga el ToF longitudinal
  delay(100);                                               // Espera para que los dos queden apagados

  // ToF ALTURA: modo largo alcance y más tiempo de medida
  digitalWrite(XSHUT_ALTURA, HIGH);                         // Prende solo el ToF de altura
  delay(10);                                                // Espera a que arranque
  tofAltura.setBus(&Wire1);                                 // Lo pone en el segundo bus I2C
  if(tofAltura.init(true)) {                                // Si arranca bien (modo de 2,8 V, sirve para módulos de 2,8 a 3,3 V)
    tofAltura.setAddress(0x30);                             // Le cambia la dirección para no chocar con el otro ToF
    tofAltura.setTimeout(500);                              // Tiempo máximo de espera
    tofAltura.setSignalRateLimit(0.1);                      // Acepta señales más débiles (objetos lejanos)
    tofAltura.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);  // Pulsos más largos para largo alcance
    tofAltura.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);  // Pulsos más largos para largo alcance
    tofAltura.setMeasurementTimingBudget(TOF_BUDGET_ALTURA_US);  // Tiempo de cada medida
    tofAltura.startContinuous();                            // Empieza a medir sin parar
  }

  // ToF LONGITUDINAL: modo normal (más exacto) y más tiempo de medida
  digitalWrite(XSHUT_LONG, HIGH);                           // Prende el ToF longitudinal
  delay(10);                                                // Espera a que arranque
  tofLongitudinal.setBus(&Wire1);                           // Lo pone en el segundo bus I2C
  if(tofLongitudinal.init(true)) {                          // Si arranca bien
    tofLongitudinal.setAddress(0x31);                       // Le pone otra dirección
    tofLongitudinal.setTimeout(500);                        // Tiempo máximo de espera
    tofLongitudinal.setSignalRateLimit(0.25);               // Límite normal de señal
    tofLongitudinal.setMeasurementTimingBudget(TOF_BUDGET_LONG_US);  // Tiempo de cada medida
    tofLongitudinal.startContinuous();                      // Empieza a medir sin parar
  }

  tiempoUltimoMovimiento = millis();                        // Arranca el conteo de movimiento

  for (int i = 0; i < 30; i++) {                            // Hace 30 lecturas para que los filtros se estabilicen
    leerAnguloTren();                                       // Lee Trendelenburg
    leerAnguloTilt();                                       // Lee tilt
    leerAnguloFowler();                                     // Lee Fowler
    leerSensorAltura();                                     // Lee la altura
    leerSensorLongitudinal();                               // Lee la posición longitudinal

    memAlt = filtroAltura;                                  // Guarda la altura de referencia
    memLong = filtroLongitudinal;                           // Guarda la posición longitudinal de referencia
    memTren = filtroTren;                                   // Guarda Trendelenburg de referencia
    memTilt = filtroTilt;                                   // Guarda tilt de referencia
    memFow = filtroEspalda;                                 // Guarda Fowler de referencia

    delay(15);                                              // Pausa corta entre lecturas
  }
  primeraLectura = false;                                   // Ya terminaron las primeras lecturas
}

void loop() {                                               // Se repite todo el tiempo
  if (!client.connected()) {                                // Si se perdió la conexión con AWS
    liberarTodosLosSolenoides();                            // Por seguridad apaga todo
    conectarAWS();                                          // Vuelve a conectar
  }

  client.loop();                                            // Atiende los mensajes de AWS
  actualizarMonitor();                                      // Imprime los sensores
  publicarShadow();                                         // Manda el estado a AWS
}
