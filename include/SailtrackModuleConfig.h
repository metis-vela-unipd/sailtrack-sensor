#ifndef SAILTRACK_MODULE_CONFIG_H
#define SAILTRACK_MODULE_CONFIG_H

// ----------------------- Module Configuration ----------------------- //

#define STM_MODULE_NAME_MAX_LENGTH      16

// ------------------- JSON Documents Configuration ------------------- //

#define STM_JSON_DOCUMENT_BIG_SIZE      1024
#define STM_JSON_DOCUMENT_MEDIUM_SIZE   512
#define STM_JSON_DOCUMENT_SMALL_SIZE    256

// ------------------ Notification LED Configuration ------------------ //


//Deletable zone?
#ifndef STM_NOTIFICATION_LED_PIN
#ifdef LED_BUILTIN
#define STM_NOTIFICATION_LED_PIN        LED_BUILTIN
#else
#ifdef BUILTIN_LED
#define STM_NOTIFICATION_LED_PIN        BUILTIN_LED
#endif
#endif
#endif
#ifndef STM_NOTIFICATION_LED_ON_STATE
#define STM_NOTIFICATION_LED_ON_STATE   HIGH
#endif
//End delitable zone?


#define BLINKING_TIME 1000
/*
To use RGB led define NOTIFICATION with this code:
    0x01234567
        01 -> Anode [00] or Catode [01]
        23 -> Red led pin
        45 -> Green led pin
        67 -> blue led pin   
*/

// ----------------------- Tasks Configuration ----------------------- //

#define STM_TASK_HIGH_PRIORITY          3
#define STM_TASK_MEDIUM_PRIORITY        2
#define STM_TASK_LOW_PRIORITY           1

#define STM_TASK_BIG_STACK_SIZE         8192
#define STM_TASK_MEDIUM_STACK_SIZE      4096
#define STM_TASK_SMALL_STACK_SIZE       2048

// ------------------- Library Tasks Configuration ------------------- //

#define STM_STATUS_PUBLISH_FREQ_HZ      0.1
#define STM_LOG_PRINT_FREQ_HZ           0.1
#define STM_OTA_HANDLE_FREQ_HZ          1

#define STM_STATUS_TASK_INTERVAL_MS     1000 / STM_STATUS_PUBLISH_FREQ_HZ
#define STM_LOG_TASK_INTERVAL_MS        1000 / STM_LOG_PRINT_FREQ_HZ
#define STM_OTA_TASK_INTERVAL_MS        1000 / STM_OTA_HANDLE_FREQ_HZ

// ------------------------ WiFi Configuration ------------------------ //

#ifndef STM_WIFI_AP_SSID
#define STM_WIFI_AP_SSID                "SailTrack-CoreNet"
#endif
#ifndef STM_WIFI_AP_PASSWORD
#define STM_WIFI_AP_PASSWORD            "sailtracknet"
#endif
#ifndef STM_WIFI_GATEWAY_ADDR
#define STM_WIFI_GATEWAY_ADDR           "192.168.42.1"
#endif
#ifndef STM_WIFI_SUBNET
#define STM_WIFI_SUBNET                 "255.255.255.0"
#endif

#define STM_WIFI_CONNECTION_TIMEOUT_MS  10 * 1e3
#define STM_WIFI_SLEEP_DURATION_US      60 * 1e6

// ------------------------ MQTT Configuration ------------------------ //

#ifndef STM_MQTT_HOST_ADDR
#define STM_MQTT_HOST_ADDR              STM_WIFI_GATEWAY_ADDR
#endif
#ifndef STM_MQTT_PORT
#define STM_MQTT_PORT                   1883
#endif
#ifndef STM_MQTT_USERNAME
#define STM_MQTT_USERNAME               "mosquitto"
#endif
#ifndef STM_MQTT_PASSWORD
#define STM_MQTT_PASSWORD               "sailtrack"
#endif
#define STM_MQTT_DATA_BUFFER_SIZE       1024
#define STM_MQTT_TOPIC_BUFFER_SIZE      32

#define STM_MQTT_CONNECTION_TIMEOUT_MS  STM_WIFI_CONNECTION_TIMEOUT_MS

#endif
