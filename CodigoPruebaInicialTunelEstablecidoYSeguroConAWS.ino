// Importamos la librería para manejar conexiones Wi-Fi cifradas y seguras (HTTPS/TLS) en el ESP32
#include <WiFiClientSecure.h>
// Importamos la librería PubSubClient para permitir la comunicación mediante el protocolo MQTT
#include <PubSubClient.h>
// Importamos la librería principal de Wi-Fi del ESP32 para conectarnos a nuestra red local
#include <WiFi.h>

// 1. CREDENCIALES DE RED
// Definimos el nombre de la red Wi-Fi (SSID) y la contraseña a la que se conectará el ESP32
const char* WIFI_SSID = "vivo Y33s";
const char* WIFI_PASS = "Venom3y4";

// 2. ENDPOINT MQTT
// Dirección única del servidor de AWS IoT asignada a nuestra cuenta
const char* AWS_IOT_ENDPOINT = "a213cdp4lqcq2s-ats.iot.us-east-1.amazonaws.com"; 

// 3. CERTIFICADOS DE SEGURIDAD TLS 1.2
// Almacenamos en la memoria flash el certificado de la autoridad raíz de Amazon
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

// Certificado digital específico de nuestro ESP32 que lo identifica de forma única ante AWS IoT Core
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

// Llave criptográfica privada de nuestro dispositivo. Permite firmar la comunicación de manera segura
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

// 4. INSTANCIAS DE CONEXIÓN
// Creamos un objeto cliente seguro para gestionar las conexiones TLS y el cliente MQTT
WiFiClientSecure net;
PubSubClient client(net);

// 5. FUNCIÓN DE CONEXIÓN AL TÚNEL SEGURO
void conectarAWS() {
  // Desactivamos el modo de suspensión del Wi-Fi para asegurar una conexión estable
  WiFi.setSleep(false);
  // Iniciamos la conexión con la red Wi-Fi configurada
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("\n[WiFi] Conectando a red de soporte");
  
  // Bucle que se mantiene ejecutándose mientras el ESP32 logra conectarse al Wi-Fi
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println(" OK");

  // Cargamos en el cliente seguro los tres certificados necesarios para autenticarnos con AWS
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Configuramos el servidor MQTT indicando el Endpoint de AWS y el puerto seguro estándar (8883)
  client.setServer(AWS_IOT_ENDPOINT, 8883);

  Serial.print("[AWS] Negociando certificados TLS con Amazon...");
  // Generamos un identificador de cliente único
  String clientId = "ESP32_Mesa_" + String(random(0xffff), HEX);

  // Bucle de reintento que intenta establecer la conexión MQTT cifrada hasta lograrlo
  while (!client.connect(clientId.c_str())) {
    Serial.print(".");
    delay(1000);
  }
  
  // Mensaje de éxito que confirma que el protocolo de seguridad y el enlace están activos
  Serial.println("\n✅ TÚNEL ESTABLECIDO Y SEGURO.");
}

// 6. CONFIGURACIÓN INICIAL (SETUP)
void setup() {
  // Inicializamos el puerto serial para visualizar mensajes de diagnóstico
  Serial.begin(115200);
  // Llamamos a nuestra función para levantar el Wi-Fi y establecer el túnel seguro con AWS
  conectarAWS();
}

// 7. BUCLE PRINCIPAL (LOOP)
void loop() {
  // Verificamos de forma continua si la conexión MQTT sigue activa; si se pierde, la reconectamos
  if (!client.connected()) {
    conectarAWS(); 
  }
  // Mantenemos el proceso MQTT activo
  client.loop(); 
}