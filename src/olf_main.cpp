
// #include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <Ticker.h>
#include <time.h>

#include "defines.h"
#include "utilities.h"
#include "cntrl2.h"

#include <AsyncMqttClient_Generic.hpp>

#define ESP8266_DRD_USE_RTC true
#define ESP_DRD_USE_LITTLEFS false
#define ESP_DRD_USE_SPIFFS false
#define ESP_DRD_USE_EEPROM false
#define DOUBLERESETDETECTOR_DEBUG true
#include <ESP_DoubleResetDetector.h>

//***********************
// Template functions
//***********************
bool onMqttMessageAppExt(char *, char *, const AsyncMqttClientMessageProperties &, const size_t &, const size_t &, const size_t &);
bool onMqttMessageCntrlExt(char *, char *, const AsyncMqttClientMessageProperties &, const size_t &, const size_t &, const size_t &);
void appMQTTTopicSubscribe();
void telnet_extension_1(char);
void telnet_extension_2(char);
void telnet_extensionHelp(char);
void startTimesReceivedChecker();
void processCntrlTOD_Ext();
// void app_WD_on(String);
// void app_WE_off(String);
// void app_WD_on(String);
// void app_WE_off(String);
// void app_WD_auto(String);
// void app_WE_auto(String);

//*************************************
// defined in asyncConnect.cpp
//*************************************
extern void mqttTopicsubscribe(const char *topic, int qos);
extern void platform_setup(bool);
extern void handleTelnet();
extern void printTelnet(String);
extern AsyncMqttClient mqttClient;
extern void wifiSetupConfig(bool);
extern templateServices coreServices;
extern char ntptod[MAX_CFGSTR_LENGTH];

//*************************************
// defined in cntrl.cpp
//*************************************
extern cntrlState *cntrlObjRef; // pointer to cntrlStateOLF
cntrlState cntrlStateOLF;		// Create and set defaults

#define WDCntlTimes "/house/cntrl/outside-lights-front/wd-control-times" // Times received from either UI or Python app
#define WECntlTimes "/house/cntrl/outside-lights-front/we-control-times" // Times received from either UI or MySQL via Python app
#define runtimeState "/house/cntrl/outside-lights-front/runtime-state"	 // published state: ON, OFF, and AUTO
#define WDUICmdState "/house/cntrl/outside-lights-front/wd-command"		 // UI Button press received
#define WEUICmdState "/house/cntrl/outside-lights-front/we-command"		 // UI Button press received
#define RefreshID "OLF"													 // the key send to Python app to refresh Cntroler state

#define DRD_TIMEOUT 3
#define DRD_ADDRESS 0

DoubleResetDetector *drd;

// defined in telnet.cpp
extern int reporting;
extern bool telnetReporting;

//
// Application specific
//

String deviceName = "outside-lights-front";
String deviceType = "CNTRL";
String app_id = "OLF"; // configure

int relay_pin = D1;		  // wemos D1. LIght on or off (Garden lights)
int relay_pin_pir = D2;	  // wemos D2. LIght on or off (Garage Path)
int OLFManualStatus = D3; // Manual over ride.  If low then lights held on manually
int LIGHTSON = 0;
int LIGHTSOFF = 1;
int LIGHTSAUTO = 3; // Not using this at the moment

bool bManMode = false; // true = Manual, false = automatic

const char *oh3CommandTrigger = "/house/cntrl/outside-lights-front/pir-command"; // Event fron the PIR detector (front porch: PIRON or PIROFF
const char *oh3StateManual = "/house/cntrl/outside-lights-front/manual-state";	 // 	Status of the Manual control switch control MAN or AUTO

//************************
// Application specific
//************************
bool processCntrlMessageApp_Ext(char *, const char *, const char *, const char *);
void processAppTOD_Ext();

devConfig espDevice;
// runState cntrlState;

Ticker configurationTimesReceived;
bool timesReceived;

void setup()
{
	//***************************************************
	// Set-up Platform - hopefully dont change this
	//***************************************************
	bool configWiFi = false;
	Serial.begin(115200);
	while (!Serial)
		delay(300);

	espDevice.setup(deviceName, deviceType);
	Serial.println("\nStarting Outside lights Controller on ");
	Serial.println(ARDUINO_BOARD);

	drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
	if (drd->detectDoubleReset())
	{
		configWiFi = true;
	}

	// this app is a contoller
	// configure the MQTT topics for the Controller
	cntrlStateOLF.setCntrlName((String)app_id);
	cntrlStateOLF.setRefreshID(RefreshID);
	cntrlStateOLF.setCntrlObjRef(cntrlStateOLF);

	// startCntrl();

	// Platform setup: Set up and manage: WiFi, MQTT and Telnet
	platform_setup(configWiFi);

	//***********************
	// Application setup
	//***********************

	pinMode(relay_pin, OUTPUT);
	pinMode(relay_pin_pir, OUTPUT);
	pinMode(OLFManualStatus, INPUT);
	digitalWrite(relay_pin, LIGHTSOFF);
	digitalWrite(relay_pin_pir, LIGHTSOFF);

	configurationTimesReceived.attach(30, startTimesReceivedChecker);
}

void loop()
{
	int pinVal = 0;
	char logString[MAX_LOGSTRING_LENGTH];
	drd->loop();

	// Go look for OTA request
	ArduinoOTA.handle();

	handleTelnet();

	pinVal = digitalRead(OLFManualStatus);
	if (pinVal == 0) // means manual switch ON and lights forced to stay on
	{
		bManMode = true;
		memset(logString, 0, sizeof logString);
		sprintf(logString, "%s,%s,%s,%s", ntptod, espDevice.getType().c_str(), espDevice.getName().c_str(), "Outside Lights Manually Held ON");
		mqttLog(logString, true, true);

		app_WD_on(&cntrlStateOLF); // FIXTHIS WD or WE
		mqttClient.publish(oh3StateManual, 1, true, "MAN");
	}
	else
	{
		bManMode = false;
		// mqttClient.publish(oh3StateManual, 1, true, "AUTO"); //FIXTHIS  I thing we cant assume auto - need to get state prior

		//	if (TODReceived == true)
		//		processState();
	}
}

//****************************************************************
// Process any application specific inbound MQTT messages
// Return False if none
// Return true if an MQTT message was handled here
//****************************************************************
bool onMqttMessageAppExt(char *topic, char *payload, const AsyncMqttClientMessageProperties &properties, const size_t &len, const size_t &index, const size_t &total)
{
	(void)payload;
	// char logString[MAX_LOGSTRING_LENGTH];

	char mqtt_payload[len + 1];
	mqtt_payload[len] = '\0';
	strncpy(mqtt_payload, payload, len);

	// if (reporting == REPORT_DEBUG)
	//{
	mqttLog(mqtt_payload, true, true);
	//}

	if (strcmp(topic, oh3CommandTrigger) == 0)
	{
		if (strcmp(mqtt_payload, "PIRON") == 0)
		{
			digitalWrite(relay_pin_pir, LIGHTSON);
			digitalWrite(relay_pin, LIGHTSON);
			return true;
		}

		if (strcmp(mqtt_payload, "PIROFF") == 0)
		{
			// if (cntrlStateWD.getRunMode() != ONMODE && cntrlStateWE.getRunMode() != ONMODE)
			//{
			//  Switch off unless manually held on by switch
			if (bManMode != true)
			{ // FIXTHIS - Both off ?
				digitalWrite(relay_pin_pir, LIGHTSOFF);
				digitalWrite(relay_pin, LIGHTSOFF);
			}
			//}
			return true;
		}
		/*else
		{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s%s %s", "Unknown PIR Command received: ", topic, mqtt_payload);
			mqttLog(logString, true, true);
			return true;
		} */
	}
	/*else
	{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s%s %s", "Unknown Command received by App: ", topic, mqtt_payload);
			mqttLog(logString, true, true);
	}*/
	return false;
}

void processAppTOD_Ext()
{
	mqttLog("OLF Application Processing TOD", true, true);
}

bool processCntrlMessageApp_Ext(char *mqttMessage, const char *onMessage, const char *offMessage, const char *commandTopic)
{
	if (strcmp(mqttMessage, "SET") == 0)
	{
		mqttClient.publish(oh3StateManual, 1, true, "AUTO"); // This just sets the UI to show that MAN start is OFF
		return true;
	}
	return false;
}
//***************************************************
// Connected to MQTT Broker
// Subscribe to application specific topics
//***************************************************
void appMQTTTopicSubscribe()
{

	mqttTopicsubscribe(oh3CommandTrigger, 2);

	cntrlStateOLF.setWDCntrlTimesTopic(WDCntlTimes);
	cntrlStateOLF.setWDUIcommandStateTopic(WDUICmdState);
	cntrlStateOLF.setWDCntrlRunTimesStateTopic(runtimeState);

	cntrlStateOLF.setWECntrlTimesTopic(WECntlTimes);
	cntrlStateOLF.setWEUIcommandStateTopic(WEUICmdState);
	cntrlStateOLF.setWECntrlRunTimesStateTopic(runtimeState);
}

void app_WD_on(void *cid)
{
	digitalWrite(relay_pin, LIGHTSON);

	cntrlState *obj = (cntrlState *)cid;
	String msg = obj->getCntrlName() + " WD ON";
	mqttLog(msg.c_str(), true, true);
}

void app_WD_off(void *cid)
{
	digitalWrite(relay_pin, LIGHTSOFF);

	cntrlState *obj = (cntrlState *)cid;
	String msg = obj->getCntrlName() + " WD OFF";
	mqttLog(msg.c_str(), true, true);
}

void app_WE_on(void *cid)
{
	digitalWrite(relay_pin, LIGHTSON);

	cntrlState *obj = (cntrlState *)cid;
	String msg = obj->getCntrlName() + " WE ON";
	mqttLog(msg.c_str(), true, true);
}

void app_WE_off(void *cid)
{
	digitalWrite(relay_pin, LIGHTSOFF);

	cntrlState *obj = (cntrlState *)cid;
	String msg = obj->getCntrlName() + " WE OFF";
	mqttLog(msg.c_str(), true, true);
}
void app_WD_auto(void *cid)
{
	cntrlState *obj = (cntrlState *)cid;
	String msg = obj->getCntrlName() + " WD AUTO";
	mqttLog(msg.c_str(), true, true);
}

void app_WE_auto(void *cid)
{
	cntrlState *obj = (cntrlState *)cid;
	String msg = obj->getCntrlName() + " WE AUTO";
	mqttLog(msg.c_str(), true, true);
}

void startTimesReceivedChecker()
{
	cntrlStateOLF.runTimeReceivedCheck();
}

void processCntrlTOD_Ext()
{
	cntrlStateOLF.processCntrlTOD_Ext();
}
void telnet_extension_1(char c)
{
	cntrlStateOLF.telnet_extension_1(c);
}
// Process any application specific telnet commannds
void telnet_extension_2(char c)
{
	printTelnet((String)c);
}

// Process any application specific telnet commannds
void telnet_extensionHelp(char c)
{
	printTelnet((String) "x\t\tSome description");
}

bool onMqttMessageCntrlExt(char *topic, char *payload, const AsyncMqttClientMessageProperties &properties, const size_t &len, const size_t &index, const size_t &total)
{
	return cntrlStateOLF.onMqttMessageCntrlExt(topic, payload, properties, len, index, total);
}
