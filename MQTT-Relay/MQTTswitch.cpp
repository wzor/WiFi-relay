#include "MQTTswitch.h"

int switchCount;
String startupCongigFileName = "/startup.json";

MQTTswitch::MQTTswitch(String feed, String name, uint8_t pin)
{
	switchCount++;
	this->name = name;
	this->index = switchCount;
	this->type = "switch";
	this->pin = pin;
	pinMode(pin, OUTPUT);
	String s = "/switches/" + String(feed) + "/" + String(name);
	feed_sate = (char *)calloc(s.length() + 1, 1);
	strncpy(feed_sate, s.c_str(), s.length());

	s = "/switches/" + String(feed) + "/" + String(name) + "/set";
	feed_set = (char *)calloc(s.length() + 1, 1);
	strncpy(feed_set, s.c_str(), s.length());

	s = "/switches/" + String(feed) + "/" + String(name) + "/available";
	feed_available = (char *)calloc(s.length() + 1, 1);
	strncpy(feed_available, s.c_str(), s.length());

	Serial.printf("feed_sate=%s\n", feed_sate);
	Serial.printf("feed_set=%s\n", feed_set);
	Serial.printf("feed_available=%s\n", feed_available);

}

MQTTswitch::MQTTswitch(char * feed, char * name, uint8_t pin) :MQTTswitch(String(feed), String(name), pin)
{
}


MQTTswitch::~MQTTswitch()
{
}

bool MQTTswitch::process(Adafruit_MQTT_Subscribe * subscription)
{
	if (subscription == onoffbutton_set) {
		String value = String((char *)onoffbutton_set->lastread);
		Serial.print(F("Got: "));
		Serial.println(value);
		setState(value == "ON");
		return true;
	}


	return false;
}

void MQTTswitch::Register(Adafruit_MQTT_Client * connection)
{
	onoffbutton_state = new Adafruit_MQTT_Publish(connection, (const char *)feed_sate);
	onoffbutton_set = new Adafruit_MQTT_Subscribe(connection, (const char *)feed_set);
	onoffbutton_available = new Adafruit_MQTT_Publish(connection, (const char *)feed_available);
	// Setup MQTT subscription for onoff feed.
	connection->subscribe(onoffbutton_set);

}

void MQTTswitch::printInfo(JsonString * ret)
{
	MQTTprocess::printInfo(ret);
	ret->AddValue("state", (state ? "ON" : "OFF"));
	ret->AddValue("index", String(index));
	ret->AddValue("visual", "switch");
}

void MQTTswitch::setState(bool newState)
{
	if (newState) {
		digitalWrite(pin, onPinValue);
		publish_state("ON");
	}
	else {
		digitalWrite(pin, offPinValue);
		publish_state("OFF");
	}
}

bool MQTTswitch::schedule()
{
	ulong m = millis();
	if (lastReport == 0 || (m - lastReport) > repotPeriod) {
		if (publish_available()) lastReport = m;
	}
}

void MQTTswitch::loadStartupStates(MQTTprocess * first)
{
	Serial.println(F("loading startup values\n"));
	if (SPIFFS.exists(startupCongigFileName)) {
		File sv = SPIFFS.open(startupCongigFileName, "r");
		JsonString values = JsonString(sv.readString());
		MQTTprocess * p = first;
		while (p != nullptr) {
			if (p->type = "switch") {
				MQTTswitch * mqttSwitch = (MQTTswitch *)p;
				String v = values.getValue(p->name.c_str());
				mqttSwitch->startupState = (v == "on");
				mqttSwitch->setState(mqttSwitch->startupState);
			}
			p = p->next;
		}
	}
}

void MQTTswitch::saveStartup(MQTTprocess * first)
{
	MQTTprocess * p = first;
	JsonString ret = JsonString();
	ret.beginObject();
	while (p != nullptr) {
		if (p->type = "switch") {
			MQTTswitch * mqttSwitch = (MQTTswitch *)p;
			ret.AddValue(p->name.c_str(), (mqttSwitch->startupState ? "on" : "off"));
		}
		p = p->next;
	}
	ret.endObject();
	File f = SPIFFS.open(startupCongigFileName, "w");
	f.print(ret);
	f.flush();
	f.close();
}



bool MQTTswitch::publish_available() {
	if (mqtt_connection.serverOnline) {
		// Now we can publish stuff!
		if (!onoffbutton_available->publish("online")) {
			Serial.println(F("available - Failed"));
		}
		else {
			Serial.println(F("available - send!"));
			return true;
		}
	}
	return false;
}


bool MQTTswitch::publish_state(const char * state) {
	this->state = (String(state) == "ON");
	if (mqtt_connection.serverOnline) {
		// Now we can publish stuff!
		if (!onoffbutton_state->publish(state)) {
			Serial.printf("Publish %s state - Failse!\n", state);
			return true;
		}
		else {
			Serial.printf("Publised %s state - send!\n", state);
		}
	}
	return false;
}
