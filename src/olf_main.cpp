
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
bool onMqttMessageExt(char *, char *, const AsyncMqttClientMessageProperties &, const size_t &, const size_t &, const size_t &);
void appMQTTTopicSubscribe();
//int sensorRead();
void telnet_extension_1(char);
void telnet_extension_2(char);
void telnet_extensionHelp(char);

// defined in asyncConnect.cpp
extern void mqttTopicsubscribe(const char *topic, int qos);
extern void platform_setup(bool);
extern void handleTelnet();
extern void printTelnet(String);
extern AsyncMqttClient mqttClient;
extern void wifiSetupConfig(bool);
extern bool weekDay;
extern int ohTimenow;
extern int ohTODReceived;

#define DRD_TIMEOUT 3
#define DRD_ADDRESS 0

DoubleResetDetector *drd;

// defined in telnet.cpp
extern int reporting;
extern bool telnetReporting;
//***************************************************************
// Application Specific MQTT Topics and config
//***************************************************************
#define AUTOMODE    0 // Normal running mode - Heating times are based on the 3 time zones
#define NEXTMODE    1 // Advances the control time to the next zone. FIX-THIS: Crap description
#define ONMODE      2   // Permanently ON.  Heat is permanently requested. Zones times are ignored
#define OFFMODE     3  // Permanently OFF.  Heat is never requested. Zones times are ignored

#define SBUNKOWN    0
#define SBON        1
#define SBOFF       2

#define ZONE1       0
#define ZONE2       1
#define ZONE3       2
#define ZONEGAP     9

const char *oh3StateValue   = "/house/ldr/daylight-front/value";
//const char *iotDevice       = "D1-MINI";
//const char *deviceName      = "outside-lights-front";
//const char *deviceType      = "CNTRL";
String deviceName      = "outside-lights-front";
String deviceType      = "CNTRL";

int relay_pin       = D1;		    // wemos D1. LIght on or off (Garden lights)
int relay_pin_pir   = D2;	        // wemos D2. LIght on or off (Garage Path)
int OLFManualStatus = D3;           // Manual over ride.  If low then lights held on manually
int LIGHTSON        = 0;
int LIGHTSOFF       = 1;
int LIGHTSAUTO      = 3;

int SBWD            = SBON;
int SBWE            = SBON;

int ZoneWD          = ZONEGAP;
int ZoneWE          = ZONEGAP;

int RunModeWD       = AUTOMODE;	    // Current run mode for control
int RunModeWE       = AUTOMODE;     // Current run mode for control

bool bManMode = false; // true = Manual, false = automatic


bool TimesReceivedWD = false;
bool TimesReceivedWE = false;

const char *oh3CmdStateWD       = "/house/cntrl/outside-lights-front/wd-command";	    // UI Button press
const char *oh3CmdStateWE       = "/house/cntrl/outside-lights-front/we-command";       // UI Button press
const char *oh3CommandWDTimes   = "/house/cntrl/outside-lights-front/wd-control-times"; // Times message from either UI or Python app
const char *oh3CommandWETimes   = "/house/cntrl/outside-lights-front/we-control-times"; // Times message from either UI or MySQL via Python app
const char *oh3StateRuntime     = "/house/cntrl/outside-lights-front/runtime-state";    // ON, OFF and AUTO
const char *oh3StateManual      = "/house/cntrl/outside-lights-front/manual-state";	    // Status of the Manual control switch control MAN or AUTO
const char *oh3CommandTrigger   = "/house/cntrl/outside-lights-front/pir-command";	    // Event fron the PIR detector (front porch: PIRON or PIROFF


int lcntrlTimesWD[6];																// contains ON and OFF time for the 3 zone periods for upstairs heat request
int lcntrlTimesWE[6];															// contains ON and OFF time for the 3 zone periods for downstairs heat request
char cntrlTimesWD[6][10]{"00:00", "01:00", "02:00", "03:00", "04:00", "06:00"};	// 22/1/2019 how big is each array element? 6 elements each element 10 characters long (9 + 1 for /0)
char cntrlTimesWE[6][10]{"00:00", "01:00", "02:00", "03:00", "04:00", "06:00"}; // 22/1/2019

//************************
// Application specific
//************************
bool OLFControl(int, int);
void processCrtlTimes(char *, char (&ptr)[6][10], int lptr[6]);
bool processCntrlMessage(char *, const char *, const char *, const char *, int, int *, int *, char (&cntrlTimes)[6][10], int *);
bool onORoff();
bool readyCheck();
void debugPrint();


//Ticker sensorReadTimer;
//int sensorValue;

// why is this here... this is for platform config not application config
devConfig espDevice;

void setup()
{
    //***************************************************
    // Set-up Platform - hopefully dont change this
    //***************************************************
    bool configWiFi = false;
    Serial.begin(115200);
    while (!Serial)
        delay(300);

    espDevice.setup(deviceName, deviceType);       // maybe change this to deviceName and deviceType

    Serial.println("\nStarting Daylight detector on ");
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
	OLFControl(relay_pin, LIGHTSOFF);
	OLFControl(relay_pin_pir, LIGHTSOFF);

    // remove below
    //sensorReadTimer.attach(90, sensorRead); // read every 5 mins
}

void loop()
{
    drd->loop();

    // Go look for OTA request
    ArduinoOTA.handle();

    handleTelnet();
}

//****************************************************************
// Process any application specific inbound MQTT messages
// Return False if none
// Return true if an MQTT message was handled here
//****************************************************************
bool onMqttMessageExt(char *topic, char *payload, const AsyncMqttClientMessageProperties &properties, const size_t &len, const size_t &index, const size_t &total)
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



    /****************************************************
	 * Week days Times received
	 * array of three times zones : on,off,on,off,on,off
	 ****************************************************/
	if (strcmp(topic, oh3CommandWDTimes) == 0)
	{
		processCrtlTimes(mqtt_payload, cntrlTimesWD, lcntrlTimesWD);
		TimesReceivedWD = true;
        return true;
	}
	/****************************************************
	 * Week End Times received
	 * array of three times zones : on,off,on,off,on,off
	 ****************************************************/
	else if (strcmp(topic, oh3CommandWETimes) == 0)
	{
		processCrtlTimes(mqtt_payload, cntrlTimesWE, lcntrlTimesWE);
		TimesReceivedWE = true;
		// client.publish(inTopicGateCommandWE, "SET");		// Simulate the SET button being pressed on the UI
        return true;
	}

	/************************************************************************
	 * Weekday Command Control
	 * Message content:
	 * ON :  Set control permanently on. Ignore all time of day settings
	 * OFF:  Set control permanently off. Ignore all time of day settings
	 * ONOFF  : open gates and allow to close normally
	 * NEXT: Move control setting to next time zone
	 * SET:  Set the heat times
	 ************************************************************************/
	else if (strcmp(topic, oh3CmdStateWD) == 0)
	{
		//StateReceivedWD = true;
		if (weekDay == true)
		{
			if (processCntrlMessage(mqtt_payload, "ON", "OFF", oh3CmdStateWD, 0, &SBWD, &RunModeWD, cntrlTimesWD, &ZoneWD) == true)
			{
				Serial.print("ERROR: Unknown message - ");
				Serial.print(mqtt_payload);
				Serial.print(" - received for topic ");
				Serial.println(oh3CmdStateWD);
			}
		}
        return true;
	}
	/************************************************************************
	 * Weekend Command Control
	 * Message content:
	 * ON :  Set control permanently on. Ignore all time of day settings
	 * OFF:  Set control permanently off. Ignore all time of day settings
	 * NEXT: Move control setting to next time zone
	 * SET:  Set the heat times
	 ************************************************************************/
	else if (strcmp(topic, oh3CmdStateWE) == 0)
	{
		//StateReceivedWE = true;
		if (weekDay == false) // false == weekend
		{
			if (processCntrlMessage(mqtt_payload, "ON", "OFF", oh3CmdStateWE, 0, &SBWE, &RunModeWE, cntrlTimesWE, &ZoneWE) == true)
			{
				Serial.print("ERROR: Unknown message - ");
				Serial.print(mqtt_payload);
				Serial.print(" - received for topic ");
				Serial.println(oh3CmdStateWE);
			}
		}
        return true;
	}
	else if (strcmp(topic, oh3CommandTrigger) == 0)
	{
		if (strcmp(mqtt_payload, "PIRON") == 0)
		{
			OLFControl(relay_pin_pir, LIGHTSON);
		}

		if (strcmp(mqtt_payload, "PIROFF") == 0)
		{
			// Switch off unless manually held on by switch
			if (bManMode != true)
				OLFControl(relay_pin_pir, LIGHTSOFF);
		}
		else
		{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s%s %s", "Unknown PIR Command received: ", topic, mqtt_payload);
			mqttLog(logString, true, true);
		}
        return true;
	}
    return false;
}

/************************************************************
 * Process heating control ON, OFF, SET and NEXT messages
 * returns: false = ok
 * 			true  = fail
 ************************************************************/
bool processCntrlMessage(char *mqttMessage,
						 const char *onMessage,
						 const char *offMessage,
						 const char *commandTopic,
						 int demanded,
						 int *sb,
						 int *rm,
						 char (&cntrlTimes)[6][10],
						 int *zone)
{
	if (strcmp(mqttMessage, "ON") == 0)
	{
		*rm = ONMODE;
		OLFControl(relay_pin, LIGHTSON);
		Serial.println("relay_pin -> HIGH");
	}
	else if (strcmp(mqttMessage, "OFF") == 0)
	{
		*rm = OFFMODE;
		OLFControl(relay_pin, LIGHTSOFF);
		Serial.println("relay_pin -> LOW");
	}
	else if (strcmp(mqttMessage, "ONOFF") == 0)
	{
		OLFControl(relay_pin, LIGHTSON);

		Serial.println("relay_pin -> HIGH");
		delay(5000); // FIX this
		OLFControl(relay_pin, LIGHTSOFF);

		Serial.println("relay_pin -> LOW");
		*rm = AUTOMODE;
	}
	else if (strcmp(mqttMessage, "NEXT") == 0)
	{
		debugPrint();
		*rm = NEXTMODE;
		*sb = SBOFF; // Switch back to AUTOMODE when Time of Day is next OFF (don't switch back when this zone ends)
		OLFControl(relay_pin, LIGHTSON);
	}
	else if (strcmp(mqttMessage, "SET") == 0)
	{
		mqttClient.publish(oh3StateManual,1, true, "AUTO");

		Serial.println("processOLFCntrlMessage: SET received.");

		// IF I've pressed SET then check the ON Close time and sent the appropriate message
		*rm = AUTOMODE;
		if (onORoff() == true)
		{
			OLFControl(relay_pin, LIGHTSON);
		}
		else
		{
			OLFControl(relay_pin, LIGHTSOFF);
		}
		// OLFControl(99, LIGHTSAUTO);  // 99 mean not used just setting operating status
	}
	else
	{
		return true;
	}

	return false;
}

//***********************************************************************************
// inStr:   INPUT.  A string containing the times
// ptr:     INPUT.  points to the array of ON and OFF times as characters
// lptr:    OUTPUT. The function populates the array with ON and OFF times as integers
//************************************************************************************
void processCrtlTimes(char *inStr, char (&ptr)[6][10], int lptr[6])
{
	/************************************************************************
	 * Parse the string containing the on and off times into a string array.
	 * e.g. 06:30,09:20,15:10,17:40,20:00,23:50 : This means
	 *
	 *  	[0] 06:30 - ON		Time Zone 1
	 * 		[1] 09:20 - OFF		Time Zone 1
	 * 		[2] 15:10 - ON		Time Zone 2
	 * 		[3] 17:40 - OFF		Time Zone 2
	 * 		[4] 20:00 - ON		Time Zone 3
	 * 		[5] 23:50 - OFF		Time Zone 3
	 *
	 ************************************************************************/
	int i = 0;
	char *pch;
	char timeString[10];

	pch = strtok(inStr, ",");
	while (pch != NULL)
	{
		if (pch != NULL)
		{
			char *output = &ptr[i][0];
			for (int i = 0, j = 0; i < 4; i++, j++) // Evaluate each character in the input
			{
				if (pch[i] != ':')		// If the character is not a space
					output[j] = pch[i]; // Copy that character to the output char[]
				else
					j--; // If it is a space then do not increment the output index (j), the next non-space will be entered at the current index
			}
			output[4] = '\0';

			memset(timeString, 0, sizeof timeString);
			sprintf(timeString, "%s", &ptr[i][0]);
			lptr[i] = atoi(timeString); // Store integer value of time
										// comment out
										// mqttLog(timeString,true);
		}
		pch = strtok(NULL, ",");
		i++;
	}
}

/***********************************************************************
 * Determines if control should be ON or OFF based on current zone times
 * Uses and updates global variables:
 *    weekDay,
 *    lcntrlTimes,
 *    lcntrlTimesWE,
 *    Zone,
 *    ZoneWE.
 *
 * Returns true to switch ON, false to switch OFF
 ************************************************************************/
bool onORoff()
{
	// Serial.println("onORoff");
	// debugPrint();
	if (weekDay == true)
		ZoneWD = ZONEGAP; // not in a zone
	else
		ZoneWE = ZONEGAP;

	bool state = false;

	if (readyCheck() == true) // All conditions to start are met
	{
		/****************************************************************
		 * Work out from the array of longs if the control is On or OFF
		 ****************************************************************/
		// i = 0;
		Serial.print("PS ZONE CONFIG: ");

		for (int i = 0, j = 0; i < 6; i++, j++)
		{
			if (i == 0)
			{
				Serial.print("TOD: ");
				Serial.print((int)ohTimenow);
			}
			// Serial.print (", Zone: ");
			// Serial.print ((int) i - j);
			// Serial.print ("[");
			// Serial.print ( lcntrlTimes[i]);
			// Serial.print (",");
			// Serial.print ( lcntrlTimes[i+1]);
			// Serial.print ("] ");
			// memset(logString,0, sizeof logString);
			// sprintf(logString, "%s,%s,%s,%s[%i: %i: %i]", ntptod, sensorType, sensorName, "ohTimenow: ", ohTimenow, lcntrlTimes[i], lcntrlTimesWE[i] );
			// mqttLog(logString,true);
			if (weekDay == true)
			{
				if (ohTimenow >= lcntrlTimesWD[i] && ohTimenow < lcntrlTimesWD[i + 1])
				{
					state = true; // Switch ON
					ZoneWD = i;	  // Update which zone we are in (there maybe overlapping time zones)
					// memset(logString,0, sizeof logString);
					// sprintf(logString, "%s,%s,%s,%s[%i]", ntptod, sensorType, sensorName, "WD onORoff zone: ", i );
					// mqttLog(logString,true);
					break;
				}
			}
			else
			{
				if (ohTimenow >= lcntrlTimesWE[i] && ohTimenow < lcntrlTimesWE[i + 1])
				{
					state = true; // Switch ON
					ZoneWE = i;	  // Update which zone we are in (there maybe overlapping time zones)
					// memset(logString,0, sizeof logString);
					// sprintf(logString, "%s,%s,%s,%s[%i]", ntptod, sensorType, sensorName, "WE onORoff zone: ", i );
					// mqttLog(logString,true);
					break;
				}
			}

			i++;
		}

		if (state == true)
		{
			// mqttLog("onORoff returns  - ON", true);
			Serial.println(" and set to ON");
		}
		else
		{
			// mqttLog("onORoff returns  - OFF", true);
			Serial.println(" In between zones so set to OFF unless NEXT override");
			// mqttLog("In between zones so set to OFF unless NEXT. If in NEXT then don't switch OFF - wait for next zone to switch OFF", true);
		}
	}
	return state;
}


//*******************************
// Set lights ON or OFF
//*******************************
bool OLFControl(int pin, int state)
{
	if (state == LIGHTSOFF) // HIGH
	{
		mqttClient.publish(oh3StateRuntime,1, true, "OFF");
		digitalWrite(pin, LIGHTSOFF);
	}
	else if (state == LIGHTSON) // LOW
	{
		mqttClient.publish(oh3StateRuntime,1, true, "ON");
		digitalWrite(pin, LIGHTSON);
	}
	else if (state == LIGHTSAUTO) // LOW
	{
		mqttClient.publish(oh3StateRuntime, 1, true, "AUTO");
	}
	else
		return false;
	return true;
}
/*
 * Check that the initialisation data all been received before processing
 * Returns True if all checks have been received
 */
bool readyCheck()
{
	if (ohTODReceived == true && TimesReceivedWD == true && TimesReceivedWE == true)
	{
		return true;
	}
	else
	{
		return false;
	}
}



void debugPrint()
{
	Serial.print("...Week Day Run Mode: ");
	Serial.print((int)RunModeWD);
	Serial.print(", Switch Back: ");
	Serial.print((int)SBWD);
	Serial.print(",  Zone: ");
	Serial.println((int)ZoneWD);

	Serial.print("...Week End Run Mode: ");
	Serial.print((int)RunModeWE);
	Serial.print(", Switch Back: ");
	Serial.print((int)SBWE);
	Serial.print(",  Zone: ");
	Serial.println((int)ZoneWE);
}




// Subscribe to application specific topics
void appMQTTTopicSubscribe()
{
			mqttTopicsubscribe(oh3CmdStateWD, 2);
			mqttTopicsubscribe(oh3CmdStateWE, 2);
			mqttTopicsubscribe(oh3CommandWDTimes, 2);
			mqttTopicsubscribe(oh3CommandWETimes, 2);
			mqttTopicsubscribe(oh3CommandTrigger, 2); 
}

//************************************
//Application specific behaviour
//************************************
// here

//************************************************************************
// Write out over telnet session and application specific infomation
// FIXTHIS: Move to utilities
//***********************************************************************
void telnet_extension_1(char c)
{
    //char logString[MAX_LOGSTRING_LENGTH];
    //memset(logString, 0, sizeof logString);
    //sprintf(logString, "%s%d\n\r", "Sensor Value:\t", sensorValue);
    //printTelnet((String)logString);

   
    String wd;
    String rmWD, rmWE;
    char stringOfTimesWD [70];
    char stringOfTimesWE [70];
    // get weekday  
    wd = "Week Day";
    if (weekDay == false)
        wd = "Weekend";

    // Get on, off times
    sprintf(stringOfTimesWD,"%s,%s,%s,%s,%s,%s",  cntrlTimesWD[0],cntrlTimesWD[1],cntrlTimesWD[2],cntrlTimesWD[3],cntrlTimesWD[4],cntrlTimesWD[5] );
    sprintf(stringOfTimesWE,"%s,%s,%s,%s,%s,%s",  cntrlTimesWE[0],cntrlTimesWE[1],cntrlTimesWE[2],cntrlTimesWE[3],cntrlTimesWE[4],cntrlTimesWE[5] );

    // get Run Mode
    if (RunModeWD == AUTOMODE)
        rmWD ="AUTOMODE";
    else if (RunModeWD == NEXTMODE)
        rmWD ="NEXTMODE";
    else if (RunModeWD == ONMODE)
        rmWD ="ONMODE";
    else if (RunModeWD == OFFMODE)
        rmWD ="OFFMODE";  
    else   
        rmWD = "UNKNOWN";

    if (RunModeWE == AUTOMODE)
        rmWE ="AUTOMODE";
    else if (RunModeWE == NEXTMODE)
        rmWE ="NEXTMODDE";
    else if (RunModeWE == ONMODE)
        rmWE ="ONMODE";
    else if (RunModeWE == OFFMODE)
        rmWE ="ONMODE";  
    else   
        rmWE = "UNKNOWN";

    // Print out on Telnet terminal
    char logString[MAX_LOGSTRING_LENGTH];
    //memset(logString, 0, sizeof logString);
    sprintf(logString, "%s%s\r", "Week Day:\t", wd.c_str());
    printTelnet((String)logString);
    sprintf(logString, "%s%s\r", "WD Times:\t", stringOfTimesWD);
    printTelnet((String)logString);
    sprintf(logString, "%s%s\r", "WE Times:\t", stringOfTimesWE);
    printTelnet((String)logString);
    sprintf(logString, "%s%s\r", "WD Run Mode:\t", rmWD.c_str());
    printTelnet((String)logString);
    sprintf(logString, "%s%s\r", "WE Run Mode:\t", rmWE.c_str());
    printTelnet((String)logString);
}

// Process any application specific telnet commannds
void telnet_extension_2(char c)
{
    printTelnet((String)c);
}

// Process any application specific telnet commannds
void telnet_extensionHelp(char c)
{
    printTelnet((String) "x\t\tSome description0");
}

void drdDetected()
{
    Serial.println("Double resert detected");
}
