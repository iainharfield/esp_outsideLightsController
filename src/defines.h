/****************************************************************************************************************************
  defines.h
  AsyncMqttClient_Generic is a library for ESP32, ESP8266, Protenta_H7, STM32F7, etc. with current AsyncTCP support

  Based on and modified from :

  1) async-mqtt-client (https://github.com/marvinroger/async-mqtt-client)

  Built by Khoi Hoang https://github.com/khoih-prog/AsyncMqttClient_Generic
 ***************************************************************************************************************************************/

#ifndef defines_h
#define defines_h

#include <Arduino.h>
#include <TZ.h>

#define _ASYNC_MQTT_LOGLEVEL_ 1

#define MAX_CFGSTR_LENGTH 51
#define MAX_LOGSTRING_LENGTH 501

#define TELNET_PORT 23

#define MY_TZ TZ_Europe_London
#define MY_NTP_SERVER "at.pool.ntp.org"

#define p_mqttBrokerIP_Label "mqttBrokerIP"
#define p_mqttBrokerPort_Label "mqttBrokerPort"

// MQTT Topic Names
#define oh3CommandIOT "/house/service/iot-command"             // e.g. IOT-IDENTITY, IOT-RESET, IOT-RESTART, IOT-SWITCH-CONFIG
#define oh3StateLog "/house/service/log"                       // Log messages
#define oh3CommandTOD "/house/service/time-of-day"             // Time of day broadcast from OpenHab
#define oh3StateIOTRefresh "/house/service/iot-device-refresh" // Request Refresh of Contol times ( not needed by all apps)

#define REPORT_INFO 0
#define REPORT_WARN 1
#define REPORT_DEBUG 2


//***************************************************************
// Controller Specific MQTT Topics and config
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

class devConfig
{
  String devName;
  String devType;
  bool configWifiOnboot;

public:
  devConfig() {}
  devConfig(String name, String type)
  {
    devName = name;
    devType = type;
    configWifiOnboot = false;
  }
  void setup(String name, String type)
  {
    devName = name;
    devType = type;
  }
  String getName()
  {
    return devName;
  }
  String getType()
  {
    return devType;
  }
  void setwifiConfigOnboot(bool bValue)
  {
    configWifiOnboot = bValue;
  }
  bool getwifiConfigOnboot(bool bValue)
  {
    return configWifiOnboot;
  }
};

class cntrlState
{
  int runMode;
  int switchBack;
  int zone;
  bool cntrlTimesReceived;
  String refreshID;
  String cntrlTimesTopic;
  String UICmdTopic;
  String CntrlRuntimeStateTopic;

public:
  cntrlState()
  {
    runMode = AUTOMODE;
    switchBack = SBUNKOWN;
    zone = ZONEGAP;
    cntrlTimesReceived = false; 
  }
  cntrlState(int rm, int sb, int zn)
  {
    runMode = rm;
    switchBack = sb;
    zone = zn;
    cntrlTimesReceived = false;
  }
  void setup(int rm, int sb, int zn)
  {
    runMode = rm;
    switchBack = sb;
    zone = zn;
    cntrlTimesReceived = false;
  }
  int getRunMode()
  {
    return runMode;
  }
  int getSwitchBack()
  {
    return switchBack;
  }
  int getZone()
  {
    return zone;
  }
  void setRunMode(int rm)
  {
    runMode = rm;
  }
  void setSwitchBack(int sb)
  {
    switchBack = sb;
  }
  void setZone(int zn)
  {
    zone = zn;
  }
  void setCntrlTimesTopic(String timesTopic)
  {
    cntrlTimesTopic = timesTopic;
  }
  void setUIcommandStateTopic(String UIcmdTopic)
  {
    UICmdTopic = UIcmdTopic;
  }
  void setCntrlRunTimesStateTopic(String runtimeState)
  {
    CntrlRuntimeStateTopic = runtimeState;
  }
  void setCntrlTimesReceived(bool ctr)
  {
    cntrlTimesReceived = ctr;
  }
  void setRefreshID(String rID)
  {
    refreshID = rID;
  }
  String getRefreshID()
  {
    return refreshID;
  }
  bool getCntrlTimesReceived()
  {
    return cntrlTimesReceived;
  }
  String getCntrlTimesTopic()
  {
    return cntrlTimesTopic;
  }
  String getCntrlRunTimesStateTopic()
  {
    return CntrlRuntimeStateTopic;
  }
  String getUIcommandStateTopic()
  {
    return UICmdTopic  ;
  }
};


class templateServices
{
  bool weekDay = false; // initialise FIXTHIS
  int dayNumber;
  int timeNow;
  char cntrlTimesWD[6][10]{"0000", "0100", "0200", "0300", "0400", "0600"}; //  how big is each array element? 6 elements each element 10 characters long (9 + 1 for /0)
  char cntrlTimesWE[6][10]{"0000", "0100", "0200", "0300", "0400", "0600"};


public:
  templateServices() {}
  templateServices(int dn)
  {
    dayNumber = dn;
    if (dn >= 1 && dn <= 5)
      weekDay = true;
    else
      weekDay = false;
  }
  void setup( int dn)
  {
    dayNumber = dn;
  }
  void setDayNumber(int dn)
  {
    dayNumber = dn;
    if (dn >= 1 && dn <= 5)
      weekDay = true;
    else
      weekDay = false;
  }
  bool getWeekDayState()
  {
    return weekDay;
  }
};

#endif // defines_h