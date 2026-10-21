#include <WiFiClientSecure.h>                   // Conexión WiFi segura con certificados (TLS)
#include <PubSubClient.h>                       // Cliente MQTT para hablar con AWS IoT Core
#include <WiFi.h>                               // Librería para conectar la ESP32 al WiFi

// 1. CREDENCIALES DE RED
// Datos de la red WiFi, se cambian por los de la red que se vaya a usar
const char* WIFI_SSID = "NOMBRE_DE_SU_RED";     // Nombre de la red WiFi
const char* WIFI_PASS = "CONTRASENA_DE_SU_RED";  // Contraseña de la red WiFi

// 2. ENDPOINT MQTT
const char* AWS_IOT_ENDPOINT = "SU_ENDPOINT-ats.iot.us-east-1.amazonaws.com";  // Dirección del servidor de AWS IoT de la cuenta

// 3. CERTIFICADOS DE SEGURIDAD TLS 1.2
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

// Certificado del dispositivo, lo identifica ante AWS IoT Core (archivo certificate.pem.crt)
// Es privado, se pega aquí solo en la copia que se carga a la ESP32
const char AWS_CERT_CRT[] PROGMEM = R"EOF(
PEGAR AQUI EL CERTIFICADO DEL DISPOSITIVO
)EOF";

// Llave privada del dispositivo (archivo private.pem.key)
// Nunca se publica, quien la tenga puede hacerse pasar por la mesa en AWS
const char AWS_CERT_PRIVATE[] PROGMEM = R"EOF(
PEGAR AQUI LA LLAVE PRIVADA DEL DISPOSITIVO
)EOF";

// 4. INSTANCIAS DE CONEXIÓN
WiFiClientSecure net;                           // Conexión segura que usa MQTT
PubSubClient client(net);                       // Cliente MQTT sobre la conexión segura

// 5. FUNCIÓN DE CONEXIÓN AL TÚNEL SEGURO
void conectarAWS() {                            // Conecta al WiFi y luego a AWS IoT Core
  WiFi.setSleep(false);                         // Evita que el WiFi se duerma, así la conexión es más estable
  WiFi.begin(WIFI_SSID, WIFI_PASS);             // Intenta conectarse a la red
  Serial.print("\n[WiFi] Conectando a red de soporte");  // Mensaje de conexión

  while (WiFi.status() != WL_CONNECTED) {       // Espera hasta que conecte
    delay(500);                                 // Medio segundo entre intentos
    Serial.print(".");                          // Punto mientras espera
  }
  Serial.println(" OK");                        // Ya hay WiFi

  net.setCACert(AWS_CERT_CA);                   // Carga el certificado de Amazon
  net.setCertificate(AWS_CERT_CRT);             // Carga el certificado del dispositivo
  net.setPrivateKey(AWS_CERT_PRIVATE);          // Carga la llave privada

  client.setServer(AWS_IOT_ENDPOINT, 8883);     // Servidor de AWS IoT, puerto seguro de MQTT

  Serial.print("[AWS] Negociando certificados TLS con Amazon...");  // Mensaje mientras valida los certificados
  String clientId = "ESP32_Mesa_" + String(random(0xffff), HEX);  // Nombre de conexión con un número al azar

  while (!client.connect(clientId.c_str())) {   // Intenta conectar hasta que lo logre
    Serial.print(".");                          // Punto mientras espera
    delay(1000);                                // Un segundo entre intentos
  }

  Serial.println("\nTÚNEL ESTABLECIDO Y SEGURO.");  // Ya quedó la conexión segura con AWS
}

// 6. CONFIGURACIÓN INICIAL (SETUP)
void setup() {                                  // Se ejecuta una sola vez al encender
  Serial.begin(115200);                         // Inicia el monitor serial
  conectarAWS();                                // Conecta al WiFi y a AWS
}

// 7. BUCLE PRINCIPAL (LOOP)
void loop() {                                   // Se repite todo el tiempo
  if (!client.connected()) {                    // Si se perdió la conexión con AWS
    conectarAWS();                              // Vuelve a conectar
  }
  client.loop();                                // Mantiene viva la conexión MQTT
}
