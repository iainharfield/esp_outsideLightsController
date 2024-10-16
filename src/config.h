#include <WString.h>

#define WDCntlTimes     "/house/cntrl/outside-lights-front/wd-control-times" // Times received from either UI or Python app
#define WECntlTimes     "/house/cntrl/outside-lights-front/we-control-times" // Times received from either UI or MySQL via Python app
#define runtimeState    "/house/cntrl/outside-lights-front/runtime-state"	 // published state: ON, OFF, and AUTO
#define WDUICmdState    "/house/cntrl/outside-lights-front/wd-command"		 // UI Button press received
#define WEUICmdState    "/house/cntrl/outside-lights-front/we-command"		 // UI Button press received
#define LightState      "/house/cntrl/outside-lights-front/light-state"       // ON or OFF
#define RefreshID       "OLF"	

#define StartUpMessage  "/nStarting Outside Lights Front Controller on "

String deviceName 	= "outside-lights-front";
String deviceType 	= "CNTRL";
String app_id 		= "OLF"; 	// configure

const char *oh3CommandTrigger 	= "/house/cntrl/outside-lights-front/pir-command"; // Event fron the PIR detector (front porch: PIRON or PIROFF
const char *oh3StateManual 		= "/house/cntrl/outside-lights-front/manual-state";	 // 	Status of the Manual control switch control MAN or AUTO