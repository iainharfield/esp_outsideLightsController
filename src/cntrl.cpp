#include "defines.h"
#include <AsyncMqttClient_Generic.hpp>

void startCntrl();
bool onMqttMessageCntrlExt(char *, char *, const AsyncMqttClientMessageProperties &, const size_t &, const size_t &, const size_t &);
bool readyCheck();
bool onORoff();
bool processCntrlMessage(char *, const char *, const char *, const char *);
void processCntrlTOD_Ext();
void cntrlMQTTTopicSubscribe();

//
// Defined in utilities
//
extern bool mqttLog(const char*, bool, bool);

//*************************************
// defined in asyncConnect.cpp
//*************************************
extern void printTelnet(String);
extern char ntptod[MAX_CFGSTR_LENGTH];
extern AsyncMqttClient mqttClient;
extern void mqttTopicsubscribe(const char *topic, int qos);
extern templateServices coreServices;
//
// Application User exits
//
extern bool processCntrlMessageApp_Ext(char *, const char *, const char *, const char *);


extern int ohTimenow;
extern bool ohTODReceived;

// defined in telnet.cpp
extern int reporting;
extern bool telnetReporting;
//
// defined in app
extern void app_WD_on();
extern void app_WD_off();
extern void app_WE_on();
extern void app_WE_off();
extern void app_WD_auto();
extern void app_WE_auto();
//
extern devConfig espDevice;

//***************************************************************
// Application Specific MQTT Topics and config
//***************************************************************
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

//bool bManMode = false; // true = Manual, false = automatic

bool TimesReceivedWD = false;
bool TimesReceivedWE = false;
bool WDCommandReceived = false;
bool WECommandReceived = false;

int lcntrlTimesWD[6];													  // contains ON and OFF time for the 3 zone periods for upstairs heat request
int lcntrlTimesWE[6];													  // contains ON and OFF time for the 3 zone periods for downstairs heat request
char cntrlTimesWD[6][10]{"0000", "0100", "0200", "0300", "0400", "0600"}; //  how big is each array element? 6 elements each element 10 characters long (9 + 1 for /0)
char cntrlTimesWE[6][10]{"0000", "0100", "0200", "0300", "0400", "0600"};

// Contoller specific MQTT topics
const char *oh3CommandWDTimes = "/house/cntrl/outside-lights-front/wd-control-times"; // Times message from either UI or Python app
const char *oh3CommandWETimes = "/house/cntrl/outside-lights-front/we-control-times"; // Times message from either UI or MySQL via Python app
const char *oh3StateRuntime = "/house/cntrl/outside-lights-front/runtime-state";	  // ON, OFF, and AUTO
//const char *oh3StateManual = "/house/cntrl/outside-lights-front/manual-state";		  // Status of the Manual control switch control MAN or AUTO

const char *oh3CmdStateWD       = "/house/cntrl/outside-lights-front/wd-command";	  // UI Button press
const char *oh3CmdStateWE       = "/house/cntrl/outside-lights-front/we-command";     // UI Button press

cntrlState cntrlStateWD(AUTOMODE, SBUNKOWN, ZONEGAP);
cntrlState cntrlStateWE(AUTOMODE, SBUNKOWN, ZONEGAP);

void startCntrl()
{
	char logString[MAX_LOGSTRING_LENGTH];
	memset(logString, 0, sizeof logString);
	sprintf(logString, "%s,%s,%s,%s", ntptod, espDevice.getType().c_str(), espDevice.getName().c_str(), "Starting Controlle");
	mqttLog(logString, true, true);
	
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

//****************************************************************
// Process any application specific inbound MQTT messages
// Return False if none
// Return true if an MQTT message was handled here
//****************************************************************
bool onMqttMessageCntrlExt(char *topic, char *payload, const AsyncMqttClientMessageProperties &properties, const size_t &len, const size_t &index, const size_t &total)
{
	(void)payload;
	//char logString[MAX_LOGSTRING_LENGTH];

	char mqtt_payload[len + 1];
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
		mqttLog("WD Control times Received", true, true);
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
		mqttLog("WE Control times Received", true, true);
		processCrtlTimes(mqtt_payload, cntrlTimesWE, lcntrlTimesWE);
		TimesReceivedWE = true;
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
		WDCommandReceived = true;
		if (coreServices.getWeekDayState() == true)
		{
			if (processCntrlMessage(mqtt_payload, "ON", "OFF", oh3CmdStateWD) == true)
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
		WECommandReceived = true;
		if (coreServices.getWeekDayState() == false) // false == weekend
		{
			if (processCntrlMessage(mqtt_payload, "ON", "OFF", oh3CmdStateWE) == true)
			{
				Serial.print("ERROR: Unknown message - ");
				Serial.print(mqtt_payload);
				Serial.print(" - received for topic ");
				Serial.println(oh3CmdStateWE);
			}
		}
        return true;
	}
	return false;
}


/************************************************************
 * Process lighting control ON, OFF, SET and NEXT messages
 * returns: false = ok
 * 			true  = fail
 ************************************************************/
bool processCntrlMessage(char *mqttMessage, const char *onMessage, const char *offMessage, const char *commandTopic)
{
	if (strcmp(mqttMessage, "ON") == 0)
	{
		
		if (coreServices.getWeekDayState() == true)
		{
			cntrlStateWD.setRunMode(ONMODE);
			mqttClient.publish(oh3StateRuntime, 1, true, "ON");		// FIXTHIS WD or WE
			app_WD_on();											 
		}
		else
		{
			cntrlStateWE.setRunMode(ONMODE);
			mqttClient.publish(oh3StateRuntime, 1, true, "ON");		// FIXTHIS WD or WE
			app_WE_on();											 
		}
	}
	else if (strcmp(mqttMessage, "OFF") == 0)
	{
		//*rm = OFFMODE;
		if (coreServices.getWeekDayState() == true)
		{
			cntrlStateWD.setRunMode(ONMODE);
			mqttClient.publish(oh3StateRuntime, 1, true, "OFF");		// FIXTHIS WD or WE
			app_WD_off();											 
		}
		else
		{
			cntrlStateWE.setRunMode(ONMODE);
			mqttClient.publish(oh3StateRuntime, 1, true, "OFF");		// FIXTHIS WD or WE
			app_WE_off();											 
		}
	}
	else if (strcmp(mqttMessage, "ONOFF") == 0)
	{
		mqttClient.publish(oh3StateRuntime,1, true, "ON");		// FIXTHIS WD or WE
		app_WD_on();											// FIXTHIS WD or WE 
		delay(5000);											// FIX this
		mqttClient.publish(oh3StateRuntime,1, true, "OFF");		// FIXTHIS WD or WE
		app_WD_off(); 											// FIXTHIS WD or WE

		cntrlStateWE.setRunMode(AUTOMODE);			// FIX THIS : why am I doing this? leave as is
	}
	else if (strcmp(mqttMessage, "NEXT") == 0)
	{
		if (coreServices.getWeekDayState() == true)
		{			
			cntrlStateWD.setRunMode(NEXTMODE);
			cntrlStateWD.setSwitchBack(SBOFF);						// Switch back to AUTOMODE when Time of Day is next OFF (don't switch back when this zone ends)	
			mqttClient.publish(oh3StateRuntime, 1, true, "ON");		// FIXTHIS WD or WE
			app_WD_on();											// Set on because we want ON until the end of the next OFF	
		}
		else
		{
			cntrlStateWE.setRunMode(NEXTMODE);
			cntrlStateWE.setSwitchBack(SBOFF);						// Switch back to AUTOMODE when Time of Day is next OFF (don't switch back when this zone ends)	
			mqttClient.publish(oh3StateRuntime, 1, true, "ON");		// FIXTHIS WD or WE
			app_WE_on();											// Set on because we want ON until the end of the next OFF	

		}

	}
	else if (strcmp(mqttMessage, "SET") == 0)
	{
		
		mqttLog("processOLFCntrlMessage: SET received.", true, true);

		// IF pressed SET then check the ON Close time and sent the appropriate message
		if (coreServices.getWeekDayState() == true)
		{
			cntrlStateWD.setRunMode(AUTOMODE);
			if (onORoff() == true)
			{
				mqttClient.publish(oh3StateRuntime,1, true, "ON");		// FIXTHIS WD or WE
				app_WD_on();											 
			}
			else
			{
				mqttClient.publish(oh3StateRuntime,1, true, "OFF");		// FIXTHIS WD or WE
				app_WD_off(); 											
			}
		}
		else
		{
			cntrlStateWE.setRunMode(AUTOMODE);
			if (onORoff() == true)
			{
				mqttClient.publish(oh3StateRuntime,1, true, "ON");		// FIXTHIS WD or WE
				app_WE_on();											 
			}
			else
			{
				mqttClient.publish(oh3StateRuntime,1, true, "OFF");		// FIXTHIS WD or WE
				app_WE_off(); 											
			}
		}	
	}
	else
	{
		processCntrlMessageApp_Ext(mqttMessage, onMessage, offMessage, commandTopic);
		return true;
	}
	processCntrlMessageApp_Ext(mqttMessage, onMessage, offMessage, commandTopic);
	return false;
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

	//char logString[MAX_LOGSTRING_LENGTH]; 
	// debugPrint();
	mqttLog("processOLFCntrlMessage: onORoff checking", true, true);

	if (coreServices.getWeekDayState() == true)
		cntrlStateWD.setZone(ZONEGAP); // not in a zone
	else
		cntrlStateWE.setZone(ZONEGAP);

	bool state = false;

	if (readyCheck() == true) // All conditions to start are met
	{
		/****************************************************************
		 * Work out from the array of longs if the control is On or OFF
		 ****************************************************************/
		Serial.print("PS ZONE CONFIG: ");

		for (int i = 0, j = 0; i < 6; i++, j++)
		{
			// Serial.print (", Zone: ");
			// Serial.print ((int) i - j);
			// Serial.print ("[");
			// Serial.print ( lcntrlTimes[i]);
			// Serial.print (",");
			// Serial.print ( lcntrlTimes[i+1]);
			// Serial.print ("] ");
			
		
			//memset(logString,0, sizeof logString);
			//sprintf(logString, "%s,%s,%s,%s[%i: %i: %i]", ntptod, espDevice.getType().c_str(), espDevice.getName().c_str(), "ohTimenow: ", ohTimenow, lcntrlTimesWD[i], lcntrlTimesWE[i]);
			//mqttLog(logString,true, true);

			if (coreServices.getWeekDayState() == true)
			{
				if (ohTimenow >= lcntrlTimesWD[i] && ohTimenow < lcntrlTimesWD[i + 1])
				{
					state = true; // Switch ON
					cntrlStateWD.setZone(i);	  // Update which zone we are in (there maybe overlapping time zones)
					break;
				}
			}
			else
			{
				if (ohTimenow >= lcntrlTimesWE[i] && ohTimenow < lcntrlTimesWE[i + 1])
				{
					state = true; // Switch ON
					cntrlStateWE.setZone(i);	  // Update which zone we are in (there maybe overlapping time zones)
					break;
				}
			}
			i++;
		}

		if (state == true)
		{
			//FIXTHIS
			//mqttLog("onORoff returns  - ON", true, true);
			//Serial.println(" and set to ON");
		}
		else
		{
			// FIXTHIS

			//mqttLog("onORoff returns  - OFF", true, true);
			//Serial.println(" In between zones so set to OFF unless NEXT override");
			// mqttLog("In between zones so set to OFF unless NEXT. If in NEXT then don't switch OFF - wait for next zone to switch OFF", true);
		}
	}
	return state;
}

/*
 * Check that the initialisation data all been received before processing
 * Returns True if all checks have been received
 */
bool readyCheck()
{
	if (ohTODReceived == true && TimesReceivedWD == true && TimesReceivedWE == true)
	{
		//mqttLog("readyCheck = true", true, true);
		return true;
	}
	else
	{
		//mqttLog("readyCheck = false...", true, true);

		//if (ohTODReceived == false)
		//	mqttLog("TOD not received", true, true);
		//if (TimesReceivedWD == false)
		//	mqttLog("WD Contol times not received", true, true);
		//if (TimesReceivedWE == false)
		//	mqttLog("WE Contol times not received", true, true);

		return false;
	}
}

/*
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
*/
// Subscribe to controler specific topics
void cntrlMQTTTopicSubscribe()
{	
	mqttTopicsubscribe(oh3CommandWDTimes, 2);
	mqttTopicsubscribe(oh3CommandWETimes, 2);

	mqttTopicsubscribe(oh3CmdStateWD, 2);
	mqttTopicsubscribe(oh3CmdStateWE, 2);
}


//************************************
// Application specific behaviour
//************************************
// here

//************************************************************************
// Write out over telnet session and application specific infomation
// FIXTHIS: Move to utilities
//***********************************************************************
void telnet_extension_1(char c)
{
	// char logString[MAX_LOGSTRING_LENGTH];
	// memset(logString, 0, sizeof logString);
	// sprintf(logString, "%s%d\n\r", "Sensor Value:\t", sensorValue);
	// printTelnet((String)logString);

	String wd;
	String rmWD, rmWE;
	char stringOfTimesWD[70];
	char stringOfTimesWE[70];
	// get weekday
	wd = "Week Day";
	if (coreServices.getWeekDayState() == false)
		wd = "Weekend";

	// Get on, off times
	sprintf(stringOfTimesWD, "%s,%s,%s,%s,%s,%s", cntrlTimesWD[0], cntrlTimesWD[1], cntrlTimesWD[2], cntrlTimesWD[3], cntrlTimesWD[4], cntrlTimesWD[5]);
	sprintf(stringOfTimesWE, "%s,%s,%s,%s,%s,%s", cntrlTimesWE[0], cntrlTimesWE[1], cntrlTimesWE[2], cntrlTimesWE[3], cntrlTimesWE[4], cntrlTimesWE[5]);

	// get Run Mode
	if (cntrlStateWD.getRunMode() == AUTOMODE)
		rmWD = "AUTOMODE";
	else if (cntrlStateWD.getRunMode() == NEXTMODE)
		rmWD = "NEXTMODE";
	else if (cntrlStateWD.getRunMode() == ONMODE)
		rmWD = "ONMODE";
	else if (cntrlStateWD.getRunMode() == OFFMODE)
		rmWD = "OFFMODE";
	else
		rmWD = "UNKNOWN";

	if (cntrlStateWE.getRunMode() == AUTOMODE)
		rmWE = "AUTOMODE";
	else if (cntrlStateWE.getRunMode() == NEXTMODE)
		rmWE = "NEXTMODDE";
	else if (cntrlStateWE.getRunMode() == ONMODE)
		rmWE = "ONMODE";
	else if (cntrlStateWE.getRunMode() == OFFMODE)
		rmWE = "ONMODE";
	else
		rmWE = "UNKNOWN";

	// Print out on Telnet terminal
	char logString[MAX_LOGSTRING_LENGTH];
	// memset(logString, 0, sizeof logString);
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

// Triggerd by xxx ticker
bool timeReceivedChecker()
{
	char logString[MAX_LOGSTRING_LENGTH];

	if (TimesReceivedWD == false || TimesReceivedWE == false || WDCommandReceived == false || WECommandReceived == false || ohTODReceived == false)
	{
		memset(logString, 0, sizeof logString);
		sprintf(logString, "%s,%s,%s,%s", ntptod, espDevice.getType().c_str(), espDevice.getName().c_str(), "Waiting for all initialisation messages to be received");
		mqttLog(logString, true, true);

		if (ohTODReceived == false)
		{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%s", ntptod, espDevice.getType().c_str(), espDevice.getName().c_str(), "Time of Day not yet received");
			mqttLog(logString, true, true);
		}
		if (TimesReceivedWD == false)
		{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%s", ntptod, espDevice.getType().c_str(), espDevice.getName().c_str(), "Weekday control times not yet received");
			mqttLog(logString, true, true);
		}
		if (TimesReceivedWE == false)
		{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%s", ntptod, espDevice.getType().c_str(), espDevice.getName().c_str(), "Weekend control times not yet received");
			mqttLog(logString, true, true);
		}
		if (WDCommandReceived == false && coreServices.getWeekDayState() == true)
		{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%s", ntptod, espDevice.getType().c_str(), espDevice.getName().c_str(), "Weekday operating mode not yet received");
			mqttLog(logString, true, true);
		}
		if (WECommandReceived == false && coreServices.getWeekDayState() == false)
		{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%s", ntptod, espDevice.getType().c_str(), espDevice.getName().c_str(), "Weekend operating mode not yet received");
			mqttLog(logString, true, true);
		}
		mqttClient.publish(oh3StateIOTRefresh, 1, true, "OLF");
		return false;
	}
	else
		return true;
}

//**********************************************************************
// Controller TOD Porocessing 
// check current time against configutation and decide what to do.
// Send appropriate MQTT command for each control.
// Run this everytime the TOD changes
// If TOD event is not received then nothing happens
//**********************************************************************
void processCntrlTOD_Ext()
{

	// Check all WD and WE times have been received before doing anything
	mqttLog("OLF Contoller Processing TOD", true, true);

	char logString[MAX_LOGSTRING_LENGTH];

	if (timeReceivedChecker() == true)
	{
		if (cntrlStateWD.getRunMode() == AUTOMODE && coreServices.getWeekDayState() == true)
		{
			if (onORoff() == true)
			{
				// trigger Application let it know that the Contoller in an on period.
				// Let the app decide what to do.
				app_WD_on();
				//OLFControl(relay_pin, LIGHTSON);
			}
			else
			{
				app_WD_off();
				//OLFControl(relay_pin, LIGHTSOFF);
			}
		}
		else if (cntrlStateWE.getRunMode() == AUTOMODE && coreServices.getWeekDayState() == false)
		{
			if (onORoff() == true)
			{
				app_WE_on();
				//OLFControl(relay_pin, LIGHTSON);
			}
			else
			{
				app_WE_off();
				//OLFControl(relay_pin, LIGHTSOFF);
			}
		}
		else if (cntrlStateWD.getRunMode() == NEXTMODE && coreServices.getWeekDayState() == true)
		{
			// onORoff updates zone to the zone currently in
			if (cntrlStateWD.getSwitchBack() == SBOFF && cntrlStateWD.getZone() == ZONEGAP)
			{
				if (onORoff() == true) // moved into an on zone
					cntrlStateWD.setSwitchBack(SBON);	   // allow switch back to occur at end of zone period
			}
			else if (cntrlStateWD.getSwitchBack() == SBON)
			{
				if (onORoff() == false) // moved into a zone gap
				{
					cntrlStateWD.setRunMode(AUTOMODE);
					mqttClient.publish(oh3StateRuntime, 1, true, "AUTO");
					mqttClient.publish(oh3CmdStateWD, 1, true, "SET");	
					app_WD_auto();
					
				}
			}
		}

		else if (cntrlStateWE.getRunMode() == NEXTMODE && coreServices.getWeekDayState() == false)
		{
			// onORoff updates zone to the zone currently in
			if (cntrlStateWE.getSwitchBack() == SBOFF && cntrlStateWE.getZone() == ZONEGAP)
			{
				if (onORoff() == true) // moved into an on zone
					cntrlStateWE.setSwitchBack(SBON);	   // allow switch back to occur
			}
			else if (cntrlStateWE.getSwitchBack() == SBON)
			{
				if (onORoff() == false) // moved into a zone gap
				{
					cntrlStateWE.setRunMode(AUTOMODE);
					mqttClient.publish(oh3StateRuntime, 1, true, "AUTO");
					mqttClient.publish(oh3CmdStateWE, 1, true, "SET");	
					app_WE_auto();
				}
			}
		}
		else if (cntrlStateWD.getRunMode() == ONMODE || cntrlStateWE.getRunMode() == ONMODE)
		{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%s", ntptod, espDevice.getType().c_str(), espDevice.getName().c_str(), "Permanently ON");
			mqttLog(logString, true, true);
		}
		else if (cntrlStateWD.getRunMode() == OFFMODE || cntrlStateWE.getRunMode() == OFFMODE)
		{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%s", ntptod, espDevice.getType().c_str(), espDevice.getName().c_str(), "Permanently OFF");
			mqttLog(logString, true, true);
		}
		else
		{
			memset(logString, 0, sizeof logString);
			sprintf(logString, "%s,%s,%s,%s", ntptod, espDevice.getType().c_str(), espDevice.getName().c_str(), "Unknown running mode ");
			mqttLog(logString, true, true);
		}
	}
}