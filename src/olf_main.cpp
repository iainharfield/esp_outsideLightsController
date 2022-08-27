
//#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <Ticker.h>
#include <time.h>
#include <AsyncMqttClient_Generic.hpp>

#define ESP8266_DRD_USE_RTC true
#define ESP_DRD_USE_LITTLEFS false
#define ESP_DRD_USE_SPIFFS false
#define ESP_DRD_USE_EEPROM false
#define DOUBLERESETDETECTOR_DEBUG true
#include <ESP_DoubleResetDetector.h>

#include "defines.h"
#include "utilities.h"

//***********************
// Template functions
//***********************
bool onMqttMessageAppExt(char *, char *, const AsyncMqttClientMessageProperties &, const size_t &, const size_t &, const size_t &);
void appMQTTTopicSubscribe();
//int sensorRead();
void telnet_extension_1(char);
void telnet_extension_2(char);
void telnet_extensionHelp(char);
bool timeReceivedChecker(); 
void app_WD_on();
void app_WE_off();
void app_WD_on();
void app_WE_off();
void app_WD_auto();
void app_WE_auto();

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
//extern bool weekDay;
//extern int ohTimenow;
//extern int ohTODReceived;
extern char ntptod[MAX_CFGSTR_LENGTH];


// defined in cntrl.cpp
//extern int SBWD;
//extern int SBWE;            
//extern int ZoneWD;          
//extern int ZoneWE;         
//extern int RunModeWD;      
//extern int RunModeWE;   
extern void startCntrl();



#define DRD_TIMEOUT 3
#define DRD_ADDRESS 0

DoubleResetDetector *drd;

// defined in telnet.cpp
extern int reporting;
extern bool telnetReporting;


//
// Application specific 
//

String deviceName      	= "outside-lights-front";
String deviceType      	= "CNTRL";
String app_id			= "OLF";						// configure

int relay_pin       = D1;		    // wemos D1. LIght on or off (Garden lights)
int relay_pin_pir   = D2;	        // wemos D2. LIght on or off (Garage Path)
int OLFManualStatus = D3;           // Manual over ride.  If low then lights held on manually
int LIGHTSON        = 0;
int LIGHTSOFF       = 1;
int LIGHTSAUTO      = 3;

bool bManMode       = false; // true = Manual, false = automatic






const char *oh3CommandTrigger   = "/house/cntrl/outside-lights-front/pir-command";	    // Event fron the PIR detector (front porch: PIRON or PIROFF
const char *oh3StateManual = "/house/cntrl/outside-lights-front/manual-state";	 // 	Status of the Manual control switch control MAN or AUTO


//************************
// Application specific
//************************
//bool OLFControl(int, int);
//void processCrtlTimes(char *, char (&ptr)[6][10], int lptr[6]);
//bool processCntrlMessage(char *, const char *, const char *, const char *, int, int *, int *, char (&cntrlTimes)[6][10], int *);
//bool onORoff();

//void debugPrint();
void processAppTOD_Ext();


devConfig espDevice;
//runState cntrlState;

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

    configurationTimesReceived.attach(30, timeReceivedChecker); 


	
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

				app_WD_on();  // FIXTHIS WD or WE
                mqttClient.publish(oh3StateManual, 1, true, "MAN");
	}
	else
	{
		bManMode = false;
				// client.publish(outTopicOLFManual, "AUTO");
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
    char logString[MAX_LOGSTRING_LENGTH];

    char mqtt_payload[len+1];
    mqtt_payload[len] = '\0';
    strncpy(mqtt_payload, payload, len);

    if (reporting == REPORT_DEBUG)
	{
        mqttLog(mqtt_payload, true, true);
    }

	if (strcmp(topic, oh3CommandTrigger) == 0)
	{
		if (strcmp(mqtt_payload, "PIRON") == 0)
		{
			digitalWrite(relay_pin_pir, LIGHTSON);	
            return true;
		}

		if (strcmp(mqtt_payload, "PIROFF") == 0)
		{
			// Switch off unless manually held on by switch
			if (bManMode != true)
				digitalWrite(relay_pin, LIGHTSOFF);	
            return true;
		}
        else
	    {
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s%s %s", "Unknown PIR Command received: ", topic, mqtt_payload);
			mqttLog(logString, true, true);
            return true;
	    }
    }    
	else
	{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s%s %s", "Unknown Command received by App: ", topic, mqtt_payload);
			mqttLog(logString, true, true);
	}
    return false;
}

void processAppTOD_Ext()
{

}



// Subscribe to application specific topics
void appMQTTTopicSubscribe()
{	

	mqttTopicsubscribe(oh3CommandTrigger, 2);
}

void app_WD_on()
{
	digitalWrite(relay_pin, LIGHTSON);
	
}

void app_WD_off()
{
	digitalWrite(relay_pin, LIGHTSOFF);
}

void app_WE_on()
{
	digitalWrite(relay_pin, LIGHTSON);
}

void app_WE_off()
{
	digitalWrite(relay_pin, LIGHTSOFF);	
}
void app_WD_auto()
{

}

void app_WE_auto()
{
	
}