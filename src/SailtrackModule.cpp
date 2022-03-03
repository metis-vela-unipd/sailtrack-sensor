#include "SailtrackModule.h"

static const char * LOG_TAG = "SAILTRACK_MODULE";

const char * SailtrackModule::name;

SailtrackModuleCallbacks * SailtrackModule::callbacks;
int SailtrackModule::notificationLed;
bool SailtrackModule::notificationLedReversed;

WifiConfig SailtrackModule::wifiConfig;

esp_mqtt_client_config_t SailtrackModule::mqttConfig;
esp_mqtt_client_handle_t SailtrackModule::mqttClient;
bool SailtrackModule::mqttConnected;
int SailtrackModule::publishedMessagesCount;
int SailtrackModule::receivedMessagesCount;

void SailtrackModule::configWifi(const char * ssid, const char * password, IPAddress gateway, IPAddress subnet) {
    wifiConfig.ssid = ssid;
    wifiConfig.password = password;
    wifiConfig.gateway = gateway;
    wifiConfig.subnet = subnet;
}

void SailtrackModule::configMqtt(IPAddress host, int port, const char * username, const char * password) {
    mqttConfig.host = strdup(host.toString().c_str());
    mqttConfig.port = port;
    mqttConfig.username = username;
    mqttConfig.password = password;
}

void SailtrackModule::begin(const char * name, IPAddress ip, SailtrackModuleCallbacks * callbacks, int notificationLed, bool notificationLedReversed) {
    vTaskPrioritySet(NULL, TASK_HIGH_PRIORITY);

    SailtrackModule::name = name;
    SailtrackModule::callbacks = callbacks;
    SailtrackModule::notificationLed = notificationLed;
    SailtrackModule::notificationLedReversed = notificationLedReversed;

    char hostname[30] = "sailtrack-";
    wifiConfig.hostname = strdup(strcat(hostname, name));
    wifiConfig.ip = ip;
    if (!wifiConfig.ssid) wifiConfig.ssid = WIFI_DEFAULT_SSID;
    if (!wifiConfig.password) wifiConfig.password = WIFI_DEFAULT_PASSWORD;
    if (!wifiConfig.gateway) wifiConfig.gateway = WIFI_DEFAULT_GATEWAY;
    if (!wifiConfig.subnet) wifiConfig.subnet = WIFI_DEFAULT_SUBNET;

    mqttConfig.client_id = wifiConfig.hostname;
    if (!mqttConfig.host) mqttConfig.host = strdup(MQTT_DEFAULT_HOST.toString().c_str());
    if (!mqttConfig.port) mqttConfig.port = MQTT_DEFAULT_PORT;
    if (!mqttConfig.username) mqttConfig.username = MQTT_DEFAULT_USERNAME;
    if (!mqttConfig.password) mqttConfig.password = MQTT_DEFAULT_PASSWORD;

    if (notificationLed != -1) 
        beginNotificationLed();
    beginLogging();
    beginWifi();
    beginOTA();
    beginMqtt();
}

void SailtrackModule::beginNotificationLed() {
    pinMode(notificationLed, OUTPUT);
    xTaskCreate(notificationLedTask, "notificationLedTask", TASK_SMALL_STACK_SIZE, NULL, TASK_LOW_PRIORITY, NULL);
}

void SailtrackModule::beginLogging() {
    Serial.begin(115200);
    Serial.println();
    esp_log_set_vprintf(m_vprintf);
    esp_log_level_set(LOG_TAG, ESP_LOG_INFO);
}

void SailtrackModule::beginWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(wifiConfig.hostname);
    WiFi.config(wifiConfig.ip, wifiConfig.gateway, wifiConfig.subnet);
    WiFi.begin(wifiConfig.ssid, wifiConfig.password);

    if (callbacks) callbacks->onWifiConnectionBegin();
    
    ESP_LOGI(LOG_TAG, "Connecting to '%s'...", wifiConfig.ssid);

    for (int i = 0; WiFi.status() != WL_CONNECTED && i < WIFI_CONNECTION_TIMEOUT_MS / 500 ; i++)
        delay(500);

    if (callbacks) callbacks->onWifiConnectionResult(WiFi.status());

    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGI(LOG_TAG, "Impossible to connect to '%s'", wifiConfig.ssid);
        ESP_LOGI(LOG_TAG, "Going to deep sleep, goodnight...");
        ESP.deepSleep(WIFI_SLEEP_DURATION_US);
    }

    ESP_LOGI(LOG_TAG, "Successfully connected to '%s'!", wifiConfig.ssid);

    WiFi.onEvent([](WiFiEvent_t event) {
        if (callbacks) callbacks->onWifiDisconnected();
        ESP_LOGE(LOG_TAG, "Lost connection to '%s'", wifiConfig.ssid);
        ESP_LOGE(LOG_TAG, "Rebooting...");
        ESP.restart();
    }, SYSTEM_EVENT_STA_DISCONNECTED);
}

void SailtrackModule::beginOTA() {
    ArduinoOTA
        .onStart([]() {
            if (ArduinoOTA.getCommand() == U_FLASH) ESP_LOGI(LOG_TAG, "Start updating sketch...");
            else ESP_LOGI(LOG_TAG, "Start updating filesystem...");
            esp_mqtt_client_stop(mqttClient);
        })
        .onEnd([]() {
            ESP_LOGI(LOG_TAG, "Update successfully completed!");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            ESP_LOGV(LOG_TAG, "Progress: %u", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
            if (error == OTA_AUTH_ERROR) ESP_LOGE(LOG_TAG, "Error[%u]: Auth Failed", error);
            else if (error == OTA_BEGIN_ERROR) ESP_LOGE(LOG_TAG, "Error[%u]: Begin Failed", error);
            else if (error == OTA_CONNECT_ERROR) ESP_LOGE(LOG_TAG, "Error[%u]: Connect Failed", error);
            else if (error == OTA_RECEIVE_ERROR) ESP_LOGE(LOG_TAG, "Error[%u]: Receive Failed", error);
            else if (error == OTA_END_ERROR) ESP_LOGE(LOG_TAG, "Error[%u]: End Failed", error);
        });
    ArduinoOTA.setHostname(wifiConfig.hostname);
    ArduinoOTA.begin();
    xTaskCreate(otaTask, "otaTask", TASK_MEDIUM_STACK_SIZE, NULL, TASK_MEDIUM_PRIORITY, NULL);
}

void SailtrackModule::beginMqtt() {
    mqttClient = esp_mqtt_client_init(&mqttConfig);
    esp_mqtt_client_start(mqttClient);
    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_CONNECTED, mqttEventHandler, NULL);

    if (callbacks) callbacks->onMqttConnectionBegin();

    ESP_LOGI(LOG_TAG, "Connecting to 'mqtt://%s@%s:%d'...", mqttConfig.username, mqttConfig.host, mqttConfig.port);

    for (int i = 0; !mqttConnected && i < MQTT_CONNECTION_TIMEOUT_MS / 500; i++)
        delay(500);

    if (callbacks) callbacks->onMqttConnectionResult(mqttConnected);

    if (!mqttConnected) {
        ESP_LOGE(LOG_TAG, "Impossible to connect to 'mqtt://%s@%s:%d'", mqttConfig.username, mqttConfig.host, mqttConfig.port);
        ESP_LOGE(LOG_TAG, "Rebooting...");
        ESP.restart();
    }

    ESP_LOGI(LOG_TAG, "Successfully connected to 'mqtt://%s@%s:%d'!", mqttConfig.username, mqttConfig.host, mqttConfig.port);

    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_DATA, mqttEventHandler, NULL);
    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_DISCONNECTED, mqttEventHandler, NULL);
    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_PUBLISHED, mqttEventHandler, NULL);
    xTaskCreate(logTask, "logTask", TASK_MEDIUM_STACK_SIZE, NULL, TASK_LOW_PRIORITY, NULL);
    xTaskCreate(statusTask, "statusTask", TASK_MEDIUM_STACK_SIZE, NULL, TASK_MEDIUM_PRIORITY, NULL);
}

void SailtrackModule::mqttEventHandler(void * handlerArgs, esp_event_base_t base, int32_t eventId, void * eventData) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)eventData;
    switch((esp_mqtt_event_id_t)eventId) {
        case MQTT_EVENT_CONNECTED:
            mqttConnected = true;
            break;
        case MQTT_EVENT_DATA:
            char topic[20];
            char message[500];
            sprintf(topic, "%.*s", event->topic_len, event->topic);
            sprintf(message, "%.*s", event->data_len, event->data);
            receivedMessagesCount++;
            if (callbacks) callbacks->onMqttMessage(topic, message);
            break;
        case MQTT_EVENT_DISCONNECTED:
            if (callbacks) callbacks->onMqttDisconnected();
            ESP_LOGE(LOG_TAG, "Lost connection to 'mqtt://%s@%s:%d'...", mqttConfig.username, mqttConfig.host, mqttConfig.port);
            ESP_LOGE(LOG_TAG, "Rebooting...");
            ESP.restart();
            break;
        case MQTT_EVENT_PUBLISHED:
            publishedMessagesCount++;
            break;
        default:
            break;
    }
}

void SailtrackModule::notificationLedTask(void * pvArguments) {
    while(true) {
        if (WiFi.status() != WL_CONNECTED) {
            digitalWrite(notificationLed, notificationLedReversed ? LOW : HIGH);
            delay(500);
            digitalWrite(notificationLed, notificationLedReversed ? HIGH : LOW);
            delay(500);
        } else {
            digitalWrite(notificationLed, notificationLedReversed ? LOW : HIGH);
            delay(3000);
            digitalWrite(notificationLed, notificationLedReversed ? HIGH : LOW);
            break;
        }
    }
    vTaskDelete(NULL);
}

void SailtrackModule::statusTask(void * pvArguments) {
    char topic[50];
    sprintf(topic, "status/%s", name);

    while(true) {
        if (callbacks) {
            DynamicJsonDocument payload = callbacks->getStatus();
            publish(topic, payload);
        }
        delay(1000 / STATUS_PUBLISH_RATE);
    }
}

void SailtrackModule::logTask(void * pvArguments) {
    while(true) {
        ESP_LOGI(LOG_TAG, "Published messages: %d, Received messages: %d", publishedMessagesCount, receivedMessagesCount);
        delay(1000 / LOG_PUBLISH_RATE);
    }
}

void SailtrackModule::otaTask(void * pvArguments) {
    while (true) {
        ArduinoOTA.handle();
        delay(1000 / OTA_HANDLE_RATE);
    }
}

int SailtrackModule::m_vprintf(const char * format, va_list args) {
    if (mqttConnected) {
        char message[200];
        char topic[50];
        int messageSize;
        sprintf(topic, "log/%s", name);
        vsprintf(message, format, args);
        for (messageSize = 0; message[messageSize]; messageSize++);
        message[messageSize - 1] = 0;
        DynamicJsonDocument payload(500);
        payload["message"] = message;
        publish(topic, payload);
    }
    return vprintf(format, args);
}

int SailtrackModule::publish(const char * topic, DynamicJsonDocument payload) {
    payload["measurement"] = strrchr(strdup(topic), '/') + 1;
    char output[MQTT_OUTPUT_BUFFER_SIZE];
    serializeJson(payload, output);
    return esp_mqtt_client_publish(mqttClient, topic, output, 0, 1, 0);
}

int SailtrackModule::subscribe(const char * topic) {
    return esp_mqtt_client_subscribe(mqttClient, topic, 1);
}
