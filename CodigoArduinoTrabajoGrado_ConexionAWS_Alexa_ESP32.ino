#include <WiFiClientSecure.h>       // Librería para conexiones TLS/SSL seguras hacia AWS IoT
#include <PubSubClient.h>           // Librería cliente MQTT para publicar y suscribirse
#include <ArduinoJson.h>            // Librería para crear y parsear documentos JSON
#include <WiFi.h>                   // Librería base para el manejo del WiFi del ESP32
#include <Wire.h>                   // Librería para comunicación I2C
#include <Adafruit_MPU6050.h>       // Librería de Adafruit para el sensor MPU6050
#include <Adafruit_Sensor.h>        // Librería unificada de Adafruit para sensores
#include <VL53L0X.h>                // Librería de Pololu para el sensor láser VL53L0X

const char* WIFI_SSID = "vivo Y33s";    // Nombre de la red WiFi a la que se conecta
const char* WIFI_PASS = "Venom3y4";     // Contraseña de la red WiFi

const char* AWS_IOT_ENDPOINT      = "a213cdp4lqcq2s-ats.iot.us-east-1.amazonaws.com"; // Endpoint del broker AWS IoT Core
const char* AWS_IOT_TOPIC         = "quirofano/mesa/control"; // Tópico MQTT donde llegan los comandos de la Lambda
const char* AWS_IOT_SHADOW_UPDATE = "$aws/things/Mesa_Quirurgica/shadow/update"; // Tópico del Shadow para reportar estado

//Certificado raíz CA de Amazon (Amazon Root CA 1) almacenado en memoria flash
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

// Certificado del dispositivo (thing Mesa_Quirurgica) en memoria flash

const char AWS_CERT_CRT[] PROGMEM = R"EOF(  
-----BEGIN CERTIFICATE-----
MIIDWTCCAkGgAwIBAgIUcpj0QPeycSaks5QFW2iq1dzRBREwDQYJKoZIhvcNAQEL
BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g
SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTI2MDQzMDAzMjYw
NloXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0
ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL8lwtbridJRrAa58E+B
5vxsPwvbsoVA8cMy8+cspVVPhFdZWuIpISIpxLIeDq33EdU4R0dMW22kzO9TjmJw
SNLpBWICsJzrdm6Z3adCGxusEXmJhXMNgWp/tdg7oorGz7fQ8SRk0XRSQo8t4s4S
n0IVZwvfR81cx0o8NNQanrF2/vgYGGy2eUs2FmtABwzkAJOhcXBJjt8eX4rEQmnF
4Cr+0z/LmYzD1DlVxPBFS9xI7BUx/hs1SIuROKW81bWn2iA9PYvkfWKotLOmTqoB
7YG9b9frV6AS06swNqnmpRNZnNfyGelIduaUorjf3FXDea4w0uw8lka9dzr19Qj+
/20CAwEAAaNgMF4wHwYDVR0jBBgwFoAUCuC/VJmV3Tet7Ot4ZshitfpIFFIwHQYD
VR0OBBYEFJzzE62GM0OFSB38VOaZG69AzT4IMAwGA1UdEwEB/wQCMAAwDgYDVR0P
AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQCrsKTjFOYDPagx1CTnbI4Q9pzw
3EH5k4RGYnFGJG77ZcK2KHRN5QdqIZzUozC+ObV7IkSUHInWXqlmmDPFtMd4y7I6
4UIB1G3wKOcaxjqo2Nrv1mj3nTuKovQfnBc14K/Xga4yQXHxax1/XMoZGuOsmDIo
19UNxvdESwgJvzqu+rA31AC6SMCc+jPwm1Vdow3uHwI09Bj9RxbySlELBKugmKSz
/hEkKDROndxIjEFyH1MZ2WfdPhpkdmplvNnWzUPjso44y2rPV60cwtr5rOwQDey1
gydLH4w2bQ+Y7yDN4sy7WkeJGQldZfZaB+XjHwePsUdx4HOoExmi3PITtC8l
-----END CERTIFICATE-----
)EOF";

// Clave privada RSA del dispositivo en memoria flash
const char AWS_CERT_PRIVATE[] PROGMEM = R"EOF(  
-----BEGIN RSA PRIVATE KEY-----
MIIEowIBAAKCAQEAvyXC1uuJ0lGsBrnwT4Hm/Gw/C9uyhUDxwzLz5yylVU+EV1la
4ikhIinEsh4OrfcR1ThHR0xbbaTM71OOYnBI0ukFYgKwnOt2bpndp0IbG6wReYmF
cw2Ban+12DuiisbPt9DxJGTRdFJCjy3izhKfQhVnC99HzVzHSjw01BqesXb++BgY
bLZ5SzYWa0AHDOQAk6FxcEmO3x5fisRCacXgKv7TP8uZjMPUOVXE8EVL3EjsFTH+
GzVIi5E4pbzVtafaID09i+R9Yqi0s6ZOqgHtgb1v1+tXoBLTqzA2qealE1mc1/IZ
6Uh25pSiuN/cVcN5rjDS7DyWRr13OvX1CP7/bQIDAQABAoIBAGYbxN5MSn5YdJ3P
PAks6nhTVsZ94Pp/++X7VQFb/yH4bUjACeWgdPur9k/z8IGL5zHQc3sp1flXPMZo
CzqYor5QDjkSGMhqnARvG1Ha5WI948PZ67jPMR/0Au70xFp/47xc2ERJuv35Z2bW
6z6MAhkqF7bm2pNgiXElgk7/vWAstR/qGMHmf+LM30dI0kC47fe2lcmFMDmw32qz
y64COKZLxRKWBnrPxq5PDo9UPp8RaBbu1UGuqVez6ETf4fKoyRWjzOMQTYO9C9xA
Q0iMxN7TotjBg/xDMEvR8aSLW8hA0uVzVdVUXy0dlyoTs81BZHSZM6Yx+Ju3VSU8
7Sds5QECgYEA7Ho3sESxQij7rzTBvyAiFhRj0P4HgDq9Bk2iFp5lY71ri/lTksom
6df7ON8fiOLIDU7PbXqtAUfzMiZY67FxHkSkLdjSb1PxGAp21ocbisCUXjF8UuLB
crFA727xS8InTA5/Pt1sg/Yy86QU7/Dr+gGJ7RVhZkhEPMzcj9oDW8ECgYEAzu2H
MMtMFNnYKvFgLd3j2QuA1NbEaPVhCmoHtGYfbDouP14yZ45C9kjFdJlBNnbnVJrE
X9IAWQ9TDw4CpVO7GPiKjpXZSNO1lt/lXIiKwD1Ba/DbT1AkdGIkWwk6CgvVfaWB
qUQTUJe8KINx6Q9Mwzgb/VLmOSxjeY+rgGXRfq0CgYB6rNRi3t/N4JDgTVANJFBK
f/1gF8gwX0hcEr35WeZjHYrDLG6Mgc46FU9AK4+iZXM+ZBn7Q16ogXjKFVafvode
GmHJXueb1UIHJLaNh71j94+AZc+G+Bc+tnFnRHhswqW8MPEV3axDsN6xwfcSmjrw
kQfKNAM5Glg76lKpVYHUgQKBgQCPY07LxAQLasz9LNeN0PVDAFKcqdneNTKySbjf
C+j4UPSdGoPo+E50ZQbWfiAPQqOYH8q170oobJMMmpPAxl2etxMkLLgtW+64Bpvj
3AXxokDEMFl4AzvPnfY9Uw5xybN9ZoOLrE2KCirrE/KKIfz6RdY3WwuHULf5k1Gv
Hl7l3QKBgBcR8cb3VGU/my/7CZ73MQATRnUfbpyodi9pKtT84aeFFDbinTxpQXv0
vQljxTCBDsQvppEO8p4I+AzeNo7/HLmU9R5ISnAaEZM0eNp6ZP0rkpQdXwm87FdM
DhUdJ9VbSWrn+mQOlJKTBY4/19W7wEoA1Z2a3WwYljJU1l8v0hVQ
-----END RSA PRIVATE KEY-----
)EOF";

#define RELE_ENCENDIDO LOW      // Define que un LOW enciende el relé (lógica invertida)
#define RELE_APAGADO   HIGH     // Define que un HIGH apaga el relé (lógica invertida)

const int I2C0_SDA = 8;         // Pin SDA del bus I2C 0 (para los MPU6050)
const int I2C0_SCL = 9;         // Pin SCL del bus I2C 0 (para los MPU6050)
const int I2C1_SDA = 1;         // Pin SDA del bus I2C 1 (para los VL53L0X)
const int I2C1_SCL = 2;         // Pin SCL del bus I2C 1 (para los VL53L0X)

const int XSHUT_ALTURA = 16;    // Pin XSHUT para encender/apagar el sensor ToF de altura
const int XSHUT_LONG   = 17;    // Pin XSHUT para encender/apagar el sensor ToF longitudinal

const int sol_Subir       = 4;  // Pin del solenoide para subir la mesa
const int sol_Bajar       = 5;  // Pin del solenoide para bajar la mesa
const int sol_Tren        = 6;  // Pin del solenoide para Trendelenburg
const int sol_AntiTren    = 7;  // Pin del solenoide para Anti-Trendelenburg
const int sol_TiltDer     = 10; // Pin del solenoide para tilt derecho
const int sol_TiltIzq     = 11; // Pin del solenoide para tilt izquierdo
const int sol_FowlerSubir = 12; // Pin del solenoide para subir el respaldo (Fowler)
const int sol_FowlerBajar = 13; // Pin del solenoide para bajar el respaldo
const int sol_Cefalico    = 14; // Pin del solenoide para desplazamiento cefálico
const int sol_Caudal      = 15; // Pin del solenoide para desplazamiento caudal

const int PIN_PULSO     = 42;   // Pin de pulsos del driver Kinco FD5 (velocidad)
const int PIN_DIRECCION = 41;   // Pin de dirección del driver Kinco FD5
const int RELE_ENABLE   = 39;   // Pin del relé que da energía al driver Kinco FD5
const int PIN_FRENO     = 38;   // Pin del relé que libera el freno electromecánico de 24V

const float VALOR_UN_POCO_MM = 25.0;    // Distancia por defecto cuando se pide "un poco"
const float MS_POR_MILIMETRO_Z = 119.78; // Constante de milisegundos por milímetro en Z
const float AVANCE_POR_VUELTA_MM = 5.0;  // Milímetros que avanza el motor Kinco por vuelta
const long PULSOS_POR_VUELTA = 10000;    // Pulsos por vuelta del driver Kinco
const int VELOCIDAD_MICROSEGUNDOS = 50;  // Microsegundos de espera entre pulsos (velocidad)
const float TOLERANCIA_MM = 1.0;         // Tolerancia en milímetros para frenar el actuador
const float TOLERANCIA_GRADOS = 0.2;     // Tolerancia en grados para frenar el actuador
const unsigned long TIMEOUT_SEGURIDAD_MS = 25000; // Tiempo máximo que puede durar un movimiento

// --- FILTROS MAXIMIZADOS ---
bool primeraLectura = true;             // Bandera que indica si es la primera lectura de sensores
const float ALPHA_MPU = 0.03;           // Coeficiente del filtro EMA para los MPU
const float ALPHA_TOF = 0.05;           // Coeficiente del filtro EMA para los ToF

float filtroAltura = 0;                 // Valor filtrado de la altura
float filtroLongitudinal = 0;           // Valor filtrado de la posición longitudinal
float filtroTren = 0;                   // Valor filtrado del ángulo de Trendelenburg
float filtroTilt = 0;                   // Valor filtrado del ángulo de Tilt
float filtroEspalda = 0;                // Valor filtrado del ángulo del respaldo

float estableAltura = 0;                // Valor estable (redondeado) de la altura
float estableLongitudinal = 0;          // Valor estable de la posición longitudinal
float estableTren = 0;                  // Valor estable del ángulo de Trendelenburg
float estableTilt = 0;                  // Valor estable del ángulo de Tilt
float estableFowler = 0;                // Valor estable del ángulo de Fowler

// Variables de memoria para detectar saltos intencionales
unsigned long tiempoCheckMov = 0;       // Timestamp del último chequeo de movimiento
float memAlt = 0, memLong = 0, memTren = 0, memTilt = 0, memFow = 0; // Memorias de valores previos

unsigned long tiempoAnteriorMonitor = 0; // Timestamp del último print en el monitor serie
unsigned long tiempoUltimoShadow = 0;    // Timestamp de la última publicación al Shadow
unsigned long tiempoUltimoMovimiento = 0; // Timestamp del último movimiento detectado
bool mesaEnMovimiento = false;           // Bandera que indica si la mesa está en movimiento

// VARIABLE GLOBAL DE PARO DE EMERGENCIA

volatile bool paroActivo = false;        // Bandera global de paro de emergencia (volatile por ISR)

WiFiClientSecure net;                    // Cliente TLS seguro para la conexión a AWS
PubSubClient client(net);                // Cliente MQTT que usa la conexión TLS

Adafruit_MPU6050 mpuBase;                // Objeto del MPU6050 de la base de la mesa
Adafruit_MPU6050 mpuEspalda;             // Objeto del MPU6050 del respaldo

VL53L0X tofAltura;                       // Objeto del sensor ToF de altura (eje Z)
VL53L0X tofLongitudinal;                 // Objeto del sensor ToF longitudinal (eje X)

// FORWARD DECLARATIONS (prototipos)

// Necesarios porque ejecutarParoEmergencia() llama a publicarShadow() y
// publicarShadow() está definido más abajo en el archivo.

void publicarShadow(bool forzar);        // Prototipo de la función que publica al Shadow
float leerSensorAltura();                // Prototipo de la función que lee el ToF de altura
float leerSensorLongitudinal();          // Prototipo de la función que lee el ToF longitudinal
float leerAnguloTilt();                  // Prototipo de la función que lee el ángulo de Tilt
float leerAnguloTren();                  // Prototipo de la función que lee el ángulo de Tren
float leerAnguloFowler();                // Prototipo de la función que lee el ángulo de Fowler

// FUNCIÓN DE PARO DE EMERGENCIA

void ejecutarParoEmergencia() {          // Función que detiene todo el sistema de inmediato
  Serial.println("\n🚨🚨🚨 PARO DE EMERGENCIA 🚨🚨🚨"); // Imprime alerta en el monitor serie
  Serial.println("Deteniendo todos los actuadores..."); // Imprime mensaje descriptivo
  
  // 1. Apagar TODOS los solenoides inmediatamente
  digitalWrite(sol_Subir,       RELE_APAGADO); // Apaga el solenoide de subir
  digitalWrite(sol_Bajar,       RELE_APAGADO); // Apaga el solenoide de bajar
  digitalWrite(sol_Tren,        RELE_APAGADO); // Apaga el solenoide de Trendelenburg
  digitalWrite(sol_AntiTren,    RELE_APAGADO); // Apaga el solenoide de Anti-Trendelenburg
  digitalWrite(sol_TiltDer,     RELE_APAGADO); // Apaga el solenoide de tilt derecho
  digitalWrite(sol_TiltIzq,     RELE_APAGADO); // Apaga el solenoide de tilt izquierdo
  digitalWrite(sol_FowlerSubir, RELE_APAGADO); // Apaga el solenoide de subir respaldo
  digitalWrite(sol_FowlerBajar, RELE_APAGADO); // Apaga el solenoide de bajar respaldo
  digitalWrite(sol_Cefalico,    RELE_APAGADO); // Apaga el solenoide cefálico
  digitalWrite(sol_Caudal,      RELE_APAGADO); // Apaga el solenoide caudal
  
  // 2. Desactivar motor Kinco
  digitalWrite(RELE_ENABLE, RELE_APAGADO);     // Corta la energía al driver Kinco FD5
  
  // 3. Activar freno electromecánico
  digitalWrite(PIN_FRENO, RELE_APAGADO);       // Activa el freno electromecánico (cae el freno)
  
  // 4. Detener pulsos
  digitalWrite(PIN_PULSO, LOW);                // Detiene los pulsos al driver Kinco
  
  // 5. Actualizar flags
  mesaEnMovimiento = false;                    // Marca que la mesa ya no se mueve
  paroActivo = true;                           // Activa la bandera de paro activo
  
  // 6. Publicar estado actual al Shadow
  publicarShadow(true);                        // Publica el estado actual al Shadow inmediatamente
  
  Serial.println("✅ Sistema detenido. Esperando reactivación."); // Imprime confirmación
  Serial.println("=========================================\n"); // Imprime separador visual
}

// ---- FUNCIÓN DE DESPERTAR ESTRICTO ----
void detectarDespertarMatematico() {             // Función que detecta si hay movimiento real
  if (millis() - tiempoCheckMov >= 300) {        // Solo evalúa cada 300 ms
    float difAlt = abs(filtroAltura - memAlt);   // Diferencia de altura contra memoria
    float difLong = abs(filtroLongitudinal - memLong); // Diferencia longitudinal contra memoria
    float difTren = abs(filtroTren - memTren);   // Diferencia de Trendelenburg contra memoria
    float difTilt = abs(filtroTilt - memTilt);   // Diferencia de Tilt contra memoria
    float difFow = abs(filtroEspalda - memFow);  // Diferencia de Fowler contra memoria

    if (difAlt > 3.0 || difLong > 3.0 || difTren > 0.7 || difTilt > 0.7 || difFow > 0.7) { // Si algún eje supera umbral
      tiempoUltimoMovimiento = millis();         // Actualiza el timestamp del último movimiento
    }

    memAlt = filtroAltura;                       // Guarda altura en memoria
    memLong = filtroLongitudinal;                // Guarda longitudinal en memoria
    memTren = filtroTren;                        // Guarda Trendelenburg en memoria
    memTilt = filtroTilt;                        // Guarda Tilt en memoria
    memFow = filtroEspalda;                      // Guarda Fowler en memoria
    tiempoCheckMov = millis();                   // Actualiza el timestamp del chequeo
  }
}

bool actualizandoSensores() {                    // Función que decide si los sensores siguen activos
  detectarDespertarMatematico();                 // Ejecuta la detección de movimiento
  
  if (mesaEnMovimiento) {                        // Si la mesa está en movimiento
    tiempoUltimoMovimiento = millis();           // Resetea el contador de inactividad
  }
  
  return (millis() - tiempoUltimoMovimiento) < 1500; // Devuelve true si estuvo activo en 1.5s
}

float leerSensorAltura() {                       // Función que lee el ToF de altura
  uint16_t distancia = tofAltura.readRangeContinuousMillimeters(); // Lee la distancia del sensor
  if (tofAltura.timeoutOccurred() || distancia == 65535) return -1.0; // Si hay timeout o error devuelve -1
  
  if (primeraLectura) {                          // Si es la primera lectura
    filtroAltura = distancia;                    // Inicializa el filtro con el valor actual
    estableAltura = distancia;                   // Inicializa el valor estable
  }
  
  filtroAltura = (ALPHA_TOF * (float)distancia) + ((1.0 - ALPHA_TOF) * filtroAltura); // Aplica filtro EMA
  
  if (actualizandoSensores()) {                  // Si el sensor debe actualizarse
    estableAltura = round(filtroAltura);         // Redondea el valor filtrado a entero
  }
  
  return estableAltura;                          // Devuelve el valor estable de altura
}

float leerSensorLongitudinal() {                 // Función que lee el ToF longitudinal
  uint16_t distancia = tofLongitudinal.readRangeContinuousMillimeters(); // Lee la distancia
  if (tofLongitudinal.timeoutOccurred() || distancia == 65535) return -1.0; // Si hay error devuelve -1
  
  if (primeraLectura) {                          // Si es la primera lectura
    filtroLongitudinal = distancia;              // Inicializa el filtro
    estableLongitudinal = distancia;             // Inicializa el valor estable
  }
  
  filtroLongitudinal = (ALPHA_TOF * (float)distancia) + ((1.0 - ALPHA_TOF) * filtroLongitudinal); // Aplica filtro EMA
  
  if (actualizandoSensores()) {                  // Si el sensor debe actualizarse
    estableLongitudinal = round(filtroLongitudinal); // Redondea el valor filtrado
  }
  
  return estableLongitudinal;                    // Devuelve el valor estable longitudinal
}

float leerAnguloTren() {                         // Función que lee el ángulo de Trendelenburg
  sensors_event_t a, g, temp;                    // Estructuras para aceleración, giroscopio y temperatura
  mpuBase.getEvent(&a, &g, &temp);               // Lee los datos del MPU de la base
  float pitchBruto = atan2(a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / PI; // Calcula el pitch en grados
  
  if (primeraLectura) {                          // Si es la primera lectura
    filtroTren = pitchBruto;                     // Inicializa el filtro
    estableTren = pitchBruto;                    // Inicializa el valor estable
  }
  
  filtroTren = (ALPHA_MPU * pitchBruto) + ((1.0 - ALPHA_MPU) * filtroTren); // Aplica filtro EMA
  
  if (actualizandoSensores()) {                  // Si el sensor debe actualizarse
    estableTren = filtroTren;                    // Actualiza el valor estable
  }
  
  return estableTren;                            // Devuelve el ángulo estable de Tren
}

float leerAnguloTilt() {                         // Función que lee el ángulo de Tilt lateral
  sensors_event_t a, g, temp;                    // Estructuras para los datos del sensor
  mpuBase.getEvent(&a, &g, &temp);               // Lee los datos del MPU de la base
  float rollBruto = atan2(a.acceleration.y, sqrt(a.acceleration.x * a.acceleration.x + a.acceleration.z * a.acceleration.z)) * 180.0 / PI; // Calcula el roll en grados
  
  if (primeraLectura) {                          // Si es la primera lectura
    filtroTilt = rollBruto;                      // Inicializa el filtro
    estableTilt = rollBruto;                     // Inicializa el valor estable
  }
  
  filtroTilt = (ALPHA_MPU * rollBruto) + ((1.0 - ALPHA_MPU) * filtroTilt); // Aplica filtro EMA
  
  if (actualizandoSensores()) {                  // Si el sensor debe actualizarse
    estableTilt = filtroTilt;                    // Actualiza el valor estable
  }
  
  return estableTilt;                            // Devuelve el ángulo estable de Tilt
}

float leerAnguloFowler() {                       // Función que lee el ángulo de Fowler (respaldo)
  sensors_event_t a, g, temp;                    // Estructuras para los datos del sensor
  mpuEspalda.getEvent(&a, &g, &temp);            // Lee los datos del MPU del respaldo
  float pitchEspaldaBruto = atan2(a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / PI; // Calcula el pitch en grados
  
  if (primeraLectura) {                          // Si es la primera lectura
    filtroEspalda = pitchEspaldaBruto;           // Inicializa el filtro
    estableFowler = pitchEspaldaBruto;           // Inicializa el valor estable
  }
  
  filtroEspalda = (ALPHA_MPU * pitchEspaldaBruto) + ((1.0 - ALPHA_MPU) * filtroEspalda); // Aplica filtro EMA
  
  if (actualizandoSensores()) {                  // Si el sensor debe actualizarse
    estableFowler = filtroEspalda;               // Actualiza el valor estable
  }
  
  float trenActual = leerAnguloTren();           // Lee el ángulo de Trendelenburg actual
  return (estableFowler - trenActual);           // Devuelve la diferencia (ángulo real del respaldo)
}

void publicarShadow(bool forzar = false) {       // Función que publica el estado al Shadow IoT
  unsigned long intervalo = (mesaEnMovimiento || forzar) ? 120 : 350; // Intervalo: 120ms en movimiento, 350ms en reposo

  if (millis() - tiempoUltimoShadow >= intervalo) { // Si ya pasó el intervalo
    tiempoUltimoShadow = millis();               // Actualiza el timestamp de publicación

    StaticJsonDocument<512> shadowDoc;           // Crea un documento JSON estático de 512 bytes
    JsonObject reported = shadowDoc.createNestedObject("state").createNestedObject("reported"); // Crea estructura state.reported

    float altura = leerSensorAltura();           // Lee la altura actual
    float longitudinal = leerSensorLongitudinal(); // Lee la posición longitudinal actual
    
    if (altura != -1.0) reported["tofAltura"] = (int)altura; // Añade altura al JSON si es válida
    if (longitudinal != -1.0) reported["tofLong"] = (int)longitudinal; // Añade longitudinal si es válida
    
    reported["mpuTilt"] = round(leerAnguloTilt() * 10.0) / 10.0; // Añade Tilt redondeado a 1 decimal
    reported["mpuTren"] = round(leerAnguloTren() * 10.0) / 10.0; // Añade Tren redondeado a 1 decimal
    reported["mpuFowler"] = round(leerAnguloFowler() * 10.0) / 10.0; // Añade Fowler redondeado a 1 decimal

    char shadowBuffer[512];                      // Buffer de caracteres para el JSON serializado
    serializeJson(shadowDoc, shadowBuffer);      // Serializa el documento JSON al buffer
    
    client.publish(AWS_IOT_SHADOW_UPDATE, shadowBuffer); // Publica el JSON al tópico del Shadow
  }
}

void actualizarMonitor() {                       // Función que imprime telemetría en el monitor serie
  if (millis() - tiempoAnteriorMonitor >= 500) { // Cada 500 ms
    tiempoAnteriorMonitor = millis();            // Actualiza el timestamp del monitor
    
    float anguloTren = leerAnguloTren();         // Lee el ángulo de Tren
    float anguloTilt = leerAnguloTilt();         // Lee el ángulo de Tilt
    float anguloFowler = leerAnguloFowler();     // Lee el ángulo de Fowler
    float distZ = leerSensorAltura();            // Lee la distancia de altura
    float distX = leerSensorLongitudinal();      // Lee la distancia longitudinal
    
    Serial.print("-> [TELEMETRIA] Tren: "); Serial.print(anguloTren, 1); // Imprime Tren
    Serial.print("° | Tilt: "); Serial.print(anguloTilt, 1); // Imprime Tilt
    Serial.print("° | Fowler: "); Serial.print(anguloFowler, 1); // Imprime Fowler
    
    Serial.print("° | ToF Alt: ");               // Imprime etiqueta de ToF Altura
    if(distZ != -1.0) { Serial.print((int)distZ); Serial.print(" mm"); } // Imprime valor si es válido
    else { Serial.print("Error"); }              // Imprime "Error" si no es válido
    
    Serial.print(" | ToF Long: ");               // Imprime etiqueta de ToF Longitudinal
    if(distX != -1.0) { Serial.print((int)distX); Serial.println(" mm"); } // Imprime valor si es válido
    else { Serial.println("Error"); }            // Imprime "Error" si no es válido
  }
}

void liberarTodosLosSolenoides() {               // Función que apaga todos los solenoides y relés
  digitalWrite(sol_Subir, RELE_APAGADO);         // Apaga solenoide de subir
  digitalWrite(sol_Bajar, RELE_APAGADO);         // Apaga solenoide de bajar
  digitalWrite(sol_Tren, RELE_APAGADO);          // Apaga solenoide de Trendelenburg
  digitalWrite(sol_AntiTren, RELE_APAGADO);      // Apaga solenoide de Anti-Trendelenburg
  digitalWrite(sol_TiltDer, RELE_APAGADO);       // Apaga solenoide de tilt derecho
  digitalWrite(sol_TiltIzq, RELE_APAGADO);       // Apaga solenoide de tilt izquierdo
  digitalWrite(sol_FowlerSubir, RELE_APAGADO);   // Apaga solenoide de subir respaldo
  digitalWrite(sol_FowlerBajar, RELE_APAGADO);   // Apaga solenoide de bajar respaldo
  digitalWrite(sol_Cefalico, RELE_APAGADO);      // Apaga solenoide cefálico
  digitalWrite(sol_Caudal, RELE_APAGADO);        // Apaga solenoide caudal
  
  digitalWrite(PIN_FRENO, RELE_APAGADO);         // Activa el freno electromecánico
  digitalWrite(RELE_ENABLE, RELE_APAGADO);       // Corta la energía al driver Kinco
}

void moverDistancia_mm(float distancia_mm, bool haciaAdelante) { // Mueve el motor Kinco una distancia
  long total_pulsos = (distancia_mm / AVANCE_POR_VUELTA_MM) * PULSOS_POR_VUELTA; // Calcula los pulsos totales
  
  Serial.println("\n[MOTOR KINCO] Arrancando..."); // Imprime aviso de arranque
  Serial.print("Carrera total: "); Serial.print(distancia_mm); Serial.println(" mm."); // Imprime la distancia

  if (haciaAdelante) {                            // Si debe moverse hacia adelante
    digitalWrite(PIN_DIRECCION, HIGH);            // Configura la dirección hacia adelante
  } else {                                        // Si debe moverse hacia atrás
    digitalWrite(PIN_DIRECCION, LOW);             // Configura la dirección hacia atrás
  }
  delay(10);                                      // Pequeña pausa para estabilizar la dirección

  digitalWrite(RELE_ENABLE, RELE_ENCENDIDO);      // Da energía al driver Kinco
  delay(20);                                      // Espera a que el driver se estabilice
  digitalWrite(PIN_FRENO, RELE_ENCENDIDO);        // Libera el freno electromecánico
  delay(50);                                      // Espera a que el freno se libere completamente

  mesaEnMovimiento = true;                        // Marca la mesa como en movimiento

  for (long i = 0; i < total_pulsos; i++) {       // Bucle que genera los pulsos
    if (paroActivo) {                             // Si hay un paro de emergencia activo
      Serial.println("🚨 PARO detectado en bucle Kinco. Abortando."); // Avisa del aborto
      break;                                      // Sale del bucle inmediatamente
    }
    
    digitalWrite(PIN_PULSO, HIGH);                // Pone el pin de pulso en HIGH
    delayMicroseconds(VELOCIDAD_MICROSEGUNDOS);   // Espera el ancho del pulso
    digitalWrite(PIN_PULSO, LOW);                 // Pone el pin de pulso en LOW
    delayMicroseconds(VELOCIDAD_MICROSEGUNDOS);   // Espera entre pulsos
    
    if (i % 2000 == 0) {                          // Cada 2000 pulsos
        float monitorDist = leerSensorLongitudinal(); // Lee el sensor longitudinal
        publicarShadow();                         // Publica el estado al Shadow
        client.loop();                            // Mantiene viva la conexión MQTT
    }
  }

  digitalWrite(PIN_FRENO, RELE_APAGADO);          // Activa el freno electromecánico
  digitalWrite(RELE_ENABLE, RELE_APAGADO);        // Corta la energía al driver Kinco
  mesaEnMovimiento = false;                       // Marca la mesa como detenida
  publicarShadow(true);                           // Publica el estado final al Shadow

  Serial.println("Arco longitudinal completo.");  // Imprime mensaje de fin de movimiento
}

void procesarComandoAWS(char* topic, byte* payload, unsigned int length) { // Callback al recibir MQTT
  StaticJsonDocument<256> doc;                    // Documento JSON de 256 bytes para el comando
  deserializeJson(doc, payload, length);          // Parsea el payload JSON recibido
  
  const char* instancia = doc["comando"];         // Extrae el nombre del comando
  float cantidad_a_mover = abs((float)doc["valor"]); // Extrae el valor absoluto a mover
  
  if (strcmp(instancia, "paro") == 0) {           // Si el comando es "paro"
    ejecutarParoEmergencia();                     // Ejecuta el paro de emergencia
    return;                                       // Sale del callback inmediatamente
  }
  
  paroActivo = false;                             // Si no es paro, desactiva la bandera de paro
  
  if (cantidad_a_mover <= 0.5) {                  // Si el valor es muy pequeño
    cantidad_a_mover = VALOR_UN_POCO_MM;          // Lo reemplaza por el valor por defecto
  }
  
  Serial.printf("\n[COMANDO] Accion: %s | Valor: %.1f\n", instancia, cantidad_a_mover); // Imprime el comando
  
  unsigned long tiempo_inicio = millis();         // Guarda el timestamp de inicio
  bool limite_alcanzado = false;                  // Bandera que indica si se alcanzó el objetivo
  
  mesaEnMovimiento = true;                        // Marca la mesa como en movimiento

  if (strcmp(instancia, "subir") == 0 || strcmp(instancia, "bajar") == 0) { // Si es subir o bajar
    int pin_activo = (strcmp(instancia, "subir") == 0) ? sol_Subir : sol_Bajar; // Selecciona el solenoide
    float pos_inicial_altura = leerSensorAltura(); // Lee la posición inicial de altura
    
    if(pos_inicial_altura == -1.0) {              // Si la lectura falló
      delay(10);                                  // Espera un poco
      pos_inicial_altura = leerSensorAltura();    // Reintenta la lectura
    }
    
    digitalWrite(pin_activo, RELE_ENCENDIDO);     // Enciende el solenoide seleccionado
    
    while (!limite_alcanzado) {                   // Bucle hasta alcanzar el objetivo
      if (paroActivo) {                           // Si hay paro activo
        Serial.println("🚨 PARO detectado en altura. Abortando."); // Avisa del aborto
        break;                                    // Sale del bucle
      }
      
      float pos_actual_altura = leerSensorAltura(); // Lee la posición actual de altura
      
      if(pos_actual_altura != -1.0 && pos_inicial_altura != -1.0) { // Si ambas lecturas son válidas
        float distancia_recorrida_real = abs(pos_actual_altura - pos_inicial_altura); // Calcula la distancia recorrida
        if (distancia_recorrida_real >= (cantidad_a_mover - TOLERANCIA_MM)) { // Si llegó al objetivo
          limite_alcanzado = true;                // Marca el límite como alcanzado
        }
      }
      
      if (millis() - tiempo_inicio > TIMEOUT_SEGURIDAD_MS) { // Si supera el timeout
        Serial.println("Error: Timeout actuador altura."); // Avisa del timeout
        break;                                    // Sale del bucle
      }
      
      actualizarMonitor();                        // Actualiza el monitor serie
      publicarShadow();                           // Publica el estado al Shadow
      client.loop();                              // Mantiene viva la conexión MQTT
      delay(30);                                  // Pequeña pausa antes de la siguiente lectura
    }
    digitalWrite(pin_activo, RELE_APAGADO);       // Apaga el solenoide
  }

  else if (strcmp(instancia, "cefalico") == 0 || strcmp(instancia, "caudal") == 0) { // Si es cefálico o caudal
    bool haciaAdelante = (strcmp(instancia, "cefalico") == 0); // Determina la dirección
    moverDistancia_mm(cantidad_a_mover, haciaAdelante); // Llama a la función del motor Kinco
    limite_alcanzado = true;                      // Marca el límite como alcanzado
  }

  else if (strcmp(instancia, "tren") == 0 || strcmp(instancia, "antitren") == 0) { // Si es Trendelenburg
    int pin_activo = (strcmp(instancia, "tren") == 0) ? sol_Tren : sol_AntiTren; // Selecciona el solenoide
    float pos_inicial = leerAnguloTren();         // Lee el ángulo inicial
    
    digitalWrite(pin_activo, RELE_ENCENDIDO);     // Enciende el solenoide
    while (!limite_alcanzado && (millis() - tiempo_inicio < TIMEOUT_SEGURIDAD_MS)) { // Bucle controlado
      if (paroActivo) {                           // Si hay paro activo
        Serial.println("🚨 PARO detectado en Trendelenburg. Abortando."); // Avisa del aborto
        break;                                    // Sale del bucle
      }
      
      float pos_actual = leerAnguloTren();        // Lee el ángulo actual
      float distancia_recorrida = abs(pos_actual - pos_inicial); // Calcula la diferencia

      if (distancia_recorrida >= (cantidad_a_mover - TOLERANCIA_GRADOS)) { // Si alcanzó el objetivo
        limite_alcanzado = true;                  // Marca el límite como alcanzado
      }
      actualizarMonitor();                        // Actualiza el monitor serie
      publicarShadow();                           // Publica el estado al Shadow
      client.loop(); delay(10);                   // Mantiene la conexión MQTT y espera 10ms
    }
    digitalWrite(pin_activo, RELE_APAGADO);       // Apaga el solenoide
  }

  else if (strcmp(instancia, "tilt_der") == 0 || strcmp(instancia, "tilt_izq") == 0) { // Si es tilt
    int pin_activo = (strcmp(instancia, "tilt_der") == 0) ? sol_TiltDer : sol_TiltIzq; // Selecciona solenoide
    float pos_inicial = leerAnguloTilt();         // Lee el ángulo inicial de tilt
    
    digitalWrite(pin_activo, RELE_ENCENDIDO);     // Enciende el solenoide
    while (!limite_alcanzado && (millis() - tiempo_inicio < TIMEOUT_SEGURIDAD_MS)) { // Bucle controlado
      if (paroActivo) {                           // Si hay paro activo
        Serial.println("🚨 PARO detectado en Tilt. Abortando."); // Avisa del aborto
        break;                                    // Sale del bucle
      }
      
      float pos_actual = leerAnguloTilt();        // Lee el ángulo actual
      float distancia_recorrida = abs(pos_actual - pos_inicial); // Calcula la diferencia

      if (distancia_recorrida >= (cantidad_a_mover - TOLERANCIA_GRADOS)) { // Si alcanzó el objetivo
        limite_alcanzado = true;                  // Marca el límite como alcanzado
      }
      actualizarMonitor();                        // Actualiza el monitor serie
      publicarShadow();                           // Publica el estado al Shadow
      client.loop(); delay(10);                   // Mantiene la conexión MQTT y espera 10ms
    }
    digitalWrite(pin_activo, RELE_APAGADO);       // Apaga el solenoide
  }

  else if (strcmp(instancia, "fowler") == 0 || strcmp(instancia, "bajar_fowler") == 0) { // Si es Fowler
    int pin_activo = (strcmp(instancia, "fowler") == 0) ? sol_FowlerSubir : sol_FowlerBajar; // Selecciona solenoide
    float pos_inicial = leerAnguloFowler();       // Lee el ángulo inicial de Fowler
    
    digitalWrite(pin_activo, RELE_ENCENDIDO);     // Enciende el solenoide
    while (!limite_alcanzado && (millis() - tiempo_inicio < TIMEOUT_SEGURIDAD_MS)) { // Bucle controlado
      if (paroActivo) {                           // Si hay paro activo
        Serial.println("🚨 PARO detectado en Fowler. Abortando."); // Avisa del aborto
        break;                                    // Sale del bucle
      }
      
      float pos_actual = leerAnguloFowler();      // Lee el ángulo actual
      float distancia_recorrida = abs(pos_actual - pos_inicial); // Calcula la diferencia

      if (distancia_recorrida >= (cantidad_a_mover - TOLERANCIA_GRADOS)) { // Si alcanzó el objetivo
        limite_alcanzado = true;                  // Marca el límite como alcanzado
      }
      actualizarMonitor();                        // Actualiza el monitor serie
      publicarShadow();                           // Publica el estado al Shadow
      client.loop(); delay(10);                   // Mantiene la conexión MQTT y espera 10ms
    }
    digitalWrite(pin_activo, RELE_APAGADO);       // Apaga el solenoide
  }
  
  liberarTodosLosSolenoides();                    // Apaga todos los solenoides por seguridad
  mesaEnMovimiento = false;                       // Marca la mesa como detenida
  publicarShadow(true);                           // Publica el estado final al Shadow
}

void conectarAWS() {                              // Función que conecta el ESP32 a WiFi y a AWS IoT
  WiFi.setSleep(false);                           // Desactiva el modo de ahorro de energía WiFi
  WiFi.mode(WIFI_STA);                            // Configura el WiFi en modo estación
  WiFi.disconnect();                              // Desconecta de cualquier red previa
  delay(100);                                     // Pequeña pausa antes de reconectar

  WiFi.begin(WIFI_SSID, WIFI_PASS);               // Inicia la conexión a la red WiFi
  while (WiFi.status() != WL_CONNECTED) { delay(500); } // Espera hasta conectarse

  net.setCACert(AWS_CERT_CA);                     // Carga el certificado raíz CA en el cliente TLS
  net.setCertificate(AWS_CERT_CRT);               // Carga el certificado del dispositivo
  net.setPrivateKey(AWS_CERT_PRIVATE);            // Carga la clave privada del dispositivo
  
  client.setServer(AWS_IOT_ENDPOINT, 8883);       // Configura el servidor MQTT con puerto 8883
  client.setCallback(procesarComandoAWS);         // Registra el callback de comandos entrantes
  client.setBufferSize(1024);                     // Amplía el buffer MQTT a 1024 bytes

  String clientId = "ESP32_Mesa_" + String(random(0xffff), HEX); // Genera un ID único para el cliente
  
  while (!client.connect(clientId.c_str())) {     // Intenta conectar al broker AWS IoT
    delay(2000);                                  // Reintenta cada 2 segundos
  }
  
  client.subscribe(AWS_IOT_TOPIC);                // Se suscribe al tópico de comandos
}

void setup() {                                    // Función de configuración inicial del ESP32
  Serial.begin(115200);                           // Inicia la comunicación serie a 115200 baudios
  delay(3000);                                    // Espera 3 segundos para estabilizar el sistema
  
  Wire.begin(I2C0_SDA, I2C0_SCL);                 // Inicia el bus I2C 0 con sus pines
  Wire1.begin(I2C1_SDA, I2C1_SCL);                // Inicia el bus I2C 1 con sus pines
  Wire1.setClock(100000);                         // Configura el bus I2C 1 a 100 kHz
  
  pinMode(sol_Subir, OUTPUT); pinMode(sol_Bajar, OUTPUT); // Configura pines de solenoides de altura
  pinMode(sol_Tren, OUTPUT); pinMode(sol_AntiTren, OUTPUT); // Configura pines de solenoides de Trendelenburg
  pinMode(sol_TiltDer, OUTPUT); pinMode(sol_TiltIzq, OUTPUT); // Configura pines de solenoides de tilt
  pinMode(sol_FowlerSubir, OUTPUT); pinMode(sol_FowlerBajar, OUTPUT); // Configura pines de solenoides de Fowler
  pinMode(sol_Cefalico, OUTPUT); pinMode(sol_Caudal, OUTPUT); // Configura pines de solenoides longitudinales

  pinMode(RELE_ENABLE, OUTPUT);                   // Configura el pin del relé de habilitación Kinco
  pinMode(PIN_FRENO, OUTPUT);                     // Configura el pin del relé del freno
  pinMode(PIN_PULSO, OUTPUT);                     // Configura el pin de pulsos del Kinco
  pinMode(PIN_DIRECCION, OUTPUT);                 // Configura el pin de dirección del Kinco
  
  liberarTodosLosSolenoides();                    // Apaga todos los solenoides al arrancar
  digitalWrite(PIN_PULSO, LOW);                   // Pone el pin de pulso en LOW
  digitalWrite(PIN_DIRECCION, LOW);               // Pone el pin de dirección en LOW
  
  if (!mpuBase.begin(0x68, &Wire)) Serial.println("Error mpu base"); // Inicia el MPU base en dirección 0x68
  if (!mpuEspalda.begin(0x69, &Wire)) Serial.println("Error mpu espalda"); // Inicia el MPU espalda en 0x69

  pinMode(XSHUT_ALTURA, OUTPUT);                  // Configura el pin XSHUT del ToF de altura
  pinMode(XSHUT_LONG, OUTPUT);                    // Configura el pin XSHUT del ToF longitudinal
  
  digitalWrite(XSHUT_ALTURA, LOW);                // Apaga el ToF de altura
  digitalWrite(XSHUT_LONG, LOW);                  // Apaga el ToF longitudinal
  delay(100);                                     // Espera a que se descarguen
  
  digitalWrite(XSHUT_ALTURA, HIGH);               // Enciende el ToF de altura
  delay(10);                                      // Espera a que se inicialice
  tofAltura.setBus(&Wire1);                       // Asocia el ToF de altura al bus I2C 1
  if(tofAltura.init(false)) {                     // Inicia el ToF de altura
    tofAltura.setAddress(0x30);                   // Cambia su dirección a 0x30
    tofAltura.setTimeout(500);                    // Configura un timeout de 500ms
    tofAltura.startContinuous();                  // Inicia el modo de lectura continua
  }

  digitalWrite(XSHUT_LONG, HIGH);                 // Enciende el ToF longitudinal
  delay(10);                                      // Espera a que se inicialice
  tofLongitudinal.setBus(&Wire1);                 // Asocia el ToF longitudinal al bus I2C 1
  if(tofLongitudinal.init(false)) {               // Inicia el ToF longitudinal
    tofLongitudinal.setAddress(0x31);             // Cambia su dirección a 0x31
    tofLongitudinal.setTimeout(500);              // Configura un timeout de 500ms
    tofLongitudinal.startContinuous();            // Inicia el modo de lectura continua
  }
  
  tiempoUltimoMovimiento = millis();              // Inicializa el timestamp de movimiento

  for (int i = 0; i < 30; i++) {                  // Bucle de calibración inicial (30 iteraciones)
    leerAnguloTren();                             // Lee el ángulo de Tren
    leerAnguloTilt();                             // Lee el ángulo de Tilt
    leerAnguloFowler();                           // Lee el ángulo de Fowler
    leerSensorAltura();                           // Lee la altura
    leerSensorLongitudinal();                     // Lee la posición longitudinal
    
    memAlt = filtroAltura;                        // Guarda altura en memoria
    memLong = filtroLongitudinal;                 // Guarda longitudinal en memoria
    memTren = filtroTren;                         // Guarda Tren en memoria
    memTilt = filtroTilt;                         // Guarda Tilt en memoria
    memFow = filtroEspalda;                       // Guarda Fowler en memoria
    
    delay(15);                                    // Pequeña espera entre lecturas
  }
  primeraLectura = false;                         // Marca que ya no es la primera lectura

  conectarAWS();                                  // Conecta a WiFi y AWS IoT
}

void loop() {                                     // Bucle principal infinito del programa
  if (!client.connected()) {                      // Si se perdió la conexión MQTT
    liberarTodosLosSolenoides();                  // Apaga todos los solenoides por seguridad
    conectarAWS();                                // Reconecta a WiFi y AWS IoT
  }
  
  client.loop();                                  // Mantiene viva la conexión MQTT
  actualizarMonitor();                            // Actualiza el monitor serie
  publicarShadow();                               // Publica el estado al Shadow periódicamente
}