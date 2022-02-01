#include <Arduino.h>

#include <SailtrackModule.h>

class TestCallbacks: public SailtrackModuleCallbacks {
	void onWifiConnectionBegin() {
		// Notify user
	}
	
	void onWifiConnectionResult(wl_status_t status) {
		// Notify user
	}

	DynamicJsonDocument getStatus() {
		// Read module status
		DynamicJsonDocument doc(500);
		return doc;
	}

	void onMqttMessage(const char * topic, const char * message) {
		Serial.println("NEW MESSAGE");
		Serial.printf("Topic: %s, Message: %s\n", topic, message);
	}
};

int counter = 0;

void publishTask(void * pvArguments) {
	while(true) {
		DynamicJsonDocument data(100);
		data["count"] = counter++;
		STModule.publish("sensor/counter0", "counter0", data);
		delay(1000);
	}
}

void setup() {
	STModule.begin("counter", "sailtrack-counter", IPAddress(192, 168, 42, 100));
	STModule.setCallbacks(new TestCallbacks());
	STModule.subscribe("sensor/counter0");
	xTaskCreate(publishTask, "publish_task", 10000, NULL, 1, NULL);
}

void loop() {
}
