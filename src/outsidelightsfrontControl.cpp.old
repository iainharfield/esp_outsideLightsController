/***************************************************
 *  MQTT Library ESP8266 Example
 ***************************************************/
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>

void callback(char *, byte *, unsigned int);
bool ScanAndConnect(void);
bool mqttLog(char *, bool);
void handleTelnet();
String getFormattedTime();
int get_weekday(char *);
void MQTT_connect();
bool OLFControl(int, int);
bool readyCheck();
bool onORoff();
int get_weekday(char *);
void processState();
void debugPrint();
bool processCntrlMessage(char *, const char *, const char *, const char *, int, int *, int *, char (&cntrlTimes)[6][10], int *);
void processCrtlTimes(char *, char (&ptr)[6][10], int lptr[6]);

/*********************************
 * WiFi Access Point
 *********************************/
char ssid[100];
const char pass[] = "harfieldhomes";

/*********************************
 * MQTT Setup
 *********************************/

#define MQTT_SERVER "192.168.1.130"
#define MQTT_SERVERPORT 1883 // use 8883 for SSL

#define AUTOMODE 0 // Normal running mode - Heating times are based on the 3 time zones
#define NEXTMODE 1 // Advances the control time to the next zone. FIX-THIS: Crap description
#define ONMODE 2   // Permanently ON.  Heat is permanently requested. Zones times are ignored
#define OFFMODE 3  // Permanently OFF.  Heat is never requested. Zones times are ignored

#define SBUNKOWN 0
#define SBON 1
#define SBOFF 2

#define ZONE1 0
#define ZONE2 1
#define ZONE3 2
#define ZONEGAP 9

#define delayTimeState 400000 // about 3 seconds
#define delayNTPQuery 10

#define MAX_CFGSTR_LENGTH 61
char mqttClientID[MAX_CFGSTR_LENGTH]; // Default this to something unique

int delayCounterState = 0;
int delayNTPQueryCounter = 0;

bool bManMode = false; // true = Manual, false = automatic

char ntptod[50];

const char *iotDevice = "D1-MINI";
const char *sensorName = "outside-lights-front";
const char *sensorType = "CNTRL";

const char *ntpPool = "pool.ntp.org";

int RunMode = AUTOMODE;	  // Current run mode for control
int RunModeWE = AUTOMODE; // Current run mode for control

int SB = SBON;
int SBWE = SBON;

int Zone = ZONEGAP;
int ZoneWE = ZONEGAP;

int day = -1; // 0=Sun 1=Mon, 2=Tue 3=Wed, 4=Thu 5=Fri, 6=Sat
bool weekDay = true;
char str[10];

char ipAddr[31];
char connectedSSID[100];

#define REPORT_INFO 0
#define REPORT_WARN 1
#define REPORT_DEBUG 2
int reporting = REPORT_INFO;

// const char* outTopicIdentity 		= "/house/outside/cntrl/front-outside-lights/identity";
// const char* outTopicState 			= "/house/outside/cntrl/front-outside-lights/State";
// const char* inTopicCntrlOLFTimes   	= "OLFCntrlTimes";		// Times message from either UI or Python app
// const char* inTopicCntrlOLFTimesWE 	= "OLFCntrlTimesWE";	// Times message from either UI or Python app
// const char* inTopicOLFCommand   	= "OLFCommand";			// UI Button press
// const char* inTopicOLFCommandWE 	= "OLFCommandWE";		// UI Button press
// const char* inTopicOLFPIRCommand   	= "OLFPIRCommand";		// PIR detection state
// const char* outTopicLog				= "CHLog";				// Log messages
// const char* outTopicOLFManual		= "OLFManualState";		// Status - Manual control switch control MAN or AUTO
// const char* inTopicTOD 				= "CHToD";  			// Time of day broadcast from OpenHab
// const char* inTopicIOT 				= "IOTCommand";			// Generic commands for IOT

const char *oh3CommandIOT = "/house/service/iot-command";			  // e.g. IOT-IDENTITY, IOT-RESET, IOT-RESTART, IOT-SWITCH-CONFIG
const char *oh3StateLog = "/house/service/log";						  // Log messages
const char *oh3CommandTOD = "/house/service/time-of-day";			  // Time of day broadcast from OpenHab
const char *oh3StateIOTRefresh = "/house/service/iot-device-refresh"; // Request Refresh

const char *oh3StateRuntime = "/house/cntrl/outside-lights-front/runtime-state"; // e.g. ON, OFF and AUTO
const char *oh3StateManual = "/house/cntrl/outside-lights-front/manual-state";	 // 	Status of the Manual control switch control MAN or AUTO
const char *oh3StateIdentity = "/house/cntrl/outside-lights-front/identity";
const char *oh3CmdStateWD = "/house/cntrl/outside-lights-front/wd-command";			  // UI Button press
const char *oh3CmdStateWE = "/house/cntrl/outside-lights-front/we-command";			  // UI Button press
const char *oh3CommandWDTimes = "/house/cntrl/outside-lights-front/wd-control-times"; // Times message from either UI or Python app
const char *oh3CommandWETimes = "/house/cntrl/outside-lights-front/we-control-times"; // Times message from either UI or MySQL via Python app
const char *oh3CommandTrigger = "/house/cntrl/outside-lights-front/pir-command";	  // Event fron the PIR dector (front porch: PIRON or PIROFF

// const char* OLFState 				= "State";				// Request Lights state from MySQL

int relay_pin = D1;		  // wemos D1. LIght on or off (Garden lights)
int relay_pin_pir = D2;	  // wemos D2. LIght on or off (Garage Path)
int OLFManualStatus = D3; // Manual over ride.  If low then lights held on manually
int LIGHTSON = 0;
int LIGHTSOFF = 1;
int LIGHTSAUTO = 3;

bool TODReceived = false;

bool TimesReceived = false;
bool TimesReceivedWE = false;
bool StateReceived = false;	  // Set to true when a CHtgUpCntrl message has been received (SET,ON,OFF,NEXT)
bool StateReceivedWE = false; // Set to true when a CHtgUpCntrl message has been received (SET,ON,OFF,NEXT)

/*********************************
 * Processing of ON and OFF times
 *********************************/

int timenow = 0;
int lcntrlTimes[6];																// contains ON and OFF time for the 3 zone periods for upstairs heat request
int lcntrlTimesWE[6];															// contains ON and OFF time for the 3 zone periods for downstairs heat request
char cntrlTimes[6][10]{"00:00", "01:00", "02:00", "03:00", "04:00", "06:00"};	// 22/1/2019 how big is each array element? 6 elements each element 10 characters long (9 + 1 for /0)
char cntrlTimesWE[6][10]{"00:00", "01:00", "02:00", "03:00", "04:00", "06:00"}; // 22/1/2019

/********************************************************************
 * Create an ESP8266 WiFiClient class to connect to the MQTT server.
 ********************************************************************/
WiFiClient wifiClient;
PubSubClient client(wifiClient);

/*
 * Set-up NTP client to get time of day directly and not via OpenHab
 */
WiFiUDP ntpUDP;
// By default 'pool.ntp.org' is used with 60 seconds update interval and no offset
NTPClient timeClient(ntpUDP, ntpPool);

// declare telnet server (do NOT put in setup())
WiFiServer TelnetServer(23);
WiFiClient Telnet;

char logString[501];

// Set default LWT
const char *lwm = "Offline";
char willTopic[51];

WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;

/*
 * Main Entry Point
 */
void setup()
{

	memset(logString, 0, sizeof logString);

	sprintf(ntptod, "%s", "0000-00-00T00:00:00:00");

	pinMode(relay_pin, OUTPUT);
	// pinMode(relay_pin2, OUTPUT);
	pinMode(relay_pin_pir, OUTPUT);
	pinMode(OLFManualStatus, INPUT);

	OLFControl(relay_pin, LIGHTSOFF);
	// OLFControl(relay_pin2, LIGHTSOFF);
	OLFControl(relay_pin_pir, LIGHTSOFF);

	Serial.begin(115200);
	delay(10);

	gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP &event)
												{
		Serial.print("EVENT: Station connected, IP: ");
    	Serial.println(WiFi.localIP());
    	strcpy(connectedSSID, WiFi.SSID().c_str());
    	sprintf(ipAddr, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] ); });

	disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &event)
															  { Serial.println("EVENT: Station disconnected"); });

	Serial.println(F("Starting Outside Lights Front WiFi Control"));

	memset(willTopic, 0, sizeof willTopic);
	sprintf(willTopic, "/house/cntrl/%s/lwt", sensorName);

	/*******************************************************************************************
	 * Connect to strongest WiFi.  Assumes for any network the password is the same.
	 * FIX-THIS.  At the moment it will try and connect to any WiFi - might be a neighbours!
	 *******************************************************************************************/
	// WiFi.disconnect(0);
	if (WiFi.status() != WL_CONNECTED)
	{
		Serial.println("Not connected to WiFi");
		ScanAndConnect();
	}

	/*
	 * Setup MQTT
	 */
	client.setServer(MQTT_SERVER, MQTT_SERVERPORT);
	client.setCallback(callback);
	client.disconnect(); // Disconnect from broaker of start-up in case of existing connections

	TelnetServer.begin();
	Serial.println("Starting telnet server");

	/************************
	 * Initialise UI state
	 ************************/

	/*
	 * OTA setup
	 */

	ArduinoOTA.onStart([]()
					   { Serial.println("Start"); });

	ArduinoOTA.onEnd([]()
					 { Serial.println("\nEnd"); });

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
						  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
	ArduinoOTA.onError([](ota_error_t error)
					   {
  	    Serial.printf("Error[%u]: ", error);
  	    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
  	    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
  	    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
  	    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
  	    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
	ArduinoOTA.begin();
	Serial.println("OTA Ready");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	// mqttLog("OTA: Garden Lights Front Controller OTA Ready",true);
	sprintf(logString, "OTA: Outside Lights Front Controller IP Address: %d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
	Serial.println(logString);
	mqttLog(logString, true);

	timeClient.begin();
	if (!timeClient.update())
		timeClient.forceUpdate();
}

uint32_t x = 0;

void loop()
{
	// Go look for OTA request
	ArduinoOTA.handle();

	handleTelnet();

	// Ensure the connection to the MQTT server is alive (this will make the first
	// connection and automatically reconnect when disconnected).  See the MQTT_connect
	// function definition further below.

	int pinVal = 0;

	if (!client.connected())
	{
		memset(mqttClientID, 0, sizeof mqttClientID);
		sprintf(mqttClientID, "%s:%s", sensorName, WiFi.macAddress().c_str());

		if (WiFi.status() == WL_CONNECTED)
		{

			// Connect to MQTT if connected to WiFi
			// Sometimes ScanAndConncet returns an ipAddr of 0.0.0.0
			// if ( (WiFi.status() == WL_CONNECTED) && (strcmp(ipAddr,"0.0.0.0")) !=0 )
			int clientState;
			clientState = client.state();
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%i", ntptod, sensorType, "MQTT Broker connection state: ", clientState);
			mqttLog(logString, true);

			sprintf(logString, "%s,%i", "MQTT Broker connection state: ", clientState);
			Telnet.println(logString);

			Serial.print("LOOP: Connecting to MQTT broker with client ID of ");
			Serial.println(mqttClientID);

			MQTT_connect();
		}
		else
		{
			sprintf(ipAddr, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);

			if (strcmp(ipAddr, "0.0.0.0") == 0)
			{
				// Serial.println("Loop: Reser Wifi Connection as ipAddr =  0.0.0.0");
				// Telnet.println("Restarting");
				// delay(1000);
				ESP.restart();
				// Serial.println("Loop: Reser Wifi Connection as ipAddr =  0.0.0.0");
				// delay(500);
				// ScanAndConnect();
			}
			else
			{
				Serial.println("Loop: Not connected to Wifi");
				delay(500);
				ScanAndConnect();
			}
		}
	}
	else
	{
		if (delayCounterState > delayTimeState)
		{
			if (!timeClient.update()) // Calling this roughly every 2s - is this too often?
				timeClient.forceUpdate();

			memset(ntptod, 0, sizeof ntptod);
			sprintf(ntptod, "%s", getFormattedTime().c_str());

			if (TimesReceived == false || TimesReceivedWE == false || StateReceived == false || StateReceivedWE == false || TODReceived == false)
			{
				memset(logString, 0, sizeof logString);
				sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "Waiting for all initialisation messages to be received");
				mqttLog(logString, true);

				if (TODReceived == false)
				{
					memset(logString, 0, sizeof logString);
					sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "Time of Day not yet received");
					mqttLog(logString, true);
				}
				if (TimesReceived == false)
				{
					memset(logString, 0, sizeof logString);
					sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "Weekday control times not yet received");
					mqttLog(logString, true);
				}
				if (TimesReceivedWE == false)
				{
					memset(logString, 0, sizeof logString);
					sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "Weekend control times not yet received");
					mqttLog(logString, true);
				}
				if (StateReceived == false && weekDay == true)
				{
					memset(logString, 0, sizeof logString);
					sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "Weekday operating mode not yet received");
					mqttLog(logString, true);
				}
				if (StateReceivedWE == false && weekDay == false)
				{
					memset(logString, 0, sizeof logString);
					sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "Weekend operating mode not yet received");
					mqttLog(logString, true);
				}

				client.publish(oh3StateIOTRefresh, "OLF");
			}

			pinVal = digitalRead(OLFManualStatus);
			if (pinVal == 0) // means manual switch ON and lights forced to stay on
			{
				bManMode = true;
				memset(logString, 0, sizeof logString);
				sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "Manually held ON");
				mqttLog(logString, true);

				OLFControl(relay_pin, LIGHTSON);
				// OLFControl(relay_pin2, LIGHTSON);
				client.publish(oh3StateManual, "MAN");
			}
			else
			{
				bManMode = false;
				// client.publish(outTopicOLFManual, "AUTO");
				if (TODReceived == true)
					processState();
			}
			delayCounterState = 0;
		}
	}
	delayCounterState++;

	client.loop(); // look for MQTT inbound messages.
}

/*
 * Callback for incoming MQTT message
 */
void callback(char *topic, byte *payload, unsigned int length)
{
	unsigned int i = 0;
	// char * pch;
	long timeNow[4];	 // Contains Current time of day [HH],[MM]
	char iotCmd[10][21]; // 10 strings each 20 chars long

	// save the topic command
	char str2[100]; // FIX THIS
	memset(str2, 0, sizeof str2);
	for (i = 0; i < length; i++)
	{
		Serial.print((char)payload[i]);
		str2[i] = payload[i];
	}

	// save the topic
	char mqttTopic[100]; // FIX THIS
	memset(mqttTopic, 0, sizeof mqttTopic);
	sprintf(mqttTopic, "%s", topic);

	char logString[501];
	memset(logString, 0, sizeof logString);
    if (reporting == REPORT_DEBUG)
	{
	   sprintf(logString, "%s,%s,%s,%s[%s], %s %s", ntptod, sensorType, sensorName, "MQTT message received. Topic: ", mqttTopic, "Message:", str2);
	   mqttLog(logString, true);
	}
	/******************************************
	 * Time of day event
	 *     Process in times
	 *     process is in NEXT mode
	 ******************************************/
	if (strcmp(mqttTopic, oh3CommandTOD) == 0)
	{
		// debugPrint();

		// parse the time to get the hour

		char str[10];
		// char* ppch;

		day = get_weekday(str2);
		sprintf(str, "%d", day);
		weekDay = false;
		if (day >= 1 && day <= 5)
			weekDay = true;

		// memset(logString,0, sizeof logString);
		// sprintf(logString, "%s,%s,%s,%s[%s]", ntptod, sensorType, sensorName, "TOD str2: ", str2  );
		// mqttLog(logString,true);

		int day, year, month, hr, min, sec;
		sscanf(str2, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hr, &min, &sec);

		// memset(logString,0, sizeof logString);
		// sprintf(logString, "%s,%i,%i,%i,%i,%i,%i", "TOD sscanf: ", year, month, day, hr, min, sec );
		// mqttLog(logString,true);

		timeNow[0] = hr;
		timeNow[1] = min;

		timenow = (timeNow[0] * 100) + timeNow[1];

		// memset(logString,0, sizeof logString);
		// sprintf(logString, "%s,%s,%s,%s[%i : %i : %i]", ntptod, sensorType, sensorName, "TOD timenow: ", timenow, timeNow[0], timeNow[1]  );
		// mqttLog(logString,true);

		TODReceived = true;
		processState();
	}
	/****************************************************
	 * Week days Times received
	 * array of three times zones : on,off,on,off,on,off
	 ****************************************************/
	else if (strcmp(mqttTopic, oh3CommandWDTimes) == 0)
	{
		processCrtlTimes(str2, cntrlTimes, lcntrlTimes);
		TimesReceived = true;
	}
	/****************************************************
	 * Week End Times received
	 * array of three times zones : on,off,on,off,on,off
	 ****************************************************/
	else if (strcmp(mqttTopic, oh3CommandWETimes) == 0)
	{
		processCrtlTimes(str2, cntrlTimesWE, lcntrlTimesWE);
		TimesReceivedWE = true;
		// client.publish(inTopicGateCommandWE, "SET");		// Simulate the SET button being pressed on the UI
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
	else if (strcmp(mqttTopic, oh3CmdStateWD) == 0)
	{
		StateReceived = true;
		if (weekDay == true)
		{
			if (processCntrlMessage(str2, "ON", "OFF", oh3CmdStateWD, 0, &SB, &RunMode, cntrlTimes, &Zone) == true)
			{
				Serial.print("ERROR: Unknown message - ");
				Serial.print(str2);
				Serial.print(" - received for topic ");
				Serial.println(oh3CmdStateWD);
			}
		}
	}
	/************************************************************************
	 * Weekend Command Control
	 * Message content:
	 * ON :  Set control permanently on. Ignore all time of day settings
	 * OFF:  Set control permanently off. Ignore all time of day settings
	 * NEXT: Move control setting to next time zone
	 * SET:  Set the heat times
	 ************************************************************************/
	else if (strcmp(mqttTopic, oh3CmdStateWE) == 0)
	{
		StateReceivedWE = true;
		if (weekDay == false) // false == weekend
		{
			if (processCntrlMessage(str2, "ON", "OFF", oh3CmdStateWE, 0, &SBWE, &RunModeWE, cntrlTimesWE, &ZoneWE) == true)
			{
				Serial.print("ERROR: Unknown message - ");
				Serial.print(str2);
				Serial.print(" - received for topic ");
				Serial.println(oh3CmdStateWE);
			}
		}
	}
	else if (strcmp(mqttTopic, oh3CommandTrigger) == 0)
	{
		if (strcmp(str2, "PIRON") == 0)
		{
			OLFControl(relay_pin_pir, LIGHTSON);
		}

		if (strcmp(str2, "PIROFF") == 0)
		{
			// Switch off unless manually held on by switch
			if (bManMode != true)
				OLFControl(relay_pin_pir, LIGHTSOFF);
		}
		else
		{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%s%s %s", ntptod, sensorType, sensorName, "Unknown PIR Command received: ", mqttTopic, str2);
			mqttLog(logString, true);
		}
	}
	else if (strcmp(mqttTopic, oh3CommandIOT) == 0)
	{
		int i = 0;
		char *pch;

		/*
		 * Parse the string into an array of strings
		 */
		pch = strtok(str2, ","); // Get the fist string
		while (pch != NULL)
		{
			memset(iotCmd[i], 0, sizeof iotCmd[i]);
			strcpy(iotCmd[i], pch);
			pch = strtok(NULL, ",");
			// mqttLog(iotCmd[i], true);
			i++;
		}
		if (strcmp(iotCmd[0], "IOT-IDENTITY") == 0) //
		{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%s%s,%s%s", ntptod, sensorType, sensorName, "ipAddr:", ipAddr, "SSID:", connectedSSID);
			mqttLog(logString, true);

			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%s,%s", iotDevice, sensorType, sensorName, ipAddr, connectedSSID);
			client.publish(oh3StateIdentity, logString);
		}
		if (strcmp(iotCmd[0], "IOT-RESET") == 0) //
		{
			if (!strcmp(iotCmd[1], ipAddr))
			{
				memset(logString, 0, sizeof logString);
				sprintf(logString, "%s,%s,%s,%s%s,%s%s", ntptod, sensorType, sensorName, "ipAddr:", ipAddr, "SSID:", connectedSSID);
				mqttLog(logString, true);
				// delay (2);
				ESP.restart();
			}
		}
	}
	else
	{
		memset(logString, 0, sizeof logString);
		sprintf(logString, "%s,%s,%s,%s%s %s", ntptod, sensorType, sensorName, "Unknown message received: ", mqttTopic, str2);
		mqttLog(logString, true);
	}
}

void processState()
{

	/**************************************************************************
	 * Check every time Time of Day event is received from OpenHab.
	 * Send appropriate MQTT command for each control.
	 **************************************************************************/
	if (RunMode == AUTOMODE && weekDay == true)
	{
		if (onORoff() == true)
		{
			OLFControl(relay_pin, LIGHTSON);
			// memset(logString,0, sizeof logString);
			// sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "WD AUTO mode and ON" );
			// mqttLog(logString,true);
		}
		else
		{
			OLFControl(relay_pin, LIGHTSOFF);
			// memset(logString,0, sizeof logString);
			// sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "WD AUTO mode and OFF" );
			// mqttLog(logString,true);
		}
	}
	else if (RunModeWE == AUTOMODE && weekDay == false)
	{

		if (onORoff() == true)
		{
			OLFControl(relay_pin, LIGHTSON);
			// memset(logString,0, sizeof logString);
			// sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "WE AUTO mode and ON" );
			// mqttLog(logString,true);
		}
		else
		{
			OLFControl(relay_pin, LIGHTSOFF);
			// memset(logString,0, sizeof logString);
			// sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "WE AUTO mode and OFF" );
			// mqttLog(logString,true);
		}
	}

	else if (RunMode == NEXTMODE && weekDay == true)
	{
		// onORoff updates zone to the zone currently in
		if (SB == SBOFF && Zone == ZONEGAP)
		{
			if (onORoff() == true) // moved into an on zone
				SB = SBON;		   // allow switch back to occur at end of zone period
		}
		else if (SB == SBON)
		{
			if (onORoff() == false) // moved into a zone gap
			{
				RunMode = AUTOMODE;
				// client.publish(inTopicGateCommand, "OFF");  // is this the best way?
				client.publish(oh3CmdStateWD, "SET");
			}
		}
	}

	else if (RunModeWE == NEXTMODE && weekDay == false)
	{
		// onORoff updates zone to the zone currently in
		if (SBWE == SBOFF && ZoneWE == ZONEGAP)
		{
			if (onORoff() == true) // moved into an on zone
				SBWE = SBON;	   // allow switch back to occur
		}
		else if (SBWE == SBON)
		{
			if (onORoff() == false) // moved into a zone gap
			{
				RunModeWE = AUTOMODE;
				// client.publish(inTopicGateCommandWE, "OFF");  // is this the best way?
				client.publish(oh3CmdStateWE, "SET");
			}
		}
	}

	else if (RunMode == ONMODE || RunModeWE == ONMODE)
	{
		memset(logString, 0, sizeof logString);
		sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "Permanently ON");
		mqttLog(logString, true);
	}
	else if (RunMode == OFFMODE || RunModeWE == OFFMODE)
	{
		memset(logString, 0, sizeof logString);
		sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "Permanently OFF");
		mqttLog(logString, true);
	}
	else
	{
		memset(logString, 0, sizeof logString);
		sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "Unknown running mode ");
		mqttLog(logString, true);
	}
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
		client.publish(oh3StateManual, "AUTO");
		Serial.println("processHtgCntrlMessage: SET received.");
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

// inStr: INPUT.  A string containing the times
// ptr: INPUT. points to the array of ON and OFF times as characters
// lptr: OUTPUT. The function populates the array with ON and OFF times as integers
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
// bool onORoff (int* zonex, bool weekday)
bool onORoff()
{
	// Serial.println("onORoff");
	// debugPrint();
	if (weekDay == true)
		Zone = ZONEGAP; // not in a zone
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
				Serial.print((int)timenow);
			}
			// Serial.print (", Zone: ");
			// Serial.print ((int) i - j);
			// Serial.print ("[");
			// Serial.print ( lcntrlTimes[i]);
			// Serial.print (",");
			// Serial.print ( lcntrlTimes[i+1]);
			// Serial.print ("] ");
			// memset(logString,0, sizeof logString);
			// sprintf(logString, "%s,%s,%s,%s[%i: %i: %i]", ntptod, sensorType, sensorName, "timenow: ", timenow, lcntrlTimes[i], lcntrlTimesWE[i] );
			// mqttLog(logString,true);
			if (weekDay == true)
			{
				if (timenow >= lcntrlTimes[i] && timenow < lcntrlTimes[i + 1])
				{
					state = true; // Switch ON
					Zone = i;	  // Update which zone we are in (there maybe overlapping time zones)
					// memset(logString,0, sizeof logString);
					// sprintf(logString, "%s,%s,%s,%s[%i]", ntptod, sensorType, sensorName, "WD onORoff zone: ", i );
					// mqttLog(logString,true);
					break;
				}
			}
			else
			{
				if (timenow >= lcntrlTimesWE[i] && timenow < lcntrlTimesWE[i + 1])
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

bool OLFControl(int pin, int state)
{
	if (state == LIGHTSOFF) // HIGH
	{
		client.publish(oh3StateRuntime, "OFF");
		digitalWrite(pin, LIGHTSOFF);
	}
	else if (state == LIGHTSON) // LOW
	{
		client.publish(oh3StateRuntime, "ON");
		digitalWrite(pin, LIGHTSON);
	}
	else if (state == LIGHTSAUTO) // LOW
	{
		client.publish(oh3StateRuntime, "AUTO");
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
	if (TODReceived == true &&
		TimesReceived == true &&
		TimesReceivedWE == true)
	{
		// mqttLog("readyCheck returns TRUE  - Everything received.", true);
		return true;
	}
	else
	{
		// mqttLog("readyCheck returns FALSE  - Still waiting.", true);
		return false;
	}
}

/*
 * Connect to MQTT server
 */

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect()
{
	Serial.println("Connecting to MQTT...");

	memset(mqttClientID, 0, sizeof mqttClientID);
	sprintf(mqttClientID, "%s:%s", sensorName, WiFi.macAddress().c_str());

	Telnet.print("Connecting to MQTT with Client ID: ");
	Telnet.println(mqttClientID);

	uint8_t retries = 3;

	while (!client.connected())
	{
		// Attempt to connect
		// if (client.connect(mqttClientID))
		if (client.connect(mqttClientID, willTopic, 1, true, lwm))
		{
			Telnet.println("Successfully connected to MQTT broker.");
			/*
			 * Publish booted status and Subscribe to topics
			 */
			client.publish(willTopic, "online", true);

			client.subscribe(oh3CommandTOD);
			client.subscribe(oh3CmdStateWD);
			client.subscribe(oh3CmdStateWE);
			client.subscribe(oh3CommandWDTimes);
			client.subscribe(oh3CommandWETimes);
			client.subscribe(oh3CommandIOT);
			client.subscribe(oh3CommandTrigger);
			client.loop();
		}
		else
		{
			/*-4 : MQTT_CONNECTION_TIMEOUT - the server didn't respond within the keepalive time
			  -3 : MQTT_CONNECTION_LOST - the network connection was broken
					-2 : MQTT_CONNECT_FAILED - the network connection failed
					-1 : MQTT_DISCONNECTED - the client is disconnected cleanly
					0 : MQTT_CONNECTED - the client is connected
					1 : MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT
					2 : MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier
					3 : MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection
					4 : MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected
					5 : MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to c
				   */
			Serial.print("MQTT connection failed. Return Code: ");
			if (client.state() == -2)
				Serial.println("MQTT_CONNECT_FAILED");
			else if (client.state() == -3)
				Serial.println("MQTT_CONNECTION_LOST");
			else if (client.state() == -4)
				Serial.println("MQTT_CONNECTION_TIMEOUT");
			else if (client.state() == -1)
				Serial.println("MQTT_DISCONNECTED");
			else
				Serial.println(client.state());
			Serial.println(" Retrying MQTT connection in 5 seconds...");

			delay(5000); // wait 5 seconds
			retries--;
			if (retries == 0)
			{
				Serial.println("ERROR: Can't connect to MQTT broker - Force a soft reset");
				// basically die and wait for WDT to reset me
				Telnet.println("ERROR: Can't connect to MQTT broker - Force a soft reset");

				while (1)
					;
			}
		}
	}

	memset(logString, 0, sizeof logString);
	sprintf(logString, "%s,%s,%s,%s", ntptod, sensorType, sensorName, "MQTT Connected.");
	mqttLog(logString, true);

	memset(logString, 0, sizeof logString);
	sprintf(logString, "%s,%s,%s,IP Address: %d.%d.%d.%d", ntptod, sensorType, sensorName, WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
	mqttLog(logString, true);
}

/*******************/
/* Connect to WiFi */
/*******************/
bool ScanAndConnect(void)
{

	strcpy(ipAddr, "1.1.1.1"); // Reset the address
	WiFi.mode(WIFI_STA);

	int n = WiFi.scanNetworks();
	if (n == 0)
		return false;
	else
	{
		int i = 0;
		int SigStrength[10][10]; // maximum of 10 access points.
		for (i = 0; i < n; ++i)
		{
			// initialise array
			// 2 columms column 0 is the signal Strength
			SigStrength[1][i] = i;
			SigStrength[0][i] = WiFi.RSSI(i);
		}
		for (int i = 1; i < n; ++i)
		{							   // insert sort into strongest signal
			int j = SigStrength[0][i]; // holding value for signal strength
			int l = SigStrength[1][i];
			int k;
			for (k = i - 1; (k >= 0) && (j > SigStrength[0][k]); k--)
			{
				SigStrength[0][k + 1] = SigStrength[0][k];
				l = SigStrength[1][k + 1];
				SigStrength[1][k + 1] = SigStrength[1][k];
				SigStrength[1][k] = l; // swap index values here.
			}
			SigStrength[0][k + 1] = j;
			SigStrength[1][k + 1] = l; // swap index values here to re-order.
		}
		int j = 0;
		while ((j < n) && (WiFi.status() != WL_CONNECTED))
		{
			Serial.print("Connecting: ");				  // debugging
			Serial.println(WiFi.SSID(SigStrength[1][j])); // debugging
			int k = 0;
			memset(ssid, 0, sizeof ssid);
			strcpy(ssid, WiFi.SSID(SigStrength[1][j]).c_str());
			WiFi.begin(ssid, pass); // conect to the j'th value in our new array
			while ((WiFi.status() != WL_CONNECTED) && k < 15)
			{
				Serial.print(".");
				delay(500);
				k++; // counter for up to 8 seconds - why 8 ? who knows
			}
			Serial.println("");
			if (k > 15)
				Serial.println("NEXT Wifi Access Point");
			j++; // choose the next SSID
		}
		if (WiFi.status() == WL_CONNECTED)
		{
			Serial.print("Connected:");
			Serial.println(WiFi.SSID());
			Serial.println(WiFi.localIP());

			strcpy(connectedSSID, WiFi.SSID().c_str());
			sprintf(ipAddr, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
			// strcpy(ipAddr, WiFi.localIP().toString().c_str());

			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%s,%d.%d.%d.%d", ntptod, sensorType, sensorName, connectedSSID, WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
			Telnet.println(logString);

			return true;
		}
		else
		{
			Serial.println("No Connection :( (Sad face)");
		}
	}
	return false;
}

//
// Publish MQTT broker and Log messages
// if bln is true log with CR
//
bool mqttLog(char *logString, bool bln)
{
	if (bln)
		Serial.println(logString);
	else
		Serial.print(logString);

	client.publish(oh3StateLog, logString);

	return false;
}

int get_weekday(char *str)
{
	struct tm tm;
	memset((void *)&tm, 0, sizeof(tm));
	if (strptime(str, "%Y-%m-%d", &tm) != NULL)
	{
		time_t t = mktime(&tm);
		if (t >= 0)
		{
			return localtime(&t)->tm_wday; // Sunday=0, Monday=1, etc.
		}
	}
	return -1;
}

void debugPrint()
{
	Serial.print("...Week Day Run Mode: ");
	Serial.print((int)RunMode);
	Serial.print(", Switch Back: ");
	Serial.print((int)SB);
	Serial.print(",  Zone: ");
	Serial.println((int)Zone);

	Serial.print("...Week End Run Mode: ");
	Serial.print((int)RunModeWE);
	Serial.print(", Switch Back: ");
	Serial.print((int)SBWE);
	Serial.print(",  Zone: ");
	Serial.println((int)ZoneWE);
}

String getFormattedTime()
{
	time_t rawtime = timeClient.getEpochTime();
	struct tm *ti;
	ti = localtime(&rawtime);

	uint16_t year = ti->tm_year + 1900;
	String yearStr = String(year);

	uint8_t month = ti->tm_mon + 1;
	String monthStr = month < 10 ? "0" + String(month) : String(month);

	uint8_t day = ti->tm_mday;
	String dayStr = day < 10 ? "0" + String(day) : String(day);

	uint8_t hours = ti->tm_hour;
	String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

	uint8_t minutes = ti->tm_min;
	String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

	uint8_t seconds = ti->tm_sec;
	String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

	return yearStr + "-" + monthStr + "-" + dayStr + "T" +
		   hoursStr + ":" + minuteStr + ":" + secondStr;
}

void handleTelnet()
{
	if (TelnetServer.hasClient())
	{
		// client is connected
		if (!Telnet || !Telnet.connected())
		{
			if (Telnet)
				Telnet.stop();				   // client disconnected
			Telnet = TelnetServer.available(); // ready for new client
		}
		else
		{
			TelnetServer.available().stop(); // have client, block new conections
		}
	}

	if (Telnet && Telnet.connected() && Telnet.available())
	{
		// client input processing
		while (Telnet.available())
		// Serial.write(Telnet.read()); // pass through
		// do other stuff with client input here
		// if (SERIAL.available() > 0) {
		{
			char c = Telnet.read();
			switch (c)
			{
			case '\r':
				Telnet.println();
				break;
			case 'l':

				int sigStrength;
				sigStrength = WiFi.RSSI();
				memset(logString, 0, sizeof logString);
				sprintf(logString,
						"%s%s\n\r%s%s\n\r%s%s\n\r%s%i\n\r%s%s\n\r%s%i\n\r%s%d.%d.%d.%d",
						"Date:\t\t", ntptod,
						"Sensor Type:\t", sensorType,
						"Sensor Name:\t", sensorName,
						"WiFi Connect:\t", WiFi.status(),
						"SSID:\t\t", connectedSSID,
						"Sig Strength:\t", sigStrength,
						"ipAddr:\t\t", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
				Telnet.println(logString);
				if (!client.connected())
					Telnet.println("MQTT Client:\tNOT CONNECTED");
				else
				{
					sprintf(logString, "%s%s\n\r", "MQTT Client:\t", mqttClientID);
					Telnet.println(logString);
				}

				break;
			case 'd':
				Telnet.println("Disconnecting from MQTT Broker ");
				client.disconnect();
				break;
			case 'r':
				Telnet.println("you are restarting this device");
				ESP.restart();
				break;
			default:
				Telnet.println(c);
				break;
			}
		}
	}
}
