// Importamos el núcleo del SDK de Alexa que permite construir la skill
const Alexa = require('ask-sdk-core');
// Importamos el SDK de AWS para conectarnos a IoT Core y a otros servicios
const AWS = require('aws-sdk');
// Importamos el módulo HTTPS nativo de Node.js para configurar el agente de conexión
const https = require('https');

// Creamos un agente HTTPS con Keep-Alive para reutilizar la conexión TLS con AWS IoT
const sslAgent = new https.Agent({
    // Activamos Keep-Alive para mantener la conexión abierta entre peticiones
    keepAlive: true,
    // Permitimos hasta 50 sockets simultáneos en el pool de conexiones
    maxSockets: 50,
    // Exigimos que el certificado TLS sea válido (no permitimos certificados no verificados)
    rejectUnauthorized: true,
    // Mantenemos el socket vivo durante 30 segundos de inactividad antes de cerrarlo
    keepAliveMsecs: 30000
});

// Instanciamos el cliente de AWS IoT Data con la configuración de conexión
const iotData = new AWS.IotData({
    // Endpoint único de la cuenta en la región us-east-1 de AWS IoT Core
    endpoint: 'a213cdp4lqcq2s-ats.iot.us-east-1.amazonaws.com',
    // Región de AWS donde está alojado el thing y el broker MQTT
    region: 'us-east-1',
    // Opciones HTTP personalizadas para el cliente
    httpOptions: {
        // Asignamos el agente HTTPS con Keep-Alive ya configurado
        agent: sslAgent,
        // Tiempo máximo de espera de respuesta por petición (600 ms)
        timeout: 600,
        // Tiempo máximo de espera para establecer la conexión inicial (300 ms)
        connectTimeout: 300
    }
});

// Nombre del thing registrado en AWS IoT Core que representa la mesa quirúrgica
const THING_NAME = 'Mesa_Quirurgica';
// Token fijo usado en todas las directivas APL para que el Echo Show las aplique correctamente
const APL_TOKEN = 'MesaToken';
// Tópico MQTT por el que la Lambda publica comandos y el ESP32 los recibe
const TOPIC_CONTROL = 'quirofano/mesa/control';
// Texto corto que Alexa dice para dejar el micrófono abierto esperando otra orden
const REPROMPT_TEXT = 'Escuchando.';

// Paso por defecto en milímetros si el usuario no especifica cantidad al subir o bajar
const PASO_SEGURO_MM = 10;
// Paso por defecto en grados si el usuario no especifica ángulo en movimientos angulares
const PASO_SEGURO_GRADOS = 5;
// Paso por defecto en milímetros para desplazamientos longitudinales (cefálico/caudal)
const PASO_SEGURO_LONG = 25;

// URL base del bucket S3 público donde están alojadas las imágenes de la interfaz
const IMG_BASE = 'https://mesa-quirurgica-imagenes-946445280155.s3.us-east-1.amazonaws.com';

// Mapa que asocia cada comando con la caja de imagen y el archivo que debe mostrar
const MAPA_IMAGENES = {
    // Comando "subir" → caja imagenToF con archivo subir.png
    subir:        { id: 'imagenToF',    file: 'subir.png' },
    // Comando "bajar" → caja imagenToF con archivo bajar.png
    bajar:        { id: 'imagenToF',    file: 'bajar.png' },
    // Comando "tren" → caja imagenTren con archivo tren.png
    tren:         { id: 'imagenTren',   file: 'tren.png' },
    // Comando "antitren" → caja imagenTren con archivo antitren.png
    antitren:     { id: 'imagenTren',   file: 'antitren.png' },
    // Comando "tilt_der" → caja imagenTilt con archivo tilt_der.png
    tilt_der:     { id: 'imagenTilt',   file: 'tilt_der.png' },
    // Comando "tilt_izq" → caja imagenTilt con archivo tilt_izq.png
    tilt_izq:     { id: 'imagenTilt',   file: 'tilt_izq.png' },
    // Comando "fowler" → caja imagenFowler con archivo fowler.png
    fowler:       { id: 'imagenFowler', file: 'fowler.png' },
    // Comando "bajar_fowler" → caja imagenFowler con archivo fowler.png (misma imagen)
    bajar_fowler: { id: 'imagenFowler', file: 'fowler.png' },
    // Comando "cefalico" → caja imagenLong con archivo cefalico.png
    cefalico:     { id: 'imagenLong',   file: 'cefalico.png' },
    // Comando "caudal" → caja imagenLong con archivo caudal.png
    caudal:       { id: 'imagenLong',   file: 'caudal.png' }
};

// Lista de las 5 cajas de imágenes con su imagen por defecto (para resetear todas)
const IMAGENES_POR_DEFECTO = [
    // Caja de altura con subir.png como imagen por defecto
    { id: 'imagenToF',    file: 'subir.png' },
    // Caja de tilt con tilt_der.png como imagen por defecto
    { id: 'imagenTilt',   file: 'tilt_der.png' },
    // Caja de trendelenburg con tren.png como imagen por defecto
    { id: 'imagenTren',   file: 'tren.png' },
    // Caja de fowler con fowler.png como imagen por defecto
    { id: 'imagenFowler', file: 'fowler.png' },
    // Caja de longitudinal con cefalico.png como imagen por defecto
    { id: 'imagenLong',   file: 'cefalico.png' }
];

// Función que genera los comandos APL para resetear todas las cajas de imágenes
function construirComandosImagenesReset() {
    // Array donde se acumulan los comandos APL a enviar
    const comandos = [];
    // Iteramos por cada caja de imagen por defecto
    IMAGENES_POR_DEFECTO.forEach(({ id, file }) => {
        // Enviamos comando SetValue para cambiar el source de la caja a su archivo por defecto
        comandos.push({ type: 'SetValue', componentId: id, property: 'source', value: `${IMG_BASE}/${file}` });
        // Enviamos comando SetValue para bajar la opacidad a 0.35 (imagen tenue)
        comandos.push({ type: 'SetValue', componentId: id, property: 'opacity', value: 0.35 });
    });
    // Devolvemos el array de comandos
    return comandos;
}

// Función que genera los comandos APL para resaltar la imagen del comando actual
function construirComandosImagenes(comando) {
    // Buscamos la configuración de la caja correspondiente al comando
    const target = MAPA_IMAGENES[comando];
    // Si el comando no tiene imagen asociada, devolvemos solo el reset
    if (!target) return construirComandosImagenesReset();

    // Empezamos con el reset de todas las cajas a su estado por defecto
    const comandos = construirComandosImagenesReset();
    // Cambiamos el source de la caja del comando a su archivo específico
    comandos.push({ type: 'SetValue', componentId: target.id, property: 'source', value: `${IMG_BASE}/${target.file}` });
    // Subimos la opacidad a 1.0 para que esa caja destaque visualmente
    comandos.push({ type: 'SetValue', componentId: target.id, property: 'opacity', value: 1.0 });
    // Devolvemos el array completo de comandos
    return comandos;
}

// Handler que procesa los eventos de telemetría en tiempo real enviados por el APL
const TelemetriaEventHandler = {
    // Verifica si la petición entrante es un UserEvent con el argumento correcto
    canHandle(handlerInput) {
        // Obtenemos la petición del sobre de entrada
        const req = handlerInput.requestEnvelope.request;
        // Retornamos true si es un UserEvent del APL con el argumento "SolicitarTelemetriaEnVivo"
        return req.type === 'Alexa.Presentation.APL.UserEvent' &&
               req.arguments && req.arguments[0] === 'SolicitarTelemetriaEnVivo';
    },
    // Maneja el evento de telemetría leyendo el Shadow y actualizando la pantalla
    async handle(handlerInput) {
        // Valor por defecto de altura si no hay dato disponible
        let alturaVal = '---';
        // Valor por defecto de trendelenburg si no hay dato disponible
        let trenVal = '---';
        // Valor por defecto de tilt si no hay dato disponible
        let tiltVal = '---';
        // Valor por defecto de fowler si no hay dato disponible
        let fowlerVal = '---';
        // Valor por defecto de longitudinal si no hay dato disponible
        let longVal = '---';

        // Bloque try para capturar errores de red o de AWS IoT
        try {
            // Pedimos al Shadow del thing su estado actual
            const shadowRaw = await iotData.getThingShadow({ thingName: THING_NAME }).promise();
            // Parseamos el payload del Shadow que viene como string JSON
            const payload = JSON.parse(shadowRaw.payload);

            // Verificamos que exista el estado reportado en el payload
            if (payload.state && payload.state.reported) {
                // Extraemos el objeto reported del payload
                const rep = payload.state.reported;
                // Si hay valor de altura, lo asignamos como string
                if (rep.tofAltura !== undefined && rep.tofAltura !== null) alturaVal = `${rep.tofAltura}`;
                // Si hay valor de trendelenburg, lo asignamos como string
                if (rep.mpuTren !== undefined && rep.mpuTren !== null) trenVal = `${rep.mpuTren}`;
                // Si hay valor de tilt, lo asignamos como string
                if (rep.mpuTilt !== undefined && rep.mpuTilt !== null) tiltVal = `${rep.mpuTilt}`;
                // Si hay valor de fowler, lo asignamos como string
                if (rep.mpuFowler !== undefined && rep.mpuFowler !== null) fowlerVal = `${rep.mpuFowler}`;
                // Si hay valor longitudinal, lo asignamos como string
                if (rep.tofLong !== undefined && rep.tofLong !== null) longVal = `${rep.tofLong}`;
            }
        // Bloque catch silencioso para no interrumpir el ciclo de telemetría si falla
        } catch (err) {
            // No hacemos nada si falla la lectura del Shadow
        }

        // Retornamos la respuesta con los comandos APL para actualizar los paneles
        return handlerInput.responseBuilder
            // Añadimos la directiva ExecuteCommands al APL activo
            .addDirective({
                // Tipo de directiva para ejecutar comandos APL
                type: 'Alexa.Presentation.APL.ExecuteCommands',
                // Token fijo que apunta a la ventana APL activa
                token: APL_TOKEN,
                // Array de comandos APL a ejecutar
                commands: [
                    // Actualizamos el texto de altura
                    { type: 'SetValue', componentId: 'textoToF', property: 'text', value: alturaVal },
                    // Actualizamos el texto de trendelenburg
                    { type: 'SetValue', componentId: 'textoTren', property: 'text', value: trenVal },
                    // Actualizamos el texto de tilt
                    { type: 'SetValue', componentId: 'textoTilt', property: 'text', value: tiltVal },
                    // Actualizamos el texto de fowler
                    { type: 'SetValue', componentId: 'textoFowler', property: 'text', value: fowlerVal },
                    // Actualizamos el texto longitudinal
                    { type: 'SetValue', componentId: 'textoLong', property: 'text', value: longVal },
                    // Esperamos 150 ms antes de re-invocar el siguiente ciclo
                    { type: 'Idle', delay: 150 },
                    // Re-invocamos el mismo evento para mantener el bucle infinito
                    { type: 'SendEvent', arguments: ['SolicitarTelemetriaEnVivo'] }
                ]
            })
            // Generamos la respuesta final de Alexa
            .getResponse();
    }
};

// Función auxiliar que publica un comando MQTT y actualiza la pantalla
async function enviarOrdenMQTT(handlerInput, comando, valor, voz, visual) {
    // Construimos el payload JSON que se enviará al tópico MQTT
    const payload = JSON.stringify({
        // Nombre del comando a ejecutar
        comando: comando,
        // Valor numérico asociado al comando
        valor: Number(valor),
        // Timestamp para auditoría y ordenamiento
        timestamp: Date.now()
    });

    // Bloque try para capturar errores de publicación
    try {
        // Publicamos el comando al tópico MQTT con QoS 1 (al menos una entrega)
        await iotData.publish({ topic: TOPIC_CONTROL, payload: payload, qos: 1 }).promise();

        // Añadimos comandos APL para actualizar la pantalla tras el comando
        handlerInput.responseBuilder.addDirective({
            // Tipo de directiva para ejecutar comandos APL
            type: 'Alexa.Presentation.APL.ExecuteCommands',
            // Token fijo de la ventana APL activa
            token: APL_TOKEN,
            // Comandos a ejecutar en el APL
            commands: [
                // Actualizamos el texto de acción con la descripción visual
                { type: 'SetValue', componentId: 'textoAccion', property: 'text', value: visual.toUpperCase() },
                // Cambiamos el color del texto a verde
                { type: 'SetValue', componentId: 'textoAccion', property: 'color', value: '#00E676' },
                // Añadimos los comandos de imágenes (reset + resaltado)
                ...construirComandosImagenes(comando),
                // Esperamos 50 ms antes de re-invocar el bucle de telemetría
                { type: 'Idle', delay: 50 },
                // Re-invocamos el evento de telemetría
                { type: 'SendEvent', arguments: ['SolicitarTelemetriaEnVivo'] }
            ]
        });
    // Bloque catch por si falla la publicación al MQTT
    } catch (e) {
        // Respondemos por voz con un mensaje de error y salimos
        return handlerInput.responseBuilder.speak('Fallo de comunicación.').getResponse();
    }

    // Respondemos por voz confirmando la acción y dejando el micrófono abierto
    return handlerInput.responseBuilder.speak(voz).reprompt(REPROMPT_TEXT).getResponse();
}

// Función auxiliar que publica el comando de paro al ESP32
async function enviarParoAlESP32() {
    // Construimos el payload JSON del comando "paro"
    const payload = JSON.stringify({
        // Nombre del comando de emergencia
        comando: 'paro',
        // Valor siempre 0 porque no se necesita magnitud
        valor: 0,
        // Timestamp para auditoría
        timestamp: Date.now()
    });
    // Bloque try para capturar errores
    try {
        // Publicamos el comando al tópico MQTT con QoS 1
        await iotData.publish({ topic: TOPIC_CONTROL, payload: payload, qos: 1 }).promise();
        // Log de confirmación en CloudWatch
        console.log("[PARO] Comando 'paro' publicado al ESP32.");
        // Retornamos true para indicar éxito
        return true;
    // Bloque catch por si falla la publicación
    } catch (e) {
        // Log del error en CloudWatch
        console.log("[PARO] Error publicando 'paro':", e.message);
        // Retornamos false para indicar fallo
        return false;
    }
}

// Handler que se ejecuta cuando el usuario abre la skill por primera vez
const LaunchRequestHandler = {
    // Verifica si la petición es de tipo LaunchRequest
    canHandle(handlerInput) {
        // Retornamos true si el tipo de petición es LaunchRequest
        return Alexa.getRequestType(handlerInput.requestEnvelope) === 'LaunchRequest';
    },
    // Maneja la apertura de la skill renderizando el APL y dando bienvenida
    handle(handlerInput) {
        // Verificamos si el dispositivo soporta APL (pantalla)
        if (Alexa.getSupportedInterfaces(handlerInput.requestEnvelope)['Alexa.Presentation.APL']) {
            // Añadimos la directiva para renderizar el documento APL
            handlerInput.responseBuilder.addDirective({
                // Tipo de directiva para renderizar documento APL
                type: 'Alexa.Presentation.APL.RenderDocument',
                // Token fijo de la ventana APL
                token: APL_TOKEN,
                // Enlace al documento APL almacenado en la skill
                document: { type: 'Link', src: 'doc://alexa/apl/documents/Estado' },
                // Fuente de datos inicial para los componentes
                datasources: {
                    // Objeto payload con los datos de la mesa
                    payload: {
                        // Subobjeto datosMesa con los valores iniciales
                        datosMesa: {
                            // Texto de acción inicial
                            accion: 'SISTEMA EN ESPERA',
                            // Valor inicial de altura
                            valorToF: '---',
                            // Valor inicial de trendelenburg
                            valorTren: '---',
                            // Valor inicial de tilt
                            valorTilt: '---',
                            // Valor inicial de fowler
                            valorFowler: '---',
                            // Valor inicial longitudinal
                            valorLong: '---'
                        }
                    }
                }
            });

            // Añadimos un comando APL para arrancar el bucle de telemetría tras la bienvenida
            handlerInput.responseBuilder.addDirective({
                // Tipo de directiva para ejecutar comandos APL
                type: 'Alexa.Presentation.APL.ExecuteCommands',
                // Token fijo de la ventana APL
                token: APL_TOKEN,
                // Comandos a ejecutar
                commands: [
                    // Esperamos 4000 ms para que Alexa termine de hablar
                    { type: 'Idle', delay: 4000 },
                    // Arrancamos el bucle de telemetría enviando el evento inicial
                    { type: 'SendEvent', arguments: ['SolicitarTelemetriaEnVivo'] }
                ]
            });
        }
        // Respondemos por voz dando la bienvenida y dejando el micrófono abierto
        return handlerInput.responseBuilder
            // Mensaje de bienvenida
            .speak('Sistema de control de mesa quirúrgica activado.')
            // Reprompt para mantener el micrófono abierto
            .reprompt(REPROMPT_TEXT)
            // Generamos la respuesta final
            .getResponse();
    }
};

// Handler que se ejecuta cuando el usuario dice "subir la mesa"
const SubirMesaIntentHandler = {
    // Verifica si la petición es del tipo SubirMesaIntent
    canHandle(handlerInput) {
        // Retornamos true si el nombre de la intención coincide
        return Alexa.getIntentName(handlerInput.requestEnvelope) === 'SubirMesaIntent';
    },
    // Maneja la intención extrayendo el slot y publicando el comando
    async handle(handlerInput) {
        // Extraemos el valor del slot "distancia" del comando de voz
        const slot = Alexa.getSlotValue(handlerInput.requestEnvelope, 'distancia');
        // Cantidad por defecto si el slot está vacío
        let cantidad = PASO_SEGURO_MM;
        // Texto de respuesta por defecto
        let voz = 'Subiendo un poco.';
        // Si el slot trae un número válido, lo usamos
        if (slot && slot !== '?' && !isNaN(slot)) {
            // Convertimos el slot a entero
            cantidad = parseInt(slot);
            // Actualizamos el texto de respuesta con la cantidad exacta
            voz = `Subiendo ${cantidad} milímetros.`;
        }
        // Enviamos el comando MQTT y actualizamos pantalla
        return await enviarOrdenMQTT(handlerInput, 'subir', cantidad, voz, `ELEVANDO (+${cantidad}mm)`);
    }
};

// Handler que se ejecuta cuando el usuario dice "bajar la mesa"
const BajarMesaIntentHandler = {
    // Verifica si la petición es del tipo BajarMesaIntent
    canHandle(handlerInput) {
        // Retornamos true si el nombre de la intención coincide
        return Alexa.getIntentName(handlerInput.requestEnvelope) === 'BajarMesaIntent';
    },
    // Maneja la intención extrayendo el slot y publicando el comando
    async handle(handlerInput) {
        // Extraemos el valor del slot "distancia" del comando de voz
        const slot = Alexa.getSlotValue(handlerInput.requestEnvelope, 'distancia');
        // Cantidad por defecto si el slot está vacío
        let cantidad = PASO_SEGURO_MM;
        // Texto de respuesta por defecto
        let voz = 'Descendiendo un poco.';
        // Si el slot trae un número válido, lo usamos
        if (slot && slot !== '?' && !isNaN(slot)) {
            // Convertimos el slot a entero
            cantidad = parseInt(slot);
            // Actualizamos el texto de respuesta con la cantidad exacta
            voz = `Descendiendo ${cantidad} milímetros.`;
        }
        // Enviamos el comando MQTT y actualizamos pantalla
        return await enviarOrdenMQTT(handlerInput, 'bajar', cantidad, voz, `DESCENSO (-${cantidad}mm)`);
    }
};

// Handler que se ejecuta cuando el usuario dice "trendelenburg"
const TrendelenburgIntentHandler = {
    // Verifica si la petición es del tipo TrendelenburgIntent
    canHandle(handlerInput) {
        // Retornamos true si el nombre de la intención coincide
        return Alexa.getIntentName(handlerInput.requestEnvelope) === 'TrendelenburgIntent';
    },
    // Maneja la intención extrayendo el slot y publicando el comando
    async handle(handlerInput) {
        // Extraemos el valor del slot "grados" del comando de voz
        const slot = Alexa.getSlotValue(handlerInput.requestEnvelope, 'grados');
        // Grados por defecto si el slot está vacío
        let grados = PASO_SEGURO_GRADOS;
        // Texto de respuesta por defecto
        let voz = 'Aplicando Trendelenburg.';
        // Si el slot trae un número válido, lo usamos
        if (slot && slot !== '?' && !isNaN(slot)) {
            // Convertimos el slot a entero
            grados = parseInt(slot);
            // Actualizamos el texto de respuesta con los grados exactos
            voz = `Aplicando Trendelenburg a ${grados} grados.`;
        }
        // Enviamos el comando MQTT y actualizamos pantalla
        return await enviarOrdenMQTT(handlerInput, 'tren', grados, voz, `TRENDELENBURG (+${grados}°)`);
    }
};

// Handler que se ejecuta cuando el usuario dice "anti trendelenburg"
const AntiTrendelenburgIntentHandler = {
    // Verifica si la petición es del tipo AntiTrendelenburgIntent
    canHandle(handlerInput) {
        // Retornamos true si el nombre de la intención coincide
        return Alexa.getIntentName(handlerInput.requestEnvelope) === 'AntiTrendelenburgIntent';
    },
    // Maneja la intención extrayendo el slot y publicando el comando
    async handle(handlerInput) {
        // Extraemos el valor del slot "grados" del comando de voz
        const slot = Alexa.getSlotValue(handlerInput.requestEnvelope, 'grados');
        // Grados por defecto si el slot está vacío
        let grados = PASO_SEGURO_GRADOS;
        // Texto de respuesta por defecto
        let voz = 'Aplicando Anti Trendelenburg.';
        // Si el slot trae un número válido, lo usamos
        if (slot && slot !== '?' && !isNaN(slot)) {
            // Convertimos el slot a entero
            grados = parseInt(slot);
            // Actualizamos el texto de respuesta con los grados exactos
            voz = `Aplicando Anti Trendelenburg a ${grados} grados.`;
        }
        // Enviamos el comando MQTT y actualizamos pantalla
        return await enviarOrdenMQTT(handlerInput, 'antitren', grados, voz, `ANTI-TREN (-${grados}°)`);
    }
};

// Handler que se ejecuta cuando el usuario dice "tilt derecho"
const TiltDerechoIntentHandler = {
    // Verifica si la petición es del tipo TiltDerechoIntent
    canHandle(handlerInput) {
        // Retornamos true si el nombre de la intención coincide
        return Alexa.getIntentName(handlerInput.requestEnvelope) === 'TiltDerechoIntent';
    },
    // Maneja la intención extrayendo el slot y publicando el comando
    async handle(handlerInput) {
        // Extraemos el valor del slot "grados" del comando de voz
        const slot = Alexa.getSlotValue(handlerInput.requestEnvelope, 'grados');
        // Grados por defecto si el slot está vacío
        let grados = PASO_SEGURO_GRADOS;
        // Texto de respuesta por defecto
        let voz = 'Inclinando a la derecha.';
        // Si el slot trae un número válido, lo usamos
        if (slot && slot !== '?' && !isNaN(slot)) {
            // Convertimos el slot a entero
            grados = parseInt(slot);
            // Actualizamos el texto de respuesta con los grados exactos
            voz = `Aplicando tilt derecho a ${grados} grados.`;
        }
        // Enviamos el comando MQTT y actualizamos pantalla
        return await enviarOrdenMQTT(handlerInput, 'tilt_der', grados, voz, `TILT DER (${grados}°)`);
    }
};

// Handler que se ejecuta cuando el usuario dice "tilt izquierdo"
const TiltIzquierdoIntentHandler = {
    // Verifica si la petición es del tipo TiltIzquierdoIntent
    canHandle(handlerInput) {
        // Retornamos true si el nombre de la intención coincide
        return Alexa.getIntentName(handlerInput.requestEnvelope) === 'TiltIzquierdoIntent';
    },
    // Maneja la intención extrayendo el slot y publicando el comando
    async handle(handlerInput) {
        // Extraemos el valor del slot "grados" del comando de voz
        const slot = Alexa.getSlotValue(handlerInput.requestEnvelope, 'grados');
        // Grados por defecto si el slot está vacío
        let grados = PASO_SEGURO_GRADOS;
        // Texto de respuesta por defecto
        let voz = 'Inclinando a la izquierda.';
        // Si el slot trae un número válido, lo usamos
        if (slot && slot !== '?' && !isNaN(slot)) {
            // Convertimos el slot a entero
            grados = parseInt(slot);
            // Actualizamos el texto de respuesta con los grados exactos
            voz = `Aplicando tilt izquierdo a ${grados} grados.`;
        }
        // Enviamos el comando MQTT y actualizamos pantalla
        return await enviarOrdenMQTT(handlerInput, 'tilt_izq', grados, voz, `TILT IZQ (${grados}°)`);
    }
};

// Handler que se ejecuta cuando el usuario dice "fowler" o "posición Fowler"
const FowlerIntentHandler = {
    // Verifica si la petición es del tipo FowlerIntent
    canHandle(handlerInput) {
        // Retornamos true si el nombre de la intención coincide
        return Alexa.getIntentName(handlerInput.requestEnvelope) === 'FowlerIntent';
    },
    // Maneja la intención extrayendo el slot y publicando el comando
    async handle(handlerInput) {
        // Extraemos el valor del slot "grados" del comando de voz
        const slot = Alexa.getSlotValue(handlerInput.requestEnvelope, 'grados');
        // Grados por defecto si el slot está vacío
        let grados = PASO_SEGURO_GRADOS;
        // Texto de respuesta por defecto
        let voz = 'Elevando respaldo.';
        // Si el slot trae un número válido, lo usamos
        if (slot && slot !== '?' && !isNaN(slot)) {
            // Convertimos el slot a entero
            grados = parseInt(slot);
            // Actualizamos el texto de respuesta con los grados exactos
            voz = `Aplicando posición Fowler a ${grados} grados.`;
        }
        // Enviamos el comando MQTT y actualizamos pantalla
        return await enviarOrdenMQTT(handlerInput, 'fowler', grados, voz, `FOWLER (+${grados}°)`);
    }
};

// Handler que se ejecuta cuando el usuario dice "bajar respaldo"
const BajarRespaldoIntentHandler = {
    // Verifica si la petición es del tipo BajarRespaldoIntent
    canHandle(handlerInput) {
        // Retornamos true si el nombre de la intención coincide
        return Alexa.getIntentName(handlerInput.requestEnvelope) === 'BajarRespaldoIntent';
    },
    // Maneja la intención extrayendo el slot y publicando el comando
    async handle(handlerInput) {
        // Extraemos el valor del slot "grados" del comando de voz
        const slot = Alexa.getSlotValue(handlerInput.requestEnvelope, 'grados');
        // Grados por defecto si el slot está vacío
        let grados = PASO_SEGURO_GRADOS;
        // Texto de respuesta por defecto
        let voz = 'Descendiendo respaldo.';
        // Si el slot trae un número válido, lo usamos
        if (slot && slot !== '?' && !isNaN(slot)) {
            // Convertimos el slot a entero
            grados = parseInt(slot);
            // Actualizamos el texto de respuesta con los grados exactos
            voz = `Descendiendo respaldo ${grados} grados.`;
        }
        // Enviamos el comando MQTT y actualizamos pantalla
        return await enviarOrdenMQTT(handlerInput, 'bajar_fowler', grados, voz, `DESCENSO RESPALDO (-${grados}°)`);
    }
};

// Handler que se ejecuta cuando el usuario dice "desplazamiento cefálico"
const DesplazamientoCefalicoIntentHandler = {
    // Verifica si la petición es del tipo DesplazamientoCefalicoIntent
    canHandle(handlerInput) {
        // Retornamos true si el nombre de la intención coincide
        return Alexa.getIntentName(handlerInput.requestEnvelope) === 'DesplazamientoCefalicoIntent';
    },
    // Maneja la intención extrayendo el slot y publicando el comando
    async handle(handlerInput) {
        // Extraemos el valor del slot "distancia" del comando de voz
        const slot = Alexa.getSlotValue(handlerInput.requestEnvelope, 'distancia');
        // Cantidad por defecto si el slot está vacío
        let cantidad = PASO_SEGURO_LONG;
        // Texto de respuesta por defecto
        let voz = 'Deslizando hacia la cabeza.';
        // Si el slot trae un número válido, lo usamos
        if (slot && slot !== '?' && !isNaN(slot)) {
            // Convertimos el slot a entero
            cantidad = parseInt(slot);
            // Actualizamos el texto de respuesta con la cantidad exacta
            voz = `Desplazando cefálico ${cantidad} milímetros.`;
        }
        // Enviamos el comando MQTT y actualizamos pantalla
        return await enviarOrdenMQTT(handlerInput, 'cefalico', cantidad, voz, `CEFÁLICO (+${cantidad}mm)`);
    }
};

// Handler que se ejecuta cuando el usuario dice "desplazamiento caudal"
const DesplazamientoCaudalIntentHandler = {
    // Verifica si la petición es del tipo DesplazamientoCaudalIntent
    canHandle(handlerInput) {
        // Retornamos true si el nombre de la intención coincide
        return Alexa.getIntentName(handlerInput.requestEnvelope) === 'DesplazamientoCaudalIntent';
    },
    // Maneja la intención extrayendo el slot y publicando el comando
    async handle(handlerInput) {
        // Extraemos el valor del slot "distancia" del comando de voz
        const slot = Alexa.getSlotValue(handlerInput.requestEnvelope, 'distancia');
        // Cantidad por defecto si el slot está vacío
        let cantidad = PASO_SEGURO_LONG;
        // Texto de respuesta por defecto
        let voz = 'Deslizando hacia los pies.';
        // Si el slot trae un número válido, lo usamos
        if (slot && slot !== '?' && !isNaN(slot)) {
            // Convertimos el slot a entero
            cantidad = parseInt(slot);
            // Actualizamos el texto de respuesta con la cantidad exacta
            voz = `Desplazando caudal ${cantidad} milímetros.`;
        }
        // Enviamos el comando MQTT y actualizamos pantalla
        return await enviarOrdenMQTT(handlerInput, 'caudal', cantidad, voz, `CAUDAL (-${cantidad}mm)`);
    }
};

// Handler que se ejecuta cuando el usuario dice "paro de emergencia"
const ParoEmergenciaIntentHandler = {
    // Verifica si la petición es del tipo ParoEmergenciaIntent
    canHandle(handlerInput) {
        // Retornamos true si el tipo y el nombre de la intención coinciden
        return Alexa.getRequestType(handlerInput.requestEnvelope) === 'IntentRequest'
            && Alexa.getIntentName(handlerInput.requestEnvelope) === 'ParoEmergenciaIntent';
    },
    // Maneja la intención enviando paro al ESP32 y actualizando pantalla
    async handle(handlerInput) {
        // 1. Publicamos el comando de paro al ESP32
        await enviarParoAlESP32();

        // 2. Verificamos si el dispositivo soporta APL
        if (Alexa.getSupportedInterfaces(handlerInput.requestEnvelope)['Alexa.Presentation.APL']) {
            // Añadimos la directiva de comandos APL
            handlerInput.responseBuilder.addDirective({
                // Tipo de directiva para ejecutar comandos APL
                type: 'Alexa.Presentation.APL.ExecuteCommands',
                // Token fijo de la ventana APL
                token: APL_TOKEN,
                // Comandos a ejecutar
                commands: [
                    // Cambiamos el texto a "PARANDO"
                    { type: 'SetValue', componentId: 'textoAccion', property: 'text', value: 'PARANDO' },
                    // Cambiamos el color a rojo
                    { type: 'SetValue', componentId: 'textoAccion', property: 'color', value: '#FF3366' },
                    // Reseteamos todas las cajas de imágenes
                    ...construirComandosImagenesReset(),
                    // Esperamos 50 ms
                    { type: 'Idle', delay: 50 },
                    // Re-invocamos el bucle de telemetría
                    { type: 'SendEvent', arguments: ['SolicitarTelemetriaEnVivo'] }
                ]
            });
        }

        // 3. Respondemos sin cerrar sesión para que el usuario pueda dar más comandos
        return handlerInput.responseBuilder
            // Mensaje confirmando el paro
            .speak('Sistema detenido.')
            // Reprompt para mantener el micrófono abierto
            .reprompt('Sistema en paro.')
            // Mantenemos la sesión abierta explícitamente
            .withShouldEndSession(false)
            // Generamos la respuesta
            .getResponse();
    }
};

// Handler anti-ruido que filtra ruidos externos del quirófano
const FallbackIntentHandler = {
    // Verifica si la petición es del tipo FallbackIntent
    canHandle(handlerInput) {
        // Retornamos true si el tipo y el nombre coinciden
        return Alexa.getRequestType(handlerInput.requestEnvelope) === 'IntentRequest'
            && Alexa.getIntentName(handlerInput.requestEnvelope) === 'AMAZON.FallbackIntent';
    },
    // Maneja el fallback respondiendo silenciosamente
    handle(handlerInput) {
        // Log para confirmar que se filtró el ruido
        console.log("[FILTRO] Ruido externo ignorado silenciosamente.");
        // Retornamos respuesta vacía sin hablar
        return handlerInput.responseBuilder.getResponse();
    }
};

// Handler que se ejecuta cuando el usuario pide ayuda
const HelpIntentHandler = {
    // Verifica si la petición es del tipo HelpIntent
    canHandle(handlerInput) {
        // Retornamos true si el tipo y el nombre coinciden
        return Alexa.getRequestType(handlerInput.requestEnvelope) === 'IntentRequest'
            && Alexa.getIntentName(handlerInput.requestEnvelope) === 'AMAZON.HelpIntent';
    },
    // Maneja la petición listando los comandos disponibles
    handle(handlerInput) {
        // Respondemos por voz con la lista de comandos
        return handlerInput.responseBuilder
            // Mensaje de ayuda
            .speak('Puedes controlar elevación, tilt, trendelenburg, fowler y deslizamiento cefalocaudal. Di paro de emergencia para detener todo.')
            // Reprompt para mantener el micrófono abierto
            .reprompt(REPROMPT_TEXT)
            // Generamos la respuesta
            .getResponse();
    }
};

// Handler que se ejecuta cuando el usuario dice "sal", "detente" o "cancela"
const CancelAndStopIntentHandler = {
    // Verifica si la petición es del tipo StopIntent o CancelIntent
    canHandle(handlerInput) {
        // Obtenemos la petición del sobre
        const req = handlerInput.requestEnvelope.request;
        // Retornamos true si es un IntentRequest con StopIntent o CancelIntent
        return req.type === 'IntentRequest' &&
            (req.intent.name === 'AMAZON.CancelIntent' || req.intent.name === 'AMAZON.StopIntent');
    },
    // Maneja la salida enviando paro al ESP32 y cerrando la sesión
    async handle(handlerInput) {
        // 1. Enviamos el comando de paro al ESP32 para detener cualquier movimiento
        await enviarParoAlESP32();

        // 2. Verificamos si el dispositivo soporta APL
        if (Alexa.getSupportedInterfaces(handlerInput.requestEnvelope)['Alexa.Presentation.APL']) {
            // Añadimos la directiva de comandos APL
            handlerInput.responseBuilder.addDirective({
                // Tipo de directiva para ejecutar comandos APL
                type: 'Alexa.Presentation.APL.ExecuteCommands',
                // Token fijo de la ventana APL
                token: APL_TOKEN,
                // Comandos a ejecutar
                commands: [
                    // Cambiamos el texto a "SALIENDO..."
                    { type: 'SetValue', componentId: 'textoAccion', property: 'text', value: 'SALIENDO...' },
                    // Cambiamos el color a rojo
                    { type: 'SetValue', componentId: 'textoAccion', property: 'color', value: '#FF3366' },
                    // Reseteamos todas las cajas de imágenes
                    ...construirComandosImagenesReset()
                ]
            });
        }

        // 3. Respondemos por voz y cerramos la sesión definitivamente
        return handlerInput.responseBuilder
            // Mensaje de despedida
            .speak('Saliendo del modo quirófano.')
            // Cerramos la sesión con true
            .withShouldEndSession(true)
            // Generamos la respuesta
            .getResponse();
    }
};

// Handler que se ejecuta cuando la sesión finaliza por timeout o error
const SessionEndedRequestHandler = {
    // Verifica si la petición es del tipo SessionEndedRequest
    canHandle(handlerInput) {
        // Retornamos true si el tipo coincide
        return Alexa.getRequestType(handlerInput.requestEnvelope) === 'SessionEndedRequest';
    },
    // Maneja el fin de sesión silenciosamente
    handle(handlerInput) {
        // Retornamos una respuesta vacía
        return handlerInput.responseBuilder.getResponse();
    }
};

// Handler global de errores que atrapa cualquier excepción no manejada
const ErrorHandler = {
    // Siempre retorna true para atrapar cualquier error
    canHandle() { return true; },
    // Maneja el error mostrando un mensaje de fallback
    handle(handlerInput, error) {
        // Log del error en CloudWatch con el mensaje específico
        console.log(`[ERROR] ${error.message}`);
        // Respondemos por voz indicando que no se entendió
        return handlerInput.responseBuilder
            // Mensaje de error
            .speak('No reconocí la instrucción.')
            // Reprompt para mantener el micrófono abierto
            .reprompt(REPROMPT_TEXT)
            // Generamos la respuesta
            .getResponse();
    }
};

// Exportamos el handler final construido con todos los controladores registrados
exports.handler = Alexa.SkillBuilders.custom()
    // Registramos todos los handlers en el orden en que se evalúan
    .addRequestHandlers(
        // Handler de apertura de la skill
        LaunchRequestHandler,
        // Handler de telemetría en tiempo real (eventos APL)
        TelemetriaEventHandler,
        // Handler de paro de emergencia (no cierra sesión)
        ParoEmergenciaIntentHandler,
        // Handler de subir mesa
        SubirMesaIntentHandler,
        // Handler de bajar mesa
        BajarMesaIntentHandler,
        // Handler de Trendelenburg
        TrendelenburgIntentHandler,
        // Handler de Anti-Trendelenburg
        AntiTrendelenburgIntentHandler,
        // Handler de tilt derecho
        TiltDerechoIntentHandler,
        // Handler de tilt izquierdo
        TiltIzquierdoIntentHandler,
        // Handler de Fowler
        FowlerIntentHandler,
        // Handler de bajar respaldo
        BajarRespaldoIntentHandler,
        // Handler de desplazamiento cefálico
        DesplazamientoCefalicoIntentHandler,
        // Handler de desplazamiento caudal
        DesplazamientoCaudalIntentHandler,
        // Handler anti-ruido
        FallbackIntentHandler,
        // Handler de ayuda
        HelpIntentHandler,
        // Handler de salir/cancelar (envía paro y cierra sesión)
        CancelAndStopIntentHandler,
        // Handler de fin de sesión
        SessionEndedRequestHandler
    )
    // Registramos el handler global de errores
    .addErrorHandlers(ErrorHandler)
    // Generamos la función Lambda final
    .lambda();