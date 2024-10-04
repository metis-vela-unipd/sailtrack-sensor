#ifndef SAILTRACK_MODULE_H
#define SAILTRACK_MODULE_H

#include <Arduino.h>
#include <WiFi.h>
#include <mqtt_client.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include "SailtrackModuleCallbacks.h"
#include "SailtrackModuleConfig.h"

/**
 * Main static class and entrypoint of the `SailtrackModule` library.
 */
class SailtrackModule {
    static char name[];
    static char hostname[];
    static SailtrackModuleCallbacks * callbacks;
    static esp_mqtt_client_handle_t mqttClient;
    static bool mqttConnected;
    static unsigned int publishedMessagesCount;
    static unsigned int receivedMessagesCount;

    static void beginWifi(IPAddress ip);
    static void beginOTA();
    static void beginMqtt();
    static void mqttEventHandler(void * handlerArgs, esp_event_base_t base, int32_t eventId, void * eventData);
    
    #ifdef NOTIFICATION
    static bool isAnode;
    static int red_pin;
    static int green_pin;
    static int blue_pin;
    static void beginNotificationLed();
    static void notificationLedTask(void * pvArguments);
    static int mqttCheck();
    static void color(uint8_t red, uint8_t green, uint8_t blue);
    static void setColor(uint32_t colorCode);
    #endif
    
    static void statusTask(void * pvArguments);
    static void logTask(void * pvArguments);
    static void otaTask(void * pvArguments);

    public:
        /**
         * Initialize the module and the peripherals, connecting it to the WiFi network and the MQTT broker. This method
         * must be called in the `setup()` function in order to correctly use the library.
         * 
         * @param name The name of the module. Used in log messages, netowork names, database entries,... .
         * @param ip The static IP address to assign to the module. Make sure that this IP does not collide with others
         *      in the network.
         * @param callbacks A pointer to the `SailtrackModuleCallbacks` callbacks class. Methods in this class will be
         *      called from inside the library to notify and request values from the main application.
         */
        static void begin(const char * name, IPAddress ip, SailtrackModuleCallbacks * callbacks);

        /**
         * Publish data to MQTT. Use this method to publish a JSON formatted payload to the given MQTT topic.
         * 
         * @param topic The topic `message` will be published to.
         * @param message The message to publish as a JSON object.
         * @return The ID of the published message on success, -1 on failure.
         */
        static int publish(const char * topic, JsonObjectConst message);

        /**
         * Subscribe to a MQTT topic. To notify new messages, the `onMqttMessage(...)` callback will be used.
         * 
         * @param topic The topic name to subscribe to.
         * @return The ID of the subscribe message on success, -1 on failure.
         */ 
        static int subscribe(const char * topic);
};

#endif
