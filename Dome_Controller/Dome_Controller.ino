///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///*****                                                                                                       *****///
///*****                            Created by Greg Hulette.                                                   *****///
///*****                                                                                                       *****///
///*****   I started with the code from flthymcnsty from from which I used the basic command structure and     *****///
///*****  serial input method.  This code also relies on the ReelTwo library for all it's servo movements.     *****///
///*****                                                                                                       *****///
///*****                      So exactly what does this all do.....?                                           *****///
///*****                       - Receives commands via ESP-NOW                                        *****///
///*****                       - Controls the Dome servos                                                      *****///
///*****                       - Controls the Camera Light                                                     *****///
///*****                       - Sends Serial commands to the HPs and RSeries Logic Displays                   *****///
///*****                                                                                                       *****///
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////
///*****        Libraries used in this sketch                 *****///
//////////////////////////////////////////////////////////////////////

// Standard Arduino library
#include <Arduino.h>

// Used for OTA
#include "ESPAsyncWebServer.h"
#include <AsyncElegantOTA.h>
#include <elegantWebpage.h>
#include <AsyncTCP.h>
#include <WiFi.h>

//Used for ESP-NOW
#include "esp_wifi.h"
#include <esp_now.h>

//Used for Camera  and status LEDs
#include <Adafruit_NeoPixel.h>

//Used for pin definition
#include "dome_controller_pin_map.h"

// Debug Functions  - Using my own library for this
#include <DebugR2.h>

//ReelTwo libaries - Using my forked version of this libarary
#include <ReelTwo.h>
#include "core/DelayCall.h"
#include "ServoDispatchPCA9685.h"
#include "ServoSequencer.h"

//Used for PC9685 - Servo Expansion Board
#include <Wire.h>

// Used to expand Serial Capacity
#include <SoftwareSerial.h>


//////////////////////////////////////////////////////////////////////
///*****       Preferences/Items to change        *****///
//////////////////////////////////////////////////////////////////////
 //ESPNOW Password - This must be the same across all devices
  String ESPNOWPASSWORD = "GregsAstromech";

  ////R2 Control Network Details for OTA only
  const char* ssid = "R2D2_Control_Network";
  const char* password =  "astromech";

  // Keepalive timer to send status messages to the Kill Switch (Droid)
  int keepAliveDuration= 4000;  // 4 seconds
  
  // used to sync timing with the dome controller better, allowing time for the ESP-NOW messages to travel to the dome
// Change this to work with how your droid performs
  int defaultESPNOWSendDuration = 50;  

// Serial Baud Rates
  #define HP_BAUD_RATE 9600
  #define RS_BAUD_RATE 9600
  #define PS_BAUD_RATE 9600 //Should be lower than 57600
  #define FU_BAUD_RATE 9600  //Should be lower than 57600




//////////////////////////////////////////////////////////////////////
///*****        Command Varaiables, Containers & Flags        *****///
//////////////////////////////////////////////////////////////////////
  String HOSTNAME = "Dome Controller";
  
  char inputBuffer[100];
  String inputString;         // a string to hold incoming data
  
  volatile boolean stringComplete  = false;      // whether the serial string is complete
  String autoInputString;         // a string to hold incoming data
  volatile boolean autoComplete    = false;    // whether an Auto command is setA
  
  int commandLength;

  String serialStringCommand;
  String serialPort;
  String serialSubStringCommand;

 uint32_t Local_Command[6]  = {0,0,0,0,0,0};
  int localCommandFunction     = 0;

  String ESPNOWStringCommand;
  String ESPNOWTarget;
  String ESPNOWSubStringCommand;
  
    byte RE_command[6] = {0,0,0,0,0,0};
 
  
  int colorState1;
  int speedState;
  int ledFunction;

  debugClass Debug;
  String debugInputIdentifier ="";

//////////////////////////////////////////////////////////////////////
///*****   Door Values, Containers, Flags & Timers   *****///
//////////////////////////////////////////////////////////////////////
  int door = -1;
  // Door Command Container
  uint32_t D_command[7]  = {0,0,0,0,0,0,0};
  int doorFunction = 0;
  int doorBoard = 0; 
  int doorEasingMethod;
  uint32_t cVarSpeedMin;
  uint32_t cVarSpeedMax;
  uint32_t doorEasingDuration;
  uint32_t delayCallTime;

  // variables for individual functions
  uint32_t varSpeedMin;
  uint32_t varSpeedMax;
  char stringToSend[20];
  uint32_t fVarSpeedMin;
  uint32_t fVarSpeedMax;

/////////////////////////////////////////////////////////////////////////
///*****              ReelTwo Servo Set Up                       *****///
/////////////////////////////////////////////////////////////////////////

#define SMALL_PANEL_ONE       0x0001 //b0000 0000 0000 0001
#define SMALL_PANEL_TWO       0x0002 //b0000 0000 0000 0010
#define SMALL_PANEL_THREE     0x0004 //b0000 0000 0000 0100
#define MEDIUM_PANEL_PAINTED  0x0008 //b0000 0000 0000 1000
#define MEDIUM_PANEL_SILVER   0x0010 //b0000 0000 0001 0000
#define BIG_PANEL             0x0020 //b0000 0000 0010 0000
#define PIE_PANEL_ONE         0x0040 //b0000 0000 0100 0000
#define PIE_PANEL_TWO         0x0080 //b0000 0000 1000 0000
#define PIE_PANEL_THREE       0x0100 //b0000 0001 0000 0000
#define PIE_PANEL_FOUR        0x0200 //b0000 0010 0000 0000

#define SMALL_PANELS_MASK     (SMALL_PANEL_ONE|SMALL_PANEL_TWO|SMALL_PANEL_THREE)
#define MEDIUM_PANELS_MASK    (MEDIUM_PANEL_PAINTED|MEDIUM_PANEL_SILVER)
#define DOME_PANELS_MASK      (SMALL_PANELS_MASK|MEDIUM_PANELS_MASK|BIG_PANEL)
#define PIE_PANELS_MASK       (PIE_PANEL_ONE|PIE_PANEL_TWO|PIE_PANEL_THREE|PIE_PANEL_FOUR)
#define ALL_SERVOS_MASK       (DOME_PANELS_MASK|PIE_PANELS_MASK)

// Group ID is used by the ServoSequencer and some ServoDispatch functions to
// identify a group of servos.

//     Pin  Min, ,Max,  Group ID  (Change the Min and Max to your Droids actual limits)
const ServoSettings servoSettings[] PROGMEM = {
    { 1,   2363, 1592, SMALL_PANEL_ONE },       /* 0: door 1 small left door by radar eye */
    { 2,   1975, 1200, SMALL_PANEL_TWO },       /* 1: door 2 small middle door by radar eye */
    { 3,   2000, 1200, SMALL_PANEL_THREE },     /* 2: door 3 small right door by radar eye */
    { 4,   2000, 1200, MEDIUM_PANEL_PAINTED },  /* 3: door 4 medium painted door */
    { 5,   2050, 1100, MEDIUM_PANEL_SILVER },   /* 4: door 5 Medium Unpainted door*/
    { 6,   1950, 1100, BIG_PANEL },             /* 5: door 6 Big Lower door */
    { 7,   2050, 1156, PIE_PANEL_ONE },         /* 6: door 7 Pie Panel near Periscope */
    { 8,   2050, 1175, PIE_PANEL_TWO },         /* 7: door 8 Pie Panel clockwise from Periscope*/
    { 9,   2050, 1168, PIE_PANEL_THREE },       /* 8: door 9 Pie Panel clockwise-2 from Periscope */
    { 10,  2050, 1257, PIE_PANEL_FOUR }        /* 9: door 10 Pie Panel clockwise-3 from Periscope */
};

ServoDispatchPCA9685<SizeOfArray(servoSettings)> servoDispatch(servoSettings);
ServoSequencer servoSequencer(servoDispatch);

//////////////////////////////////////////////////////////////////////
///*****       Startup and Loop Variables                     *****///
//////////////////////////////////////////////////////////////////////
  
  boolean startUp = true;
  boolean isStartUp = true;
  
  unsigned long mainLoopTime; // We keep track of the "Main Loop time" in this variable.
  unsigned long MLMillis;
  byte mainLoopDelayVar = 5;

 //Timer for Status updates
    int keepAliveMillis;


  //////////////////////////////////////////////////////////////////////
  ///******       Serial Ports Specific Setup                   *****///
  //////////////////////////////////////////////////////////////////////
  
  // #define RXHP 15
  // #define TXHP 16 
  // #define RXRS 25
  // #define TXRS 26
  // #define RXFU 12
  // #define TXFU 14 
  
  #define hpSerial Serial1
  #define rsSerial Serial2
  SoftwareSerial fuSerial;
  SoftwareSerial psSerial;



//////////////////////////////////////////////////////////////////////
///*****            Status and Camera Lens Variables and settings       *****///
//////////////////////////////////////////////////////////////////////
  
  unsigned long RE_loopTime; // We keep track of the "time" in this variable.
  unsigned long RadarEyeMillis;
  byte RadarEyespeed = 50;
  unsigned long RE_startMillis;
  unsigned long RE_currentMillis;  
  int dim = 75;
  boolean countUp=false;
// -------------------------------------------------
// Define some constants to help reference objects,
// pins, leds, colors etc by name instead of numbers
// -------------------------------------------------
//    CAMERA LENS LED VARIABLES
  const uint32_t red     = 0xFF0000;
  const uint32_t orange  = 0xFF8000;
  const uint32_t yellow  = 0xFFFF00;
  const uint32_t green   = 0x00FF00;
  const uint32_t cyan    = 0x00FFFF;
  const uint32_t blue    = 0x0000FF;
  const uint32_t magenta = 0xFF00FF;
  const uint32_t white   = 0xFFFFFF;
  const uint32_t off     = 0x000000;

  const uint32_t basicColors[9] = {off, red, yellow, green, cyan, blue, magenta, orange, white};

  #define NUM_RADAR_EYE_PIXELS 7
  #define STATUS_LED_COUNT 1  
  
  Adafruit_NeoPixel RADAR_EYE_LEDS = Adafruit_NeoPixel(NUM_RADAR_EYE_PIXELS, RADAR_EYE_LED_PIN, NEO_GRB + NEO_KHZ800);
  Adafruit_NeoPixel ESP_LED = Adafruit_NeoPixel(STATUS_LED_COUNT, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);


///////////////////////////////////////////////////////////////////////
///*****                 Auto Sequence Settings                *****///
///*****                                                       *****///
///*****  When enabled, each display will automatically        *****///
///*****  trigger random display sequences. This setting can   *****///
///*****  be overridden by sending the following display       *****///
///*****  Display Commands to the appropriate display device.  *****///
///*****                                                       *****///
///*****     98 - Auto Off Trigger Command                     *****///
///*****     99 - Auto On Trigger  Command                     *****///
///*****                                                       *****///
///*****     1 = enabled   0 = disable                         *****///
///*****                                                       *****///
///////////////////////////////////////////////////////////////////////
   byte enableRadarEyeAuto   = 1;   //1
  
//////////////////////////////////////////////////////////////////////
///*****             Auto Mode Interval Settings               *****///
///*****                                                       *****///
///*****  Each display enabled by auto mode neeed 4 values.    *****///
///*****  The first two values of each category below are      *****///
///*****  the min and max range that determine the run time    *****///
///*****  of each display sequence.  Run times will be chosen  *****///
///*****  from with in this range.                             *****///
///*****                                                       *****///
///*****  Likewise, the second two values of each category     *****///
///*****  determine the range from which the interval between  *****///
///*****  display sequences will be randomly chosen.           *****///
///*****                                                       *****///
///////////////////////////////////////////////////////////////////////

  unsigned const int RadarEyeAutoIntMin    = 300000;
  unsigned const int RadarEyeAutoIntMax    = 300000;
  unsigned const int RadarEyeAutoPauseMin  = 1;
  unsigned const int RadarEyeAutoPauseMax  = 1;
  unsigned int RadarEyeAutoPause;
  unsigned int RadarEyeAutoInt;

///////////////////////////////////////////////////////////////////////
///*****               Auto Sequence Assignments               *****///
///*****                                                       *****///
///*****   You can choose which sequences will be available    *****///
///*****  to each auto function. Simply add the command value  *****///
///*****  for each desired sequence to the string array below. *****///
///*****                                                       *****///
///////////////////////////////////////////////////////////////////////
  String RadarEyeAutoCommands[] = {
                            "R0155",        // Pulse Blue at medium speed
                            "R0155"        // Pulse Blue at medium speed
                            };

  int RadarEyeAutoCommandsCount = sizeof(RadarEyeAutoCommands) / sizeof(RadarEyeAutoCommands[ 0 ]);     // Determins the # of Commands in List

  unsigned long RadarEyeAutoTimer;


/////////////////////////////////////////////////////////////////////////
///*****                  ESP NOW Set Up                         *****///
/////////////////////////////////////////////////////////////////////////

//  ESP-NOW MAC Addresses used in the Droid. 
  const uint8_t droidLoRaMACAddress[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
  const uint8_t bodyControllerMACAddress[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};
  const uint8_t bodyServosControllerMACAddress[] =  {0x02, 0x00, 0x00, 0x00, 0x00, 0x03};
  const uint8_t domeControllerMACAddress[]=  {0x02, 0x00, 0x00, 0x00, 0x00, 0x04};
  const uint8_t domePlateControllerMACAddress[] =   {0x02, 0x00, 0x00, 0x00, 0x00, 0x05};
  const uint8_t broadcastMACAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


// Uses these Strings for comparators
  String droidLoRaMACAddressString = "02:00:00:00:00:01";
  String bodyControllerMACAddressString = "02:00:00:00:00:02";
  String bodyServosControllerMACAddressString = "02:00:00:00:00:03";
  String domeControllerMACAddressString = "02:00:00:00:00:04";
  String domePlateControllerMACAddressString = "02:00:00:00:00:05";
  String broadcastMACAddressString = "FF:FF:FF:FF:FF:FF";

// Define variables to store commands to be sent
  String  senderID;
  String  targetID;
  bool    commandIncluded;
  String  command;

// Define variables to store incoming commands
  String  incomingTargetID;  
  String  incomingSenderID;
  String  incomingCommand;
  bool    incomingCommandIncluded;
  String  incomingPassword;
  
// Variable to store if sending data was successful
  String success;

//Structure example to send data
//Must match the receiver structure
typedef struct espnow_struct_message {
      char structPassword[20];
      char structSenderID[4];
      char structTargetID[4];
      bool structCommandIncluded;
      char structCommand[100];
  } espnow_struct_message;

// Create a struct_message calledcommandsTosend to hold variables that will be sent
  espnow_struct_message commandsToSendtoBroadcast;
  espnow_struct_message commandsToSendtoDroidLoRa;
  espnow_struct_message commandsToSendtoBodyController;
  espnow_struct_message commandsToSendtoBodyServoController;
  espnow_struct_message commandsToSendtoDomeController;
  espnow_struct_message commandsToSendtoDomePlateController;

// Create a struct_message to hold incoming commands from the Body
  espnow_struct_message commandsToReceiveFromBroadcast;
  espnow_struct_message commandsToReceiveFromDroidLoRa;
  espnow_struct_message commandsToReceiveFromBodyController;
  espnow_struct_message commandsToReceiveFromBodyServoController;
  espnow_struct_message commandsToReceiveFromDomeController;
  espnow_struct_message commandsToReceiveFromDomePlateController;
  
  esp_now_peer_info_t peerInfo;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (Debug.debugflag_espnow == 1){
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    if (status ==0){
      success = "Delivery Success :)";
    }
    else{
      success = "Delivery Fail :(";
    }
  }
}

//   Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    colorWipeStatus("ES", orange ,255);

  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  String IncomingMacAddress(macStr);
  if (IncomingMacAddress = droidLoRaMACAddressString) {
      memcpy(&commandsToReceiveFromDroidLoRa, incomingData, sizeof(commandsToReceiveFromDroidLoRa));
      incomingPassword = commandsToReceiveFromDroidLoRa.structPassword;
      if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password was sent.  Message Ignored\n");  
      } else {
        incomingSenderID = commandsToReceiveFromDroidLoRa.structSenderID;
        incomingTargetID = commandsToReceiveFromDroidLoRa.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromDroidLoRa.structCommandIncluded;
        incomingCommand = commandsToReceiveFromDroidLoRa.structCommand;
        processESPNOWIncomingMessage();
        }
    } else if (IncomingMacAddress == bodyControllerMACAddressString){
    memcpy(&commandsToReceiveFromBodyController, incomingData, sizeof(commandsToReceiveFromBodyController));
    incomingPassword = commandsToReceiveFromBodyController.structPassword;
   if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password was sent.  Message Ignored\n");  
      } else {
        incomingSenderID = commandsToReceiveFromBodyController.structSenderID;
        incomingTargetID = commandsToReceiveFromBodyController.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromBodyController.structCommandIncluded;
        incomingCommand = commandsToReceiveFromBodyController.structCommand;
        processESPNOWIncomingMessage();
        }
    }else if (IncomingMacAddress = bodyServosControllerMACAddressString) {
      memcpy(&commandsToReceiveFromBodyServoController, incomingData, sizeof(commandsToReceiveFromBodyServoController));
      incomingPassword = commandsToReceiveFromBodyServoController.structPassword;
      if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password was sent.  Message Ignored\n");  
      } else {
        incomingSenderID = commandsToReceiveFromBodyServoController.structSenderID;
        incomingTargetID = commandsToReceiveFromBodyServoController.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromBodyServoController.structCommandIncluded;
        incomingCommand = commandsToReceiveFromBodyServoController.structCommand;
        processESPNOWIncomingMessage();
        }
    } else if (IncomingMacAddress = domeControllerMACAddressString) {
      memcpy(&commandsToReceiveFromDomeController, incomingData, sizeof(commandsToReceiveFromDomeController));
      incomingPassword = commandsToReceiveFromDomeController.structPassword;
      if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password was sent.  Message Ignored\n");  
      } else {
        incomingSenderID = commandsToReceiveFromDomeController.structSenderID;
        incomingTargetID = commandsToReceiveFromDomeController.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromDomeController.structCommandIncluded;
        incomingCommand = commandsToReceiveFromDomeController.structCommand;
        processESPNOWIncomingMessage();
        }
    } else if (IncomingMacAddress = domePlateControllerMACAddressString) {
      memcpy(&commandsToReceiveFromDomePlateController, incomingData, sizeof(commandsToReceiveFromDomePlateController));
      incomingPassword = commandsToReceiveFromDomePlateController.structPassword;
      if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password was sent.  Message Ignored\n");  
      } else {
        incomingSenderID = commandsToReceiveFromDomePlateController.structSenderID;
        incomingTargetID = commandsToReceiveFromDomePlateController.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromDomePlateController.structCommandIncluded;
        incomingCommand = commandsToReceiveFromDomePlateController.structCommand;
        processESPNOWIncomingMessage();
        }
    } else if (IncomingMacAddress = broadcastMACAddressString) {
      memcpy(&commandsToReceiveFromBroadcast, incomingData, sizeof(commandsToReceiveFromBroadcast));
      incomingPassword = commandsToReceiveFromBroadcast.structPassword;
      if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password was sent.  Message Ignored\n");  
      } else {
        incomingSenderID = commandsToReceiveFromBroadcast.structSenderID;
        incomingTargetID = commandsToReceiveFromBroadcast.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromBroadcast.structCommandIncluded;
        incomingCommand = commandsToReceiveFromBroadcast.structCommand;
        processESPNOWIncomingMessage();
        }
    }  else {Debug.ESPNOW("ESP-NOW Mesage ignored \n");}  
      colorWipeStatus("ES", blue, 10);

}

void processESPNOWIncomingMessage(){

  if (incomingTargetID == "DC" || incomingTargetID == "BR"){
    inputString = incomingCommand;
    stringComplete = true; 
    Debug.ESPNOW("Received packet from %s \n", incomingSenderID);

  }
}

  //////////////////////////////////////////////////////////////////////
  ///******             WiFi Specific Setup                     *****///
  //////////////////////////////////////////////////////////////////////
  
//LoRa Remote ESP           192.168.4.101   
//LoRa Droid ESP            192.168.4.108    ************ (Only used for OTA, Remote LoRa ESP must be on and close to Droid)
//Body Controller ESP       192.168.4.109    (Only used for OTA, Remote LoRa ESP must be on and close to Droid)
//Body Servo ESP            192.168.4.110   (Only used for OTA, Remote LoRa ESP must be on and close to Droid)
//Dome Controller ESP       192.168.4.111   (Only used for OTA, Remote LoRa ESP must be on and close to Droid)
//Dome Plate Controller ESP 192.168.4.112   (Only used for OTA, Remote LoRa ESP must be on and close to Droid)
//Droid Raspberry Pi        192.168.4.113
//Remote Raspberry Pi       192.168.4.114
//Developer Laptop          192.168.4.125
  
  // IP Address config of local ESP
  IPAddress local_IP(192,168,4,111);
  IPAddress subnet(255,255,255,0);
  IPAddress gateway(192,168,4,101);
  
  uint8_t oldLocalMACAddress[] = {0x24, 0x0A, 0xC4, 0xED, 0x30, 0x14};    //used when connecting to WiFi for OTA

  
  AsyncWebServer server(80);
  



///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                               /////
///////                                      Radar eye Camera LED Functions                           /////
///////                                                                                               /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void RadarEye_LED(uint32_t color, int RadarEyespeed1){
  int RadarEyeLow = 1;
  int RadarEyeHigh = 50;

  RE_currentMillis = millis();
      RadarEyespeed = map(RadarEyespeed1, 1, 9, 1, 250);
      if (RE_currentMillis - RE_startMillis >= RadarEyespeed){
      if(countUp == false){                   // check to see if the boolean flag is false.  If false, starting dimming the LEDs
      
          dim=dim - random(RadarEyeLow, RadarEyeHigh);  // set the brightness to current minus a random number between 5 and 40. I think that
                                              //adding a random causes a less smooth transition which makes it look a little better
          colorWipe(color, dim);              // Set the LEDs to the color and brightness using the colorWipe function
          }
      
        if(dim <= 10){                        //Check to see if the brightness is at or below 20.  Modifying the "20" will
                                              //allow the dim variable to go below zero causing the flicker.  The closer you
                                              //set the "20" to zero, the more flickering will happen. I use half the larger
                                              //dim random number to allow a small flicker without being too annoying.

           countUp = true;                    // if the dim variable is at or below "20", change the countUp flag to true      
          }
        if(countUp == true){                 // check to see if the boolean flag is true.  If true, starting brightening the LEDs
            dim=dim + random(RadarEyeLow, RadarEyeHigh); // set the brightness to current plus a random number between 5 and 40.  I think that
                                              //adding a random causes a less smooth transition which makes it look a little better
            colorWipe(color, dim);           // Set the LEDs to the color and brightness using the colorWheel function
        }
          if(dim>=250){                       //Check to see if the brightness is at or above 235.  Modifying the "235" will
                                               //allow the dim variable to go above 255 causing the flicker.  The closer you
                                              //set the "235" to 255, the more flickering will happen. I use half the larger
                                              //dim random number to allow a small flicker without being too annoying.
            countUp = false;                  // if the dim variable is at or above "235", change the countUp flag to false
          }
          RE_startMillis = RE_currentMillis; 
      }
      
  }

  //  Color Changing Function for the Camera Lens LEDs
void colorWipe(uint32_t c, int brightness) {
  for(uint16_t i=0; i<NUM_RADAR_EYE_PIXELS; i++) {
    RADAR_EYE_LEDS.setBrightness(brightness);
    RADAR_EYE_LEDS.setPixelColor(i, c);
    RADAR_EYE_LEDS.show();
  }
};

void clearRE() {
  for(uint16_t i=0; i<NUM_RADAR_EYE_PIXELS; i++) {
    RADAR_EYE_LEDS.setPixelColor(i, off);
    RADAR_EYE_LEDS.show();
  }
};

void REAuto () {
  if(millis() - RadarEyeAutoTimer >= RadarEyeAutoInt*1000) {       // and the timer has reached the set interval
    if(millis() - RadarEyeAutoTimer >= (RadarEyeAutoInt+RadarEyeAutoPause)*1000) {     // Assign a random command string from the Auto Command Array to the input string
      if(!autoComplete) {
        RadarEyeAutoTimer = millis();
        RadarEyeAutoPause = random(RadarEyeAutoPauseMin,RadarEyeAutoPauseMax);
        RadarEyeAutoInt = random(RadarEyeAutoIntMin,RadarEyeAutoIntMax);
        autoInputString = RadarEyeAutoCommands[random((RadarEyeAutoCommandsCount-1))];
        autoComplete = true;
      }
    }
    else {
      RE_command[0] = 99;
    }                                                             // and set flag so new command is processes at beginning of loop
  }
}

void colorWipeStatus(String statusled, uint32_t c, int brightness) {
  if(statusled == "ES"){
    ESP_LED.setBrightness(brightness);
    ESP_LED.setPixelColor(0, c);
    ESP_LED.show();
  } else{
  // Debug.DBG("No LED was chosen \n");
  }
  };



///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                               /////
///////                                       Door Functions                                          /////
///////                                                                                               /////
///////              Door Command Stucture: Dxyyzzabbccccddddeeeee                                    /////
///////                 D = Door Command                                                              /////
///////                 x = Servo Board                                                               /////
///////                   1 = Body Only                                                               /////
///////                   2 = Dome Only                                                               /////
///////                   3 = Both, starting with the body                                            /////
///////                   4 = Both, starting with the dome                                            /////
///////                 yy = Door Function                                                            /////
///////                 zz = Door Specified (Only used for Dx01 & Dx02)                               /////
///////                 a = Specify easing/delayCall options (B,E,D)                                  /////
///////                   B = Use Easing and Delay Call                                               /////
///////                   E = Easing Only (if VarSpeed is not needed, don't input dddd)               /////
///////                   D = Delay Call Only (don't input bbccccdddd)                                /////
///////                 bb = Easing Method (only needed if B or E was selected)
///////                 cccc = Easing Method Duration (only needed if B or E was selected)
///////                 dddd = Easing Method VarSpeedMax(only needed if B or E was selected and VARSPEED wanted)
///////                 eeeee = Delay Call Duration (only needed if B or D was selected)
///////                                                                                               /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void openDoor(int servoBoard, int doorpos, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax) {
  //Command: Dx01zz
  Debug.DBG("Open Specific Door\n");
  if (servoBoard == 1 || servoBoard == 3 || servoBoard == 4){
    switch (doorpos){
      case 1: Debug.DBG("Open Top Utility Arm\n");
              snprintf(stringToSend, sizeof(stringToSend), "D10101E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
              sendESPNOWCommand("BS", stringToSend);                                        
              break;
      case 2: Debug.DBG("Open Bottom Utility Arm\n");
              snprintf(stringToSend, sizeof(stringToSend), "D10102E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
              sendESPNOWCommand("BS", stringToSend);                                        
              break;
      case 3: Debug.DBG("Open Large Left Door\n");
              snprintf(stringToSend, sizeof(stringToSend), "D10103E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
              sendESPNOWCommand("BS", stringToSend);                                        
              break;
      case 4: Debug.DBG("Open Large Right Door\n");
              snprintf(stringToSend, sizeof(stringToSend), "D10104E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
              sendESPNOWCommand("BS", stringToSend);                                        
              break;
      case 5: Debug.DBG("Open Charge Bay Indicator Door\n");
              snprintf(stringToSend, sizeof(stringToSend), "D10105E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
              sendESPNOWCommand("BS", stringToSend);                                        
              break;
      case 6: Debug.DBG("Open Data Panel Door\n");
              snprintf(stringToSend, sizeof(stringToSend), "D10106E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
              sendESPNOWCommand("BS", stringToSend);                                        
              break;
    }
  };
  if (servoBoard == 2 || servoBoard == 3 || servoBoard == 4){
    setServoEasingMethod(servoEasingMethod);
    switch (doorpos){
      case 1: Serial.println("Open SMALL_PANEL_ONE");       SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, SMALL_PANEL_ONE, varSpeedMin, varSpeedMax);     break;
      case 2: Serial.println("Open SMALL_PANEL_TWO");       SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, SMALL_PANEL_TWO, varSpeedMin, varSpeedMax);     break;
      case 3: Serial.println("Open SMALL_PANEL_THREE");     SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, SMALL_PANEL_THREE, varSpeedMin, varSpeedMax);   break;
      case 4: Serial.println("Open MEDIUM_PANEL_PAINTED");  SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, MEDIUM_PANEL_PAINTED, varSpeedMin, varSpeedMax);break;
      case 5: Serial.println("Open MEDIUM_PANEL_SILVER");   SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, MEDIUM_PANEL_SILVER, varSpeedMin, varSpeedMax); break;
      case 6: Serial.println("Open BIG_PANEL");             SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, BIG_PANEL, varSpeedMin, varSpeedMax);           break;
      case 7: Serial.println("Open PIE_PANEL_ONE");         SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, PIE_PANEL_ONE, varSpeedMin, varSpeedMax);       break;
      case 8: Serial.println("Open PIE_PANEL_TWO");         SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, PIE_PANEL_TWO, varSpeedMin, varSpeedMax);       break;
      case 9: Serial.println("Open PIE_PANEL_THREE");       SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, PIE_PANEL_THREE, varSpeedMin, varSpeedMax);     break;
      case 10: Serial.println("Open PIE_PANEL_FOUR");       SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, PIE_PANEL_FOUR, varSpeedMin, varSpeedMax);      break;

    }
  };
  D_command[0]   = '\0';
};

void closeDoor(int servoBoard, int doorpos, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax) {
  // Command: Dx02zz
  Debug.DBG("Close Specific Door");
  if (servoBoard == 1 || servoBoard == 3 || servoBoard == 4){
    switch(doorpos){
      case 1: Debug.DBG("Close Top Utility Arm\n");             
              snprintf(stringToSend, sizeof(stringToSend), "D10201E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
              sendESPNOWCommand("BS", stringToSend);                                        
              break;
      case 2: Debug.DBG("Close Bottom Utility Arm\n");              
              snprintf(stringToSend, sizeof(stringToSend), "D10202E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
              sendESPNOWCommand("BS", stringToSend);                                        
              break;
      case 3: Debug.DBG("Close Large Left Door\n");
              snprintf(stringToSend, sizeof(stringToSend), "D10203E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
              sendESPNOWCommand("BS", stringToSend);                                        
              break;
      case 4: Debug.DBG("Close Large Right Door\n");
              snprintf(stringToSend, sizeof(stringToSend), "D10204E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
              sendESPNOWCommand("BS", stringToSend);                                        
              break;
      case 5: Debug.DBG("Close Charge Bay Indicator Door\n");
              snprintf(stringToSend, sizeof(stringToSend), "D10205E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
              sendESPNOWCommand("BS", stringToSend);                                        
              break;
      case 6: Debug.DBG("Close Data Panel Door\n");
              snprintf(stringToSend, sizeof(stringToSend), "D10206E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
              sendESPNOWCommand("BS", stringToSend);                                        
              break;
    }
  };
  if (servoBoard == 2 || servoBoard == 3 || servoBoard == 4){
    setServoEasingMethod(servoEasingMethod);
    switch (doorpos){
      case 1: Serial.println("Close SMALL_PANEL_ONE");       SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, SMALL_PANEL_ONE, varSpeedMin, varSpeedMax);     break;
      case 2: Serial.println("Close SMALL_PANEL_TWO");       SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, SMALL_PANEL_TWO, varSpeedMin, varSpeedMax);     break;
      case 3: Serial.println("Close SMALL_PANEL_THREE");     SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, SMALL_PANEL_THREE, varSpeedMin, varSpeedMax);   break;
      case 4: Serial.println("Close MEDIUM_PANEL_PAINTED");  SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, MEDIUM_PANEL_PAINTED, varSpeedMin, varSpeedMax);break;
      case 5: Serial.println("Close MEDIUM_PANEL_SILVER");   SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, MEDIUM_PANEL_SILVER, varSpeedMin, varSpeedMax); break;
      case 6: Serial.println("Close BIG_PANEL");             SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, BIG_PANEL, varSpeedMin, varSpeedMax);           break;
      case 7: Serial.println("Close PIE_PANEL_ONE");         SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, PIE_PANEL_ONE, varSpeedMin, varSpeedMax);       break;
      case 8: Serial.println("Close PIE_PANEL_TWO");         SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, PIE_PANEL_TWO, varSpeedMin, varSpeedMax);       break;
      case 9: Serial.println("Close PIE_PANEL_THREE");       SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, PIE_PANEL_THREE, varSpeedMin, varSpeedMax);     break;
      case 10: Serial.println("Close PIE_PANEL_FOUR");       SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, PIE_PANEL_FOUR, varSpeedMin, varSpeedMax);      break;
    }
  };
  D_command[0]   = '\0';
}


void openAllDoors(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration) {
  // Command: Dx03
  Debug.DBG("Open all Doors\n");
  fVarSpeedMin = varSpeedMin;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  fVarSpeedMax = varSpeedMax;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  if (delayCallDuration == 0){delayCallDuration = defaultESPNOWSendDuration;}               //sets default delayCall to allow time for ESP-NOW message to get to reciever ESP.
  snprintf(stringToSend, sizeof(stringToSend),"D103E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend); break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax);  break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([]{sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;
  }
  D_command[0] = '\0';
}


void closeAllDoors(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration) {
  // Command: Dx04
  Debug.DBG("Close all Doors\n");
  fVarSpeedMin = varSpeedMin;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  fVarSpeedMax = varSpeedMax;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  if (delayCallDuration == 0){delayCallDuration = defaultESPNOWSendDuration;}               //sets default delayCall to allow time for ESP-NOW message to get to reciever ESP.
  snprintf(stringToSend, sizeof(stringToSend), "D104E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend); break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax);  break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([]{sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;
  }
  D_command[0] = '\0';
}


void shortCircuit(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration) {
  // Command: Dx05
  // add sequence for this routine.  
}


void allOpenClose(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration){
  // Command: Dx06
  Debug.DBG("Open and Close All Doors\n");
  fVarSpeedMin = varSpeedMin;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  fVarSpeedMax = varSpeedMax;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  if (delayCallDuration == 0){delayCallDuration = defaultESPNOWSendDuration;}               //sets default delayCall to allow time for ESP-NOW message to get to reciever ESP.
  snprintf(stringToSend, sizeof(stringToSend), "D106E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend); break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpenClose, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax);  break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpenClose, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpenClose, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([]{sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;
  }
  D_command[0]   = '\0';                                           
}


void allOpenCloseLong(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration){
  // Command: Dx07
  Debug.DBG("Open and Close Doors Long\n");
  fVarSpeedMin = varSpeedMin;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  fVarSpeedMax = varSpeedMax;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  if (delayCallDuration == 0){delayCallDuration = defaultESPNOWSendDuration;}               //sets default delayCall to allow time for ESP-NOW message to get to reciever ESP.
  snprintf(stringToSend, sizeof(stringToSend), "D107E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend); break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpenCloseLong, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax);  break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpenCloseLong, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpenCloseLong, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([]{sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;
  }  
  D_command[0]   = '\0';                                                 
}


void allFlutter(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration){
  // Command: Dx08
  Debug.DBG("Flutter All Doors\n");
  fVarSpeedMin = varSpeedMin;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  fVarSpeedMax = varSpeedMax;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  if (delayCallDuration == 0){delayCallDuration = defaultESPNOWSendDuration;}               //sets default delayCall to allow time for ESP-NOW message to get to reciever ESP.
  snprintf(stringToSend, sizeof(stringToSend), "D108E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend); break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax);  break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([]{sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;
  }
  D_command[0]   = '\0';   
}


void allOpenCloseRepeat(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration){
  // Command: Dx09
  Debug.DBG("Open and Close All Doors Repeat\n");
  fVarSpeedMin = varSpeedMin;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  fVarSpeedMax = varSpeedMax;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  if (delayCallDuration == 0){delayCallDuration = defaultESPNOWSendDuration;}               //sets default delayCall to allow time for ESP-NOW message to get to reciever ESP.
  snprintf(stringToSend, sizeof(stringToSend), "D109E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend); break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFOpenCloseRepeat, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax);  break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFOpenCloseRepeat, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFOpenCloseRepeat, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([]{sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;
  }
  D_command[0]   = '\0';             
}


void panelWave(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration){
  // Command: Dx10
  Debug.DBG("Wave\n");
  fVarSpeedMin = varSpeedMin;
  fVarSpeedMax = varSpeedMax;
  snprintf(stringToSend, sizeof(stringToSend), "D110E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend); break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelWave, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); break;
    case 3: sendESPNOWCommand("BS", stringToSend);
            if (delayCallDuration == 0){delayCallDuration = 2125;}               //sets default delayCall to allow time for ESP-NOW message to get to reciever ESP.
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelWave, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelWave, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([]{sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;
  }
  D_command[0]   = '\0';                                             
}


void panelWaveFast(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration){
  // Command: Dx11  
  Debug.DBG("Wave Fast\n");
  fVarSpeedMin = varSpeedMin;
  fVarSpeedMax = varSpeedMax;
  snprintf(stringToSend, sizeof(stringToSend), "D111E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend);  break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelWaveFast, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax);  break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelWaveFast, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelWaveFast, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax);
            DelayCall::schedule([] {sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;
  }
  D_command[0]   = '\0';                                             
}


void openCloseWave(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration) {
  // Command: Dx12
  Debug.DBG("Open Close Wave \n");
  fVarSpeedMin = varSpeedMin;
  fVarSpeedMax = varSpeedMax;
  snprintf(stringToSend, sizeof(stringToSend), "D112E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend); break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelOpenCloseWave, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax);  break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelOpenCloseWave, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelOpenCloseWave, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([] {sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;
  }
  D_command[0]   = '\0';                                             
}


void marchingAnts(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration) {
  // Command: Dx13
  Debug.DBG("Marching Ants\n");
  fVarSpeedMin = varSpeedMin;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  fVarSpeedMax = varSpeedMax;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  if (delayCallDuration == 0){delayCallDuration = defaultESPNOWSendDuration;}               //sets default delayCall to allow time for ESP-NOW message to get to reciever ESP.
  snprintf(stringToSend, sizeof(stringToSend), "D113E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend);                                      break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelMarchingAnts, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelMarchingAnts, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelMarchingAnts, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([]{sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;    
  }
  D_command[0]   = '\0';                                             
}


void panelAlternate(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration) {
  // Command: Dx14
  Debug.DBG("Panel Alternate\n");
  fVarSpeedMin = varSpeedMin;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  fVarSpeedMax = varSpeedMax;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  if (delayCallDuration == 0){delayCallDuration = defaultESPNOWSendDuration;}               //sets default delayCall to allow time for ESP-NOW message to get to reciever ESP.
  snprintf(stringToSend, sizeof(stringToSend), "D114E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend);                                      break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAlternate, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAlternate, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAlternate, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([]{sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;    
  }
  D_command[0]   = '\0';                                             
}                                                            


void panelDance(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration) {
 // Command: Dx15
  Debug.DBG("Panel Dance\n");
  fVarSpeedMin = varSpeedMin;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  fVarSpeedMax = varSpeedMax;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  if (delayCallDuration == 0){delayCallDuration = defaultESPNOWSendDuration;}               //sets default delayCall to allow time for ESP-NOW message to get to reciever ESP.
  snprintf(stringToSend, sizeof(stringToSend), "D115E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend);                                      break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelDance, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelDance, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelDance, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([]{sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;    
  } 
  D_command[0]   = '\0';                                             
};



void longDisco(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration) {
  // Command: Dx16
  Debug.DBG("Panel Dance Long Disco\n");
  fVarSpeedMin = varSpeedMin;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  fVarSpeedMax = varSpeedMax;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  if (delayCallDuration == 0){delayCallDuration = defaultESPNOWSendDuration;}               //sets default delayCall to allow time for ESP-NOW message to get to reciever ESP.
  snprintf(stringToSend, sizeof(stringToSend), "D116E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend);                                      break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelLongDisco, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelLongDisco, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration); break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelLongDisco, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([]{sendESPNOWCommand("BS", stringToSend);}, delayCallDuration); break;    
  } 
  D_command[0]  = '\0';                                     
};


void longHarlemShake(int servoBoard, int servoEasingMethod, uint32_t varSpeedMin, uint32_t varSpeedMax, uint32_t delayCallDuration) {
  // Command: Dx17
  Debug.DBG("Harlem Shake\n");
  fVarSpeedMin = varSpeedMin;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  fVarSpeedMax = varSpeedMax;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  if (delayCallDuration == 0){delayCallDuration = defaultESPNOWSendDuration;}               //sets default delayCall to allow time for ESP-NOW message to get to reciever ESP.
  snprintf(stringToSend, sizeof(stringToSend), "D117E%02d%04d%04d", servoEasingMethod, varSpeedMin, varSpeedMax);
  setServoEasingMethod(servoEasingMethod);
  switch(servoBoard){
    case 1: sendESPNOWCommand("BS", stringToSend);                                                                                                                            break;
    case 2: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelLongHarlemShake, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax);                                                  break;
    case 3: sendESPNOWCommand("BS", stringToSend);     
            DelayCall::schedule([] {SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelLongHarlemShake, ALL_SERVOS_MASK, fVarSpeedMin, fVarSpeedMax);}, delayCallDuration);  break;
    case 4: SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelLongHarlemShake, ALL_SERVOS_MASK, varSpeedMin, varSpeedMax); 
            DelayCall::schedule([]{sendESPNOWCommand("BS", stringToSend);}, delayCallDuration);                                                                               break;    
  }   
  D_command[0]  = '\0';                                             
};                                                    


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                               /////
///////                             Serial & ESP-NOW Communication Functions                          /////
///////                                                                                               /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//      /////////////////////////////////////////////////////////
//      ///*****          Serial Event Function          *****///
//      /////////////////////////////////////////////////////////
//      /// This routine is run between loop() runs, so using ///
//      /// delay inside loop can delay response.  Multiple   ///
//      /// bytes of data may be available.                   ///
//      /////////////////////////////////////////////////////////
//
void serialEvent() {
  //int count = 0;
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
      stringComplete = true;            // set a flag so the main loop can do something about it.
    }
  }
  // Debug.DBG("Received %s\n", inputString);
}

void hpSerialEvent() {
  while (hpSerial.available()) {
    char inChar = (char)hpSerial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  Debug.DBG("Recieved %s\n", inputString);
}

void rsSerialEvent() {
  while (rsSerial.available()) {
    char inChar = (char)rsSerial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  Debug.DBG("Received %s\n", inputString);
}

void psSerialEvent() {
  while (psSerial.available()) {
    char inChar = (char)psSerial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  Debug.DBG("Received %s\n", inputString);
}

void fuSerialEvent() {
  while (fuSerial.available()) {
    char inChar = (char)fuSerial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  Debug.DBG("Received %s\n", inputString);
}

 /////////////////////////////////////////////////////////
  ///*****          Serial Write Function          *****///
  /////////////////////////////////////////////////////////
  /// These functions recieve a string and transmits    ///
  /// one character at a time and adds a '/r' to the    ///
  /// end of the string.                                ///
  /////////////////////////////////////////////////////////

void writeSerialString(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    Serial.write(completeString[i]);
  }
}

void writeRsSerial(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    rsSerial.write(completeString[i]);
  }
  Debug.DBG("Printing to rsSerial\n");
}

void writeHpSerial(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++){
    hpSerial.write(completeString[i]);
  }
  Debug.DBG("Printing to hpSerial\n");
}

void writeFuSerial(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++){
    fuSerial.write(completeString[i]);
  }
  Debug.DBG("Printing to fuSerial\n");
}

void writePsSerial(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    psSerial.write(completeString[i]);
  }
  Debug.DBG("Printing to rsSerial\n");
}

//////////////////////////////////////////////////////////////////////
///*****             ESP-NOW Functions                        *****///
//////////////////////////////////////////////////////////////////////

void setupSendStruct(espnow_struct_message* msg, String pass, String sender, String targetID, bool hascommand, String cmd)
{
    snprintf(msg->structPassword, sizeof(msg->structPassword), "%s", pass.c_str());
    snprintf(msg->structSenderID, sizeof(msg->structSenderID), "%s", sender.c_str());
    snprintf(msg->structTargetID, sizeof(msg->structTargetID), "%s", targetID.c_str());
    msg->structCommandIncluded = hascommand;
    snprintf(msg->structCommand, sizeof(msg->structCommand), "%s", cmd.c_str());
}

void sendESPNOWCommand(String starget, String scomm){
  String senderID = "DC";   // change to match location (BC/BS/DC/DP/LD)
  String scommEval = "";
  bool hasCommand;
  if (scommEval = scomm){
    hasCommand = 0;
  } else {hasCommand = 1;};

   if (starget == "LD"){
    setupSendStruct(&commandsToSendtoDroidLoRa, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(droidLoRaMACAddress, (uint8_t *) &commandsToSendtoDroidLoRa, sizeof(commandsToSendtoDroidLoRa));
    if (result == ESP_OK) {Debug.DBG("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.DBG("Error sending the data\n");}
  } else if (starget == "BC"){
    setupSendStruct(&commandsToSendtoBodyController, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(bodyControllerMACAddress, (uint8_t *) &commandsToSendtoBodyController, sizeof(commandsToSendtoBodyController));
    if (result == ESP_OK) {Debug.DBG("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.DBG("Error sending the data\n");}
  } else if (starget == "BS"){
    setupSendStruct(&commandsToSendtoBodyServoController, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
       esp_err_t result = esp_now_send(bodyServosControllerMACAddress, (uint8_t *) &commandsToSendtoBodyServoController, sizeof(commandsToSendtoBodyServoController));
    if (result == ESP_OK) {Debug.DBG("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.DBG("Error sending the data\n");}
  }  else if (starget == "DC"){
    setupSendStruct(&commandsToSendtoDomeController, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
       esp_err_t result = esp_now_send(domeControllerMACAddress, (uint8_t *) &commandsToSendtoDomeController, sizeof(commandsToSendtoDomeController));
    if (result == ESP_OK) {Debug.DBG("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.DBG("Error sending the data\n");}
  } else if (starget == "DP"){
    setupSendStruct(&commandsToSendtoDomePlateController, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
       esp_err_t result = esp_now_send(domePlateControllerMACAddress, (uint8_t *) &commandsToSendtoDomePlateController, sizeof(commandsToSendtoDomePlateController));
    if (result == ESP_OK) {Debug.DBG("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.DBG("Error sending the data\n");}
  } else if (starget == "BR"){
    setupSendStruct(&commandsToSendtoBroadcast, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
       esp_err_t result = esp_now_send(broadcastMACAddress, (uint8_t *) &commandsToSendtoBroadcast, sizeof(commandsToSendtoBroadcast));
    if (result == ESP_OK) {Debug.DBG("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.DBG("Error sending the data\n");}
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                               /////
///////                             Miscellaneous Functions                                           /////
///////                                                                                               /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
///*****    Connects to WiFi and turns on OTA functionality   *****///
//////////////////////////////////////////////////////////////////////

void connectWiFi(){
  esp_now_deinit();
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(500);

  Serial.println(WiFi.config(local_IP, gateway, subnet) ? "Client IP Configured" : "Failed!");
  WiFi.mode(WIFI_STA);
  esp_wifi_set_mac(WIFI_IF_STA, &oldLocalMACAddress[0]);
  
  delay(500);
  
  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.println("Connecting to WiFi..");
  }
  Serial.print("SSID: \t");Serial.println(WiFi.SSID());
  Serial.print("IP Address: \t");Serial.println(WiFi.localIP());
  Serial.print("MAC Address: \t");Serial.println(WiFi.macAddress());
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Please go to http://192.168.4.111/update to upload file");
  });
  
  AsyncElegantOTA.begin(&server);    // Start AsyncElegantOTA
  server.begin();

  Local_Command[0]   = '\0';
}   

//////////////////////////////////////////////////////////////////////
///*****        Sets Servo Easing Method                      *****///
//////////////////////////////////////////////////////////////////////

void setServoEasingMethod(int easingMethod){
  switch(easingMethod){
    case 1: servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::LinearInterpolation);    break;
    case 2: servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::QuadraticEaseIn);        break;
    case 3: servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::QuadraticEaseOut);       break;
    case 4: servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::QuadraticEaseInOut);     break;
    case 5: servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::CubicEaseIn);            break;
    case 6: servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::CubicEaseOut);           break;
    case 7: servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::CubicEaseInOut);         break;
    case 8: servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::QuarticEaseIn);          break;
    case 9: servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::QuarticEaseOut);         break;
    case 10:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::QuarticEaseInOut);      break;
    case 11:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::QuinticEaseIn);         break;
    case 12:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::QuinticEaseOut);        break;
    case 13:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::QuinticEaseInOut);      break;
    case 14:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::SineEaseIn);            break;
    case 15:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::SineEaseOut);           break;
    case 16:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::SineEaseInOut);         break;
    case 17:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::CircularEaseIn);        break;
    case 18:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::CircularEaseOut);       break;
    case 19:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::CircularEaseInOut);     break;
    case 20:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::ExponentialEaseIn);     break;
    case 21:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::ExponentialEaseOut);    break;
    case 22:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::ExponentialEaseInOut);  break;
    case 23:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::ElasticEaseIn);         break;
    case 24:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::ElasticEaseOut);        break;
    case 25:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::ElasticEaseInOut);      break;
    case 26:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::BackEaseIn);            break;
    case 27:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::BackEaseOut);           break;
    case 28:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::BackEaseInOut);         break;
    case 29:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::BounceEaseIn);          break;
    case 30:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::BounceEaseOut);         break;
    case 31:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::BounceEaseInOut);       break;
    default:  servoDispatch.setServosEasingMethod(ALL_SERVOS_MASK, Easing::LinearInterpolation); 
              Debug.DBG("No Easing Method Selected\n");                                                  break;
  }
}

//////////////////////////////////////////////////////////////////////
///*****          Scan I2C for devices                        *****///
//////////////////////////////////////////////////////////////////////

void scan_i2c(){
    unsigned nDevices = 0;
    for (byte address = 1; address < 127; address++){
      String name = "<unknown>";
      Wire.beginTransmission(address);
      byte error = Wire.endTransmission();
      if (address == 0x70){
          // All call address for PCA9685
        name = "PCA9685:all";
      }
      if (address == 0x40){
        // Adafruit PCA9685
        name = "PCA9685";
      }
      if (address == 0x14){
        // IA-Parts magic panel
        name = "IA-Parts Magic Panel";
      }
      if (address == 0x20){
        // IA-Parts periscope
        name = "IA-Parts Periscope";
      }
      if (address == 0x16){
        // PSIPro
        name = "PSIPro";
      }

      if (error == 0){
        Serial.print("I2C device found at address 0x");
        if (address < 16)
          Serial.print("0");
          Serial.print(address, HEX);
          Serial.print(" ");
          Serial.println(name);
          nDevices++;
      }
        else if (error == 4){
          Serial.print("Unknown error at address 0x");
          if (address < 16)
            Serial.print("0");
            Serial.println(address, HEX);
        }
    }
    if (nDevices == 0)
        Serial.println("No I2C devices found\n");
    else
        Serial.println("done\n");
}

//////////////////////////////////////////////////////////////////////
///*****    Send Keepalive Messages for Status                *****///
//////////////////////////////////////////////////////////////////////
  // KeepAlive Message to show status on website.



void keepAlive(){
  if (millis() - keepAliveMillis >= keepAliveDuration + random(1,1000)){
    keepAliveMillis = millis();
    sendESPNOWCommand("LD","");
  } 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////                                                                                       /////////     
/////////                             END OF FUNCTIONS                                          /////////
/////////                                                                                       /////////     
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////


void setup(){
  //Initialize the Serial Ports
  Serial.begin(115200);                                                                   // Initialize Serial Connection at 115200:
  hpSerial.begin(HP_BAUD_RATE,SERIAL_8N1,SERIAL_RX_HP_PIN,SERIAL_TX_HP_PIN);
  rsSerial.begin(RS_BAUD_RATE,SERIAL_8N1,SERIAL_RX_RS_PIN,SERIAL_TX_RS_PIN);
  psSerial.begin(PS_BAUD_RATE,SWSERIAL_8N1,SERIAL_RX_PSI_PIN,SERIAL_TX_PSI_PIN,false,95); 
  fuSerial.begin(FU_BAUD_RATE,SWSERIAL_8N1,SERIAL_RX_FU_PIN,SERIAL_TX_FU_PIN,false,95);  
 
  Serial.println("\n\n----------------------------------------");
  Serial.print("Booting up the ");Serial.println(HOSTNAME);
  Serial.println("----------------------------------------");

  //Initialize I2C for the Servo Expander Board
  Wire.begin();
  
  //Initialize the ReelTwo Library
  SetupEvent::ready();

  //Reserve the inputStrings
  inputString.reserve(100);                                                              // Reserve 100 bytes for the inputString:
  autoInputString.reserve(100);

  //Initialize the NeoPixel ring for the camera lens/radar eye
  RADAR_EYE_LEDS.begin();
  RADAR_EYE_LEDS.show(); // Initialize all pixels to 'off'
  colorWipe(red, 255); // red during bootup

  ESP_LED.begin();
  ESP_LED.show();
  colorWipeStatus("ES",red,255);
  
  Serial.println("LED Setup Complete");


  //initialize WiFi for ESP-NOW
  WiFi.mode(WIFI_STA);
  esp_wifi_set_mac(WIFI_IF_STA, &domeControllerMACAddress[0]);
  Serial.print("Local STA MAC address = ");
  Serial.println(WiFi.macAddress());

//Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
  return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  //  peerInfo.ifidx=WIFI_IF_AP;

// Add peers  
 // Broadcast
  memcpy(peerInfo.peer_addr, broadcastMACAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add Broadcast ESP-NOW peer");
    return;
  }
    // Droid LoRa Controller
  memcpy(peerInfo.peer_addr, droidLoRaMACAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add Broadcast ESP-NOW peer");
    return;
  }  
 // Body Controller
  memcpy(peerInfo.peer_addr, bodyControllerMACAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add Broadcast ESP-NOW peer");
    return;
  }
   // Body Servo Controller
  memcpy(peerInfo.peer_addr, bodyServosControllerMACAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add Broadcast ESP-NOW peer");
    return;
  }
// // Dome Controller
//   memcpy(peerInfo.peer_addr, domeControllerMACAddress, 6);
//   if (esp_now_add_peer(&peerInfo) != ESP_OK){
//     Serial.println("Failed to add Broadcast ESP-NOW peer");
//     return;
//   } 
// Dome Plate Controller
  memcpy(peerInfo.peer_addr, domePlateControllerMACAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add Broadcast ESP-NOW peer");
    return;
  } 
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);

}  // end of Setup


void loop() {
if (millis() - MLMillis >= mainLoopDelayVar){
  MLMillis = millis();
  RE_loopTime = millis();
  AnimatedEvent::process();
  if(startUp) {
      closeAllDoors(2,0,0,0,0);
      startUp = false;
      Serial.println("Startup completed, now running loop");
      colorWipeStatus("ES",blue,10);

  }
  keepAlive();
  if(Serial.available()){serialEvent();}
  if(hpSerial.available()){hpSerialEvent();}
  if(rsSerial.available()){rsSerialEvent();}
  if(fuSerial.available()){fuSerialEvent();}

//  RadarEye_LED(blue, 5); // blue

  if (stringComplete) {autoComplete=false;}
  if (stringComplete || autoComplete) {
    if(stringComplete) {inputString.toCharArray(inputBuffer, 100);inputString="";}
      else if (autoComplete) {autoInputString.toCharArray(inputBuffer, 100);autoInputString="";}
         if (inputBuffer[0] == '#'){
        if (
            inputBuffer[1]=='D' ||          // Command for debugging
            inputBuffer[1]=='d' ||          // Command for debugging
            inputBuffer[1]=='L' ||          // Command designator for internal functions
            inputBuffer[1]=='l' ||          // Command designator for internal functions
            inputBuffer[1]=='E' ||          // Command designator for storing EEPROM data
            inputBuffer[1]=='e'           // Command designator for storing EEPROM data
          ){commandLength = strlen(inputBuffer); 
            if (inputBuffer[1]=='D' || inputBuffer[1]=='d'){
              debugInputIdentifier = "";                            // flush the string
              for (int i=2; i<=commandLength-2; i++){
                char inCharRead = inputBuffer[i];
                debugInputIdentifier += inCharRead;                   // add it to the inputString:
              }
              debugInputIdentifier.toUpperCase();
              Debug.toggle(debugInputIdentifier);
              debugInputIdentifier = "";                             // flush the string
              } else if (inputBuffer[1]=='L' || inputBuffer[1]=='l') {
                localCommandFunction = (inputBuffer[2]-'0')*10+(inputBuffer[3]-'0');
                Local_Command[0]   = '\0';                                                            // Flushes Array
                Local_Command[0] = localCommandFunction;
              Debug.LOOP("Entered the Local Command Structure /n");
              } else if (inputBuffer[1] == 'E' || inputBuffer[1] == 'e'){
                Debug.LOOP("EEPROM configuration selected /n");
                // need to actually add the code to implement this.

              } else {Debug.LOOP("No valid command entered /n");}
              
          }
              if(Local_Command[0]){
                switch (Local_Command[0]){
                  case 1: Serial.println(HOSTNAME);
                        Local_Command[0]   = '\0';                                                           break;
                  case 2: Serial.println("Resetting the ESP in 3 Seconds");
                          DelayCall::schedule([] {ESP.restart();}, 3000);
                          Local_Command[0]   = '\0';                                                           break;
                  case 3: connectWiFi();                                                                          break;
                  case 4: ; break;  //reserved for future use
                  case 5: ; break;  //reserved for future use
                  case 6: ; break;  //reserved for future use
                  case 7: ; break;  //reserved for future use
                  case 8: ; break;  //reserved for future use
                  case 9: scan_i2c(); Local_Command[0]='\0';                                                        break;  //Used for scanning I2C

                }
              }





        }else if (inputBuffer[0] == ':'){
     
          if( inputBuffer[1]=='D' ||        // Door Designator
          inputBuffer[1]=='d' ||        // Door Designator
          inputBuffer[1]=='R' ||        // Radar Eye LED
          inputBuffer[1]=='r' ||        // Radar Eye LED
          inputBuffer[1]=='E' ||        // Command designator for ESP-NOW functions
          inputBuffer[1]=='E' ||        // Command designator for ESP-NOW functions
          inputBuffer[1]=='S' ||        // Command for sending Serial Strings out Serial ports
          inputBuffer[1]=='s'           // Command for sending Serial Strings out Serial ports

        ){commandLength = strlen(inputBuffer);                     //  Determines length of command character array.
          Debug.DBG("Command: %s with a length of %d \n", inputBuffer, commandLength);
          if(commandLength >= 3) {
            if(inputBuffer[1]=='D' || inputBuffer[1]=='d') {                                                            // specifies the overall door command
              doorBoard = inputBuffer[2]-'0';                                                                           // Sets the board to call the command on.
              doorFunction = (inputBuffer[3]-'0')*10+(inputBuffer[4]-'0');                                              // Sets the door command to a specific function
              if (doorFunction == 1 || doorFunction == 2){                                                              // Checks the door command to see if it's calling to open a single door
                door = (inputBuffer[5]-'0')*10+(inputBuffer[6]-'0');                                                    // Sets the specific door to move
                if (inputBuffer[7] == 'D' || inputBuffer[7] == 'd'){
                  Debug.DBG("with DelayCall \n");
                  delayCallTime =  (inputBuffer[8]-'0')*10000+(inputBuffer[9]-'0')*1000+(inputBuffer[10]-'0')*100+(inputBuffer[11]-'0')*10+(inputBuffer[12]-'0');  // converts 5 digit character to uint32_t
                  doorEasingMethod = 0;                                                                                                                           // Sets Easing Method to 0-Off
                  cVarSpeedMin = 0;
                  cVarSpeedMax = 0;                                                                                                                        // Sets Easing duration to 0-Off
                } else if (inputBuffer[7] == 'E' ||inputBuffer[7] == 'e'){
                  Debug.DBG("with Easing \n");
                  doorEasingMethod = (inputBuffer[8]-'0')*10+(inputBuffer[9]-'0');
                  doorEasingDuration = (inputBuffer[10]-'0')*1000+(inputBuffer[11]-'0')*100+(inputBuffer[12]-'0')*10+(inputBuffer[13]-'0');                
                  delayCallTime = 0;
                } else if (inputBuffer[7] == 'B' || inputBuffer[7] == 'b'){
                  Debug.DBG("with Both Easing and Delay Call \n");
                  doorEasingMethod = (inputBuffer[8]-'0')*10+(inputBuffer[9]-'0');
                  cVarSpeedMin = (inputBuffer[10]-'0')*1000+(inputBuffer[11]-'0')*100+(inputBuffer[12]-'0')*10+(inputBuffer[13]-'0');                
                  cVarSpeedMax = (inputBuffer[14]-'0')*1000+(inputBuffer[15]-'0')*100+(inputBuffer[16]-'0')*10+(inputBuffer[17]-'0');
                  delayCallTime =  (inputBuffer[18]-'0')*10000+(inputBuffer[19]-'0')*1000+(inputBuffer[20]-'0')*100+(inputBuffer[21]-'0')*10+(inputBuffer[22]-'0');
                }else{
                  Debug.DBG("No easing or Delay time specified \n");
                  delayCallTime = 0;
                  doorEasingMethod = 0;
                  cVarSpeedMin = 0;
                  cVarSpeedMax = 0;                
                }
              }
              else if (doorFunction != 1 || doorFunction != 2) {
                Debug.DBG("Other Door Function Called \n");
                if (inputBuffer[5] == 'D' || inputBuffer[5] == 'd'){
                  Debug.DBG("with DelayCall \n");
                  delayCallTime =  (inputBuffer[6]-'0')*10000+(inputBuffer[7]-'0')*1000+(inputBuffer[8]-'0')*100+(inputBuffer[9]-'0')*10+(inputBuffer[10]-'0');
                  doorEasingMethod = 0;
                  cVarSpeedMin = 0;
                  cVarSpeedMax = 0;
                } else if (inputBuffer[5] == 'E' ||inputBuffer[5] == 'e'){
                  Debug.DBG("with Easing \n");
                  doorEasingMethod = (inputBuffer[6]-'0')*10+(inputBuffer[7]-'0');
                  if (commandLength >= 13){
                    Debug.DBG("Variable Speed Selected\n");
                    cVarSpeedMin = (inputBuffer[8]-'0')*1000+(inputBuffer[9]-'0')*100+(inputBuffer[10]-'0')*10+(inputBuffer[11]-'0');                
                    cVarSpeedMax = (inputBuffer[12]-'0')*1000+(inputBuffer[13]-'0')*100+(inputBuffer[14]-'0')*10+(inputBuffer[15]-'0');  
                  } else {
                    Debug.DBG("No Variable Speed selected\n");
                    cVarSpeedMin = (inputBuffer[8]-'0')*1000+(inputBuffer[9]-'0')*100+(inputBuffer[10]-'0')*10+(inputBuffer[11]-'0');                
                    cVarSpeedMax = cVarSpeedMin; 
                  }              
                  delayCallTime = 0;
                } else if (inputBuffer[5] == 'B' || inputBuffer[5] == 'b'){
                  Debug.DBG("Both Easing and Delay Call \n");
                  doorEasingMethod = (inputBuffer[6]-'0')*10+(inputBuffer[7]-'0');
                  if (commandLength >= 17){
                    cVarSpeedMin = (inputBuffer[8]-'0')*1000+(inputBuffer[9]-'0')*100+(inputBuffer[10]-'0')*10+(inputBuffer[11]-'0');                
                    cVarSpeedMax = (inputBuffer[12]-'0')*1000+(inputBuffer[13]-'0')*100+(inputBuffer[14]-'0')*10+(inputBuffer[15]-'0');
                    delayCallTime =  (inputBuffer[16]-'0')*10000+(inputBuffer[17]-'0')*1000+(inputBuffer[18]-'0')*100+(inputBuffer[19]-'0')*10+(inputBuffer[20]-'0');
                  } else {
                    cVarSpeedMin = (inputBuffer[8]-'0')*1000+(inputBuffer[9]-'0')*100+(inputBuffer[10]-'0')*10+(inputBuffer[11]-'0');                
                    cVarSpeedMax = cVarSpeedMin;
                    delayCallTime =  (inputBuffer[12]-'0')*10000+(inputBuffer[13]-'0')*1000+(inputBuffer[14]-'0')*100+(inputBuffer[15]-'0')*10+(inputBuffer[16]-'0');
                  }
                }else{
                  Debug.DBG("No easing or DelayCall time specified \n");
                  delayCallTime = 0;
                  doorEasingMethod = 0;
                  cVarSpeedMin = 0;
                  cVarSpeedMax = 0;
                }
              }
            }                                    
                if(inputBuffer[1]=='E' || inputBuffer[1]=='e') {
                  String  ESPNOWStringCommand = "";
                  String ESPNOWSubStringCommand = "";
                  String ESPNOWTarget = "";                  for (int i=2; i<=commandLength; i++){
                    char inCharRead = inputBuffer[i];
                    ESPNOWStringCommand += inCharRead;                   // add it to the inputString:
                  }
                  Debug.LOOP("\nFull Command Recieved: %s \n",ESPNOWStringCommand.c_str());
                  ESPNOWTarget = ESPNOWStringCommand.substring(0,2);
                  Debug.LOOP("ESP NOW Target: %s\n", ESPNOWTarget.c_str());
                  ESPNOWSubStringCommand = ESPNOWStringCommand.substring(2,commandLength+1);
                  Debug.LOOP("Command to Forward: %s\n", ESPNOWSubStringCommand.c_str());
                  sendESPNOWCommand(ESPNOWTarget, ESPNOWSubStringCommand);

                  }  
                if(inputBuffer[1]=='S' || inputBuffer[1]=='s') {
                    for (int i=2; i<commandLength-1;i++ ){
                      char inCharRead = inputBuffer[i];
                      serialStringCommand += inCharRead;  // add it to the inputString:
                    }
                    serialPort = serialStringCommand.substring(0,2);
                    serialSubStringCommand = serialStringCommand.substring(2,commandLength);
                    Debug.LOOP("Serial Command: %s to Serial Port: %s\n", serialSubStringCommand.c_str(), serialPort);                
              if (serialPort == "HP"){
                writeHpSerial(serialStringCommand);
              } else if (serialPort == "RS"){
                writeRsSerial(serialStringCommand);
              }  else if (serialPort == "FU"){
                writeFuSerial(serialStringCommand);
              } else if (serialPort == "DS"){
                inputString = serialStringCommand;
                stringComplete = true; 
              }
              serialStringCommand = "";
              serialPort = "";
            }   
            if(inputBuffer[1]=='R' || inputBuffer[1]=='r') {
              ledFunction = (inputBuffer[2]-'0')*10+(inputBuffer[3]-'0');
              colorState1 = (inputBuffer[4]-'0');
              speedState = (inputBuffer[5]-'0');
              }              



            if(inputBuffer[1]=='D' || inputBuffer[1]=='d') {
              D_command[0]   = '\0';                                                            // Flushes Array
              D_command[0] = doorFunction;
              D_command[1] = doorBoard;
                if(door>=0) {D_command[2] = door;}
              D_command[3] = doorEasingMethod;
              D_command[4] = cVarSpeedMin;
              D_command[5] = cVarSpeedMax;
              D_command[6] = delayCallTime;

              Debug.DBG("Door Function Called: %d\n",doorFunction);
              Debug.DBG("Easing Method: %d \n",doorEasingMethod);
              Debug.DBG("VarSpeed - Min:%d, Max:%d \n",cVarSpeedMin, cVarSpeedMax);
              Debug.DBG("DelayCall Duration: %d\n",delayCallTime);
            }

            if(inputBuffer[1]=='R' || inputBuffer[1]=='r'){
              RE_command[0]   = '\0';                                                            // Flushes Array
              RE_command[0] = ledFunction;
              RE_command[1] = colorState1;
              RE_command[2] = speedState;
              if(!autoComplete) {enableRadarEyeAuto = 0; }                                            //  Disables Automode to keep it from overriding User selected commands
              Debug.DBG(" LED Function:%d, ColorState:%d, Color(Dec):%d, Speed:%d\n",ledFunction, colorState1, basicColors[colorState1], speedState);
              Debug.DBG(" LED Function:%d, ColorState:%d, Color(Dec):%d, Speed:%d\n",RE_command[0], RE_command[1], basicColors[RE_command[1]], RE_command[2]);
            }
                
          }
        }
        }
      ///***  Clear States and Reset for next command.  ***///
        stringComplete =false;
        autoComplete = false;
        inputBuffer[0] = '\0';
        inputBuffer[1] = '\0';

        // reset Local ESP Command Variables

        // reset Camera Variables
        int ledFunction;
        int speedState;
        int colorState1;
        
        // reset Door Variables
        int door = -1;
        int doorFunction;
        int doorBoard;
        int doorEasingMethod;
        uint32_t cVarSpeedMin;
        uint32_t cVarSpeedMax;
        uint32_t delayCallTime;

        // reset ESP-NOW Variables

    
        Debug.DBG("command taken\n");
  }



    if(D_command[0]) {
      if((D_command[0] == 1 || D_command[0] == 2) && D_command[1] >= 11) {
//        Debug.DBG("Incorrect Door Value Specified, Command Aborted!");
        D_command[0] = '\0';
      }
      else {
        switch (D_command[0]) {
          case 1: openDoor(D_command[1],D_command[2],D_command[3],D_command[4],D_command[5]);           break;
          case 2: closeDoor(D_command[1],D_command[2],D_command[3],D_command[4],D_command[5]);          break;
          case 3: openAllDoors(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);       break;
          case 4: closeAllDoors(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);      break;
          case 5: shortCircuit(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);                    break;
          case 6: allOpenClose(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);                    break;
          case 7: allOpenCloseLong(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);                break;
          case 8: allFlutter(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);                      break;
          case 9: allOpenCloseRepeat(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);              break;
          case 10: panelWave(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);         break;
          case 11: panelWaveFast(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);     break;
          case 12: openCloseWave(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);     break;
          case 13: marchingAnts(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);                   break;
          case 14: panelAlternate(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);                 break;
          case 15: panelDance(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);                     break;
          case 16: longDisco(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);                      break;
          case 17: longHarlemShake(D_command[1],D_command[3],D_command[4],D_command[5],D_command[6]);                break;
          case 98: closeAllDoors(2,0,0,0,0);                                                              break;
          case 99: closeAllDoors(2,0,0,0,0);                                                              break;
          default: break;
        }
      }
    }

    if(RE_command[0]){
      switch(RE_command[0]){
        case 1: RadarEye_LED(basicColors[RE_command[1]], RE_command[2]);                                   break;
        case 2: break;  //reserved for future use
        case 3: break;  //reserved for future use
        case 96: enableRadarEyeAuto = 0;                                                                      break;     // Disable Auto Mode
        case 97: enableRadarEyeAuto = 1;                                                                      break;     // Enables Auto Mode
        case 98:  RE_command[0] = '\0'; 
                  clearRE();
                  enableRadarEyeAuto = 0;                                                                     break;
        case 99:  RE_command[0] = '\0'; 
                  clearRE();
                  enableRadarEyeAuto = 1;                                                                     break;
      }
    }


      if(!stringComplete && inputString) {
          if(enableRadarEyeAuto == 1) {REAuto();}
      
        }
    if(isStartUp) {
      isStartUp = false;
      delay(500);
    }
  }
}  //end of main loop
