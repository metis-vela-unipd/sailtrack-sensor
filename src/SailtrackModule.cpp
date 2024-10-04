#include "SailtrackModule.h"

// ------------------------- Static Variables ------------------------- //

char SailtrackModule::name[STM_MODULE_NAME_MAX_LENGTH];
char SailtrackModule::hostname[2*STM_MODULE_NAME_MAX_LENGTH];

SailtrackModuleCallbacks * SailtrackModule::callbacks;

esp_mqtt_client_handle_t SailtrackModule::mqttClient;
bool SailtrackModule::mqttConnected;
unsigned int SailtrackModule::publishedMessagesCount;
unsigned int SailtrackModule::receivedMessagesCount;

#ifdef NOTIFICATION
bool SailtrackModule::isAnode;
int SailtrackModule::red_pin;
int SailtrackModule::green_pin;
int SailtrackModule::blue_pin;
#endif

// -------------------------- Begin Methods -------------------------- //

void SailtrackModule::begin(const char * name, IPAddress ip, SailtrackModuleCallbacks * callbacks) {
    vTaskPrioritySet(NULL, STM_TASK_HIGH_PRIORITY);

    strcpy(SailtrackModule::name, name);
    SailtrackModule::callbacks = callbacks;
    sprintf(SailtrackModule::hostname, "sailtrack-%s", name);

    #ifdef NOTIFICATION
    beginNotificationLed();
    #endif
    log_printf("\n");
    beginWifi(ip);
    beginOTA();
    beginMqtt();
}

#ifdef NOTIFICATION
void SailtrackModule::beginNotificationLed() {
    SailtrackModule::isAnode = static_cast<int>((uint8_t)(NOTIFICATION >> 24));
    SailtrackModule::red_pin = static_cast<int>((uint8_t)(NOTIFICATION >> 16));
    SailtrackModule::green_pin = static_cast<int>((uint8_t)(NOTIFICATION >> 8));
    SailtrackModule::blue_pin = static_cast<int>((uint8_t)(NOTIFICATION));

    pinMode(red_pin, OUTPUT);
    pinMode(green_pin, OUTPUT);
    pinMode(blue_pin, OUTPUT);
    if(isAnode){
        digitalWrite(red_pin, 0);
        digitalWrite(green_pin, 0);
        digitalWrite(blue_pin, 0);
    }else{
        digitalWrite(red_pin, 255);
        digitalWrite(green_pin, 255);
        digitalWrite(blue_pin, 255);
    }
    log_printf("Led enable and ready");
    xTaskCreate(notificationLedTask, "notificationLedTask", STM_TASK_SMALL_STACK_SIZE, NULL, STM_TASK_LOW_PRIORITY, NULL);
}
#endif

void SailtrackModule::beginWifi(IPAddress ip) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname);
    WiFi.config(ip, IPAddress().fromString(STM_WIFI_GATEWAY_ADDR), IPAddress().fromString(STM_WIFI_SUBNET));
    WiFi.begin(STM_WIFI_AP_SSID, STM_WIFI_AP_PASSWORD);
    
    log_i("Connecting to '%s'...", STM_WIFI_AP_SSID);

    for (int i = 0; WiFi.status() != WL_CONNECTED && i < STM_WIFI_CONNECTION_TIMEOUT_MS / 500 ; i++)
        delay(500);

    if (WiFi.status() != WL_CONNECTED) {
        log_i("Impossible to connect to '%s'", STM_WIFI_AP_SSID);
        log_i("Going to deep sleep, goodnight...");
        log_printf("\n");
        ESP.deepSleep(STM_WIFI_SLEEP_DURATION_US);
    }

    log_i("Successfully connected to '%s'!", STM_WIFI_AP_SSID);

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        log_e("Lost connection to '%s'", STM_WIFI_AP_SSID);
        log_e("Rebooting...");
        ESP.restart();
    }, arduino_event_id_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

void SailtrackModule::beginOTA() {
    ArduinoOTA
        .onStart([]() {
            if (ArduinoOTA.getCommand() == U_FLASH) log_i("Start updating sketch...");
            else log_i("Start updating filesystem...");
            esp_mqtt_client_stop(mqttClient);
        })
        .onEnd([]() {
            log_i("Update successfully completed!");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            log_v("Progress: %u", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
            if (error == OTA_AUTH_ERROR) log_e("Error[%u]: Auth Failed", error);
            else if (error == OTA_BEGIN_ERROR) log_e("Error[%u]: Begin Failed", error);
            else if (error == OTA_CONNECT_ERROR) log_e("Error[%u]: Connect Failed", error);
            else if (error == OTA_RECEIVE_ERROR) log_e("Error[%u]: Receive Failed", error);
            else if (error == OTA_END_ERROR) log_e("Error[%u]: End Failed", error);
        });
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.begin();
    xTaskCreate(otaTask, "otaTask", STM_TASK_MEDIUM_STACK_SIZE, NULL, STM_TASK_MEDIUM_PRIORITY, NULL);
}

void SailtrackModule::beginMqtt() {
    esp_mqtt_client_config_t mqttConfig = {};
    mqttConfig.client_id = hostname;
    mqttConfig.host = STM_MQTT_HOST_ADDR;
    mqttConfig.port = STM_MQTT_PORT;
    mqttConfig.username = STM_MQTT_USERNAME;
    mqttConfig.password = STM_MQTT_PASSWORD;
    mqttClient = esp_mqtt_client_init(&mqttConfig);
    esp_mqtt_client_start(mqttClient);
    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_CONNECTED, mqttEventHandler, NULL);

    log_i("Connecting to 'mqtt://%s@%s:%d'...", STM_MQTT_USERNAME, STM_MQTT_HOST_ADDR, STM_MQTT_PORT);

    for (int i = 0; !mqttConnected && i < STM_MQTT_CONNECTION_TIMEOUT_MS / 500; i++)
        delay(500);

    if (!mqttConnected) {
        log_e("Impossible to connect to 'mqtt://%s@%s:%d'", STM_MQTT_USERNAME, STM_MQTT_HOST_ADDR, STM_MQTT_PORT);
        log_e("Rebooting...");
        ESP.restart();
    }

    log_i("Successfully connected to 'mqtt://%s@%s:%d'!", STM_MQTT_USERNAME, STM_MQTT_HOST_ADDR, STM_MQTT_PORT);

    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_DATA, mqttEventHandler, NULL);
    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_DISCONNECTED, mqttEventHandler, NULL);
    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_PUBLISHED, mqttEventHandler, NULL);
    xTaskCreate(statusTask, "statusTask", STM_TASK_MEDIUM_STACK_SIZE, NULL, STM_TASK_MEDIUM_PRIORITY, NULL);
    xTaskCreate(logTask, "logTask", STM_TASK_MEDIUM_STACK_SIZE, NULL, STM_TASK_LOW_PRIORITY, NULL);
}

// ----------------------------- Handlers ----------------------------- //

void SailtrackModule::mqttEventHandler(void * handlerArgs, esp_event_base_t base, int32_t eventId, void * eventData) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)eventData;
    switch ((esp_mqtt_event_id_t)eventId) {
        case MQTT_EVENT_CONNECTED: {
            mqttConnected = true;
        } break;
        case MQTT_EVENT_DATA: {
            char topic[STM_MQTT_TOPIC_BUFFER_SIZE];
            char message[STM_MQTT_DATA_BUFFER_SIZE];
            sprintf(topic, "%.*s", event->topic_len, event->topic);
            sprintf(message, "%.*s", event->data_len, event->data);
            receivedMessagesCount++;
            StaticJsonDocument<STM_JSON_DOCUMENT_BIG_SIZE> doc;
            deserializeJson(doc, message);
            if (callbacks) callbacks->onMqttMessage(topic, doc.as<JsonObjectConst>());
        } break;
        case MQTT_EVENT_DISCONNECTED: {
            log_e("Lost connection to 'mqtt://%s@%s:%d'...", STM_MQTT_USERNAME, STM_MQTT_HOST_ADDR, STM_MQTT_PORT);
            log_e("Rebooting...");
            ESP.restart();
        } break;
        case MQTT_EVENT_PUBLISHED: {
            publishedMessagesCount++;
        } break;
        default: {} break;
    }
}

// ------------------------------ Tasks ------------------------------ //

#ifdef NOTIFICATION
void SailtrackModule::notificationLedTask(void * pvArguments) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    while(true) {
        if(mqttCheck()){
            color(0, 0, 255);
            vTaskDelay(pdMS_TO_TICKS(BLINKING_TIME));
            color(0, 0, 0);
        }else{
            setColor(callbacks->notificationLed());
        }
        vTaskDelay(pdMS_TO_TICKS(BLINKING_TIME));
    }
}

int SailtrackModule::mqttCheck(){
    if(mqttConnected){
        return 0;
    }
    return 1;
}

void SailtrackModule::setColor(uint32_t colorCode){
    uint8_t red = (uint8_t)(colorCode >> 16);
    uint8_t green = (uint8_t)(colorCode >> 8);
    uint8_t blue = (uint8_t)(colorCode);
    color(red, green, blue);
}

void SailtrackModule::color(uint8_t red, uint8_t green, uint8_t blue){
    if(isAnode){
        digitalWrite(red_pin, red);
        digitalWrite(green_pin, green);
        digitalWrite(blue_pin, blue);
    }else{
        digitalWrite(red_pin, 255-red);
        digitalWrite(green_pin, 255-green);
        digitalWrite(blue_pin, 255-blue);
    }
}

#endif


void SailtrackModule::statusTask(void * pvArguments) {
    char topic[STM_MQTT_TOPIC_BUFFER_SIZE];
    sprintf(topic, "status/%s", name);
    TickType_t lastWakeTime = xTaskGetTickCount();
    while (true) {
        StaticJsonDocument<STM_JSON_DOCUMENT_SMALL_SIZE> doc;
        if (callbacks) callbacks->onStatusPublish(doc.to<JsonObject>());
        JsonObject cpu = doc.createNestedObject("cpu");
        cpu["temperature"] = temperatureRead();
        JsonObject heap = doc.createNestedObject("heap");
        heap["load"] = 1 - (float) ESP.getFreeHeap() / (float) ESP.getHeapSize();
        heap["maxAlloc"] = ESP.getMaxAllocHeap();
        if (ESP.getPsramSize()) {
            JsonObject psram = doc.createNestedObject("psram");
            psram["load"] = 1 - (float) ESP.getFreePsram() / (float) ESP.getPsramSize();
            psram["maxAlloc"] = ESP.getMaxAllocPsram();
        }
        publish(topic, doc.as<JsonObjectConst>());
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(STM_STATUS_TASK_INTERVAL_MS));
    }
}

void SailtrackModule::logTask(void * pvArguments) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    log_printf("\n");
    while (true) {
        log_i("Published messages: %d, Received messages: %d", publishedMessagesCount, receivedMessagesCount);
        if (callbacks) callbacks->onLogPrint();
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(STM_LOG_TASK_INTERVAL_MS));
    }
}

void SailtrackModule::otaTask(void * pvArguments) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    while (true) {
        ArduinoOTA.handle();
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(STM_OTA_TASK_INTERVAL_MS));
    }
}

// ----------------------------- Wrappers ----------------------------- //

int SailtrackModule::publish(const char * topic, JsonObjectConst message) {
    char output[STM_MQTT_DATA_BUFFER_SIZE];
    serializeJson(message, output);
    return esp_mqtt_client_publish(mqttClient, topic, output, 0, 1, 0);
}

int SailtrackModule::subscribe(const char * topic) {
    return esp_mqtt_client_subscribe(mqttClient, topic, 1);
}
