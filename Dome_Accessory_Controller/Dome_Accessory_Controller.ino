///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///*****                                                                                                        *****///
///*****                                Created by Greg Hulette.  I                                             *****///
///*****   I started with the code from flthymcnsty from from which I used the basic command structure and      *****///
///*****  serial input method.  This code also relies on the ReelTwo library for all it's servo movements.     *****///
///*****                                                                                                       *****///
///*****                                     So exactly what does this all do.....?                            *****///
///*****                       - Receives commands via Serial or ESP-NOW                                       *****///
///*****                       - Controls the body servos                                                      *****///
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

//Used for the Status LED
#include <Adafruit_NeoPixel.h>
#include <Adafruit_DotStar.h>                  // Source: https://github.com/adafruit/Adafruit_DotStar

//Used for pin definition
#include "dome_accessory_controller_pin_map.h"

// Debug Functions  - Using my own library for this
#include <DebugR2.h>  //  https://github.com/greghulette/Arduino-Code/tree/main/libraries/DebugR2  Put these files in a folder called "DebugR2" in your libraries folder and restart the IDE

//ReelTwo libaries - Using my forked version of this libarary
#include <ReelTwo.h>
#include "core/DelayCall.h"
#include "ServoDispatchPCA9685.h"
#include "ServoSequencer.h"

//Used for PC9685 - Servo Expansion Board
#include <Wire.h>





//////////////////////////////////////////////////////////////////////
///*****          Preferences/Items to change                 *****///
//////////////////////////////////////////////////////////////////////
 // ESPNOW Password - This must be the same across all devices
  String ESPNOWPASSWORD = "GregsAstromech";

  // R2 Control Network Details for OTA
  const char* ssid = "R2D2_Control_Network";
  const char* password =  "astromech";

  //Enables status tracking on the LoRa Droid
  bool STATUS_TRACKING = 1;
  
    // Keepalive timer to send status messages to the Kill Switch (Droid)
  int keepAliveDuration= 4000;  // 4 seconds

// used to sync timing with the dome controller better, allowing time for the ESP-NOW messages to travel to the peers
// Change this to work with how your droid performs
  int defaultESPNOWSendDuration = 50;  

  // Serial Baud Rates
  #define SERIAL1_BAUD_RATE 115200







//////////////////////////////////////////////////////////////////////
///*****        Command Varaiables, Containers & Flags        *****///
//////////////////////////////////////////////////////////////////////
  String HOSTNAME = "Dome Accessory Controller";
  
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
  int localCommandFunction = 0;

  String ESPNOWStringCommand;
  String ESPNOWTarget;
  String ESPNOWSubStringCommand;

  debugClass Debug;
  String debugInputIdentifier ="";

  
  
  
  
  
  
  
  //////////////////////////////////////////////////////////////////////
  ///*****       Startup and Loop Variables                     *****///
  //////////////////////////////////////////////////////////////////////
  
  boolean startUp = true;
  boolean isStartUp = true;
  
  //Main Loop Timers
  unsigned long mainLoopTime;
  unsigned long MLMillis;
  byte mainLoopDelayVar = 5;


///////////////////////////////////////////////////////////////////////
  ///*****                Status Variables                     *****///
  /////////////////////////////////////////////////////////////////////

  unsigned long keepAliveMillis;

  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  //////////////////////////////////////////////////////////////////
  ///******       Serial Ports Definitions                  *****///
  //////////////////////////////////////////////////////////////////

  #define s1Serial Serial2







//////////////////////////////////////////////////////////////////////
  ///******             WiFi Specific Setup                     *****///
  //////////////////////////////////////////////////////////////////////

//Droid Remote ESP          192.168.4.101   
//Droid Gateway ESP         192.168.4.108   (Only used for OTA, Remote LoRa ESP must be on and close to Droid)
//Body Controller ESP       192.168.4.109   (Only used for OTA, Remote LoRa ESP must be on and close to Droid)
//Body Servo ESP            192.168.4.110   (Only used for OTA, Remote LoRa ESP must be on and close to Droid)
//Dome Controller ESP       192.168.4.111   (Only used for OTA, Remote LoRa ESP must be on and close to Droid)
//Dome Plate Controller ESP 192.168.4.112   (Only used for OTA, Remote LoRa ESP must be on and close to Droid)
//HP Controller ESP         192.168.4.113   (Only used for OTA, Remote LoRa ESP must be on and close to Droid)
//Dome Logics               192.168.4.114   (Only used for OTA, Remote LoRa ESP must be on and close to Droid)
//Dome Accessory ESP        192.168.4.115   (Only used for OTA, Remote LoRa ESP must be on and close to Droid)

  
  // IP Address config of local ESP
  IPAddress local_IP(192,168,4,115);
  IPAddress subnet(255,255,255,0);
  IPAddress gateway(192,168,4,101);
  
  uint8_t oldLocalMACAddress[] = {0x24, 0x0A, 0xC4, 0xED, 0x30, 0x13};    //used when connecting to WiFi for OTA

  AsyncWebServer server(80);
  

//////////////////////////////////////////////////////////////////////
///*****            Status LED Variables and settings       *****///
//////////////////////////////////////////////////////////////////////
  
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

  #define STROBE_LED_COUNT 24
  byte strobe_bright = 255;

  #define STATUS_LED_COUNT 1

  Adafruit_NeoPixel ESP_LED = Adafruit_NeoPixel(STATUS_LED_COUNT, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);
  Adafruit_DotStar strobe_LED = Adafruit_DotStar(STROBE_LED_COUNT, STROBE_DATA, STROBE_CLOCK, DOTSTAR_RBG);



int displayState;
int colorState1;
int colorState2;
int typeState;


byte Strobe_command[4]  = {0,0,0,0};



    byte defaultPrimaryColorInt     = 5;          //1 Integer color value from list above
    byte defaultSecondaryColorInt   = 1;          //5 Integer color value from list above


  unsigned long StrobeMillis;
   unsigned long SCruntimeStrobe;


byte StrobeFrame;

long int StrobeCount =0;
























































/////////////////////////////////////////////////////////////////////////
///*****                  ESP NOW Set Up                         *****///
/////////////////////////////////////////////////////////////////////////

//  ESP-NOW MAC Addresses used in the Droid. 
  const uint8_t droidLoRaMACAddress[] =               {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
  const uint8_t bodyControllerMACAddress[] =          {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};
  const uint8_t bodyServosControllerMACAddress[] =    {0x02, 0x00, 0x00, 0x00, 0x00, 0x03};
  const uint8_t domeControllerMACAddress[]=           {0x02, 0x00, 0x00, 0x00, 0x00, 0x04};
  const uint8_t domePlateControllerMACAddress[] =     {0x02, 0x00, 0x00, 0x00, 0x00, 0x05};
  const uint8_t hpControllerMACAddress[] =            {0x02, 0x00, 0x00, 0x00, 0x00, 0x06};
  const uint8_t domeLogicsControllerMACAddress[] =     {0x02, 0x00, 0x00, 0x00, 0x00, 0x07};
  const uint8_t domeAccessoryControllerMACAddress[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x08};
  const uint8_t broadcastMACAddress[] =               {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Uses these Strings for comparators
  String droidLoRaMACAddressString =                "02:00:00:00:00:01";
  String bodyControllerMACAddressString =           "02:00:00:00:00:02";
  String bodyServosControllerMACAddressString =     "02:00:00:00:00:03";
  String domeControllerMACAddressString =           "02:00:00:00:00:04";
  String domePlateControllerMACAddressString =      "02:00:00:00:00:05";
  String hpControllerMACAddressString =             "02:00:00:00:00:06";
  String domeLogicsControllerMACAddressString =      "02:00:00:00:00:06";
  String domeAccessoryControllerMACAddressString =  "02:00:00:00:00:06";
  String broadcastMACAddressString =                "FF:FF:FF:FF:FF:FF";

// Define variables to store commands to be sent
  String  senderID;
  String  targetID;
  bool    commandIncluded;
  String  command;

// Define variables to store incoming commands
  String  incomingPassword;
  String  incomingTargetID;  
  String  incomingSenderID;
  bool    incomingCommandIncluded;
  String  incomingCommand;
  












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
  espnow_struct_message commandsToSendtoHPController;
  espnow_struct_message commandsToSendtoDomeLogicsController;
  espnow_struct_message commandsToSendtoDomeAccessoryController;

// Create a espnow_struct_message to hold variables that will be received
  espnow_struct_message commandsToReceiveFromBroadcast;
  espnow_struct_message commandsToReceiveFromDroidLoRa;
  espnow_struct_message commandsToReceiveFromBodyController;
  espnow_struct_message commandsToReceiveFromBodyServoController;
  espnow_struct_message commandsToReceiveFromDomeController;
  espnow_struct_message commandsToReceiveFromDomePlateController;
  espnow_struct_message commandsToReceiveFromHPController;
  espnow_struct_message commandsToReceiveFromDomeLogicsController;
  espnow_struct_message commandsToReceiveFromDomeAccessoryController;

  esp_now_peer_info_t peerInfo;

//  // Callback when data is sent
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

  // Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  colorWipeStatus("ES", orange ,255);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  String IncomingMacAddress(macStr);
  if (IncomingMacAddress == droidLoRaMACAddressString) {
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
    }else if (IncomingMacAddress == bodyServosControllerMACAddressString) {
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
    } else if (IncomingMacAddress == domeControllerMACAddressString) {
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
    } else if (IncomingMacAddress == domePlateControllerMACAddressString) {
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
    } else if (IncomingMacAddress == broadcastMACAddressString) {
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
    }  else if (IncomingMacAddress == hpControllerMACAddressString) {
      memcpy(&commandsToReceiveFromHPController, incomingData, sizeof(commandsToReceiveFromHPController));
      incomingPassword = commandsToReceiveFromHPController.structPassword;
      if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password was sent.  Message Ignored\n");  
      } else {
        incomingSenderID = commandsToReceiveFromHPController.structSenderID;
        incomingTargetID = commandsToReceiveFromHPController.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromHPController.structCommandIncluded;
        incomingCommand = commandsToReceiveFromHPController.structCommand;
        processESPNOWIncomingMessage();
        }
    }  else if (IncomingMacAddress == domeLogicsControllerMACAddressString) {
      memcpy(&commandsToReceiveFromDomeLogicsController, incomingData, sizeof(commandsToReceiveFromDomeLogicsController));
      incomingPassword = commandsToReceiveFromDomeLogicsController.structPassword;
      if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password was sent.  Message Ignored\n");  
      } else {
        incomingSenderID = commandsToReceiveFromDomeLogicsController.structSenderID;
        incomingTargetID = commandsToReceiveFromDomeLogicsController.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromDomeLogicsController.structCommandIncluded;
        incomingCommand = commandsToReceiveFromDomeLogicsController.structCommand;
        processESPNOWIncomingMessage();
        }
    }  else if (IncomingMacAddress == domeAccessoryControllerMACAddressString) {
      memcpy(&commandsToReceiveFromDomeAccessoryController, incomingData, sizeof(commandsToReceiveFromDomeAccessoryController));
      incomingPassword = commandsToReceiveFromDomeAccessoryController.structPassword;
      if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password was sent.  Message Ignored\n");  
      } else {
        incomingSenderID = commandsToReceiveFromDomeAccessoryController.structSenderID;
        incomingTargetID = commandsToReceiveFromDomeAccessoryController.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromDomeAccessoryController.structCommandIncluded;
        incomingCommand = commandsToReceiveFromDomeAccessoryController.structCommand;
        processESPNOWIncomingMessage();
        }
    } else {Debug.ESPNOW("ESP-NOW Mesage ignored \n");}  
  colorWipeStatus("ES",blue,10);
  IncomingMacAddress ="";  
} 

void processESPNOWIncomingMessage(){
  Debug.ESPNOW("incoming target: %s\n", incomingTargetID.c_str());
  Debug.ESPNOW("incoming sender: %s\n", incomingSenderID.c_str());
  Debug.ESPNOW("incoming command included: %d\n", incomingCommandIncluded);
  Debug.ESPNOW("incoming command: %s\n", incomingCommand.c_str());
  if (incomingTargetID == "DA" || incomingTargetID == "BR"){
    inputString = incomingCommand;
    stringComplete = true; 
    Debug.ESPNOW("Recieved command from $s\n", incomingSenderID);

  }
}
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
  char stringToSend[25];
  uint32_t fVarSpeedMin;
  uint32_t fVarSpeedMax;

  /////////////////////////////////////////////////////////////////////////
///*****              ReelTwo Servo Set Up                       *****///
/////////////////////////////////////////////////////////////////////////

#define SABER_LAUNCHER        0x0001 //b000000000001
#define ALL_SERVOS_MASK       (SABER_LAUNCHER)

// Group ID is used by the ServoSequencer and some ServoDispatch functions to
// identify a group of servos.

//     Pin,  Close Pos, Open Pos,  Group ID  (Change the Close and Open to your Droids actual limits)
const ServoSettings servoSettings[] PROGMEM = {
    { 1,  2250, 1550, SABER_LAUNCHER }       /* 0: Top Utility Arm 2350,675*/
    // { 2,  1630, 860, BOTTOM_UTILITY_ARM },    /* 1: Bottom Utility Arm 1950,960*/
    // { 3,  1820, 1000, LARGE_LEFT_DOOR },      /* 2: Right Left Door as viewing from looking at R2 1900,1000*/
    // { 4,  1400, 1900, LARGE_RIGHT_DOOR },      /* 3: Left Right door as viewing from looking at R2 1200,1900*/
    // { 5,  1590 , 758, CHARGE_BAY_DOOR },       /* 4: Charge Bay Inidicator Door 1900,758*/
    // { 6,  780, 1400, DATA_PANEL_DOOR },      /* 5: Data Panel Door 700,1400*/
    // { 7,  1950, 700, DRAWER_S1 },             /* 5: Data Panel Door 2050,700*/
    // { 8,  2245, 700, DRAWER_S2 },             /* 5: Data Panel Door 2345, 700*/
    // { 9,  650, 2300, DRAWER_S3 },             /* 5: Data Panel Door 550,2300*/
    // { 10,  1300, 2500, DRAWER_S4 },            /* 5: Data Panel Door 1200,2500*/
    // { 11,  1500, 1549, REAR_LEFT_DOOR },      /* 5: Data Panel Door */
    // { 12,  1500, 1549, REAR_RIGHT_DOOR }      /* 5: Data Panel Door */
  };

ServoDispatchPCA9685<SizeOfArray(servoSettings)> servoDispatch(servoSettings);
ServoSequencer servoSequencer(servoDispatch);


/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////                                                                                       /////////     
/////////                             Start OF FUNCTIONS                                        /////////
/////////                                                                                       /////////     
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
///*****   ColorWipe Function for Status LED                  *****///
//////////////////////////////////////////////////////////////////////
void colorWipeStatus(String statusled, uint32_t c, int brightness) {
  if(statusled == "ES"){
    ESP_LED.setBrightness(brightness);
    ESP_LED.setPixelColor(0, c);
    ESP_LED.show();
  } 
  else{Debug.DBG("No LED was chosen \n");}
};

//////////////////////////////////////////////////////////////////////
///*****    Send Keepalive Messages for Status                *****///
//////////////////////////////////////////////////////////////////////

void keepAlive(){
  if (STATUS_TRACKING == 1){
    if (millis() - keepAliveMillis >= (keepAliveDuration + random(1, 1000))){
    keepAliveMillis = millis();
    sendESPNOWCommand("DG","");  
    } 
  }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////                                                                                               /////
/////                              Communication Functions                                    /////
/////                                                                                               /////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/*//////////////////////////////////////////////////////////////////////
Turns on Wifi and enables OTA.  It will connect to the Kill Switch Remotes's
WiFi network to allow the uploading of sktches(.bin files) via the OTA process. 
It does not produce it's own WiFi network.  Once enables, a reboot is
required to regain ESP-NOW functionality.
*///////////////////////////////////////////////////////////////////////
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
    request->send(200, "text/plain", "Please go to http://192.168.4.112/update to upload file");
  });
  
  AsyncElegantOTA.begin(&server);    // Start AsyncElegantOTA
  server.begin();

  Local_Command[0]   = '\0';
};

/////////////////////////////////////////////////////////
///*****          Serial Event Function          *****///
/////////////////////////////////////////////////////////                                  

void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  Debug.SERIAL_EVENT("USB Serial Input: %s \n",inputString);
};

void s1SerialEvent() {
  while (s1Serial.available()) {
    char inChar = (char)s1Serial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  Debug.SERIAL_EVENT("Serial 1 Input: %s \n",inputString);
};


//      /////////////////////////////////////////////////////////
//      ///*****          Serial Write Function          *****///
//      /////////////////////////////////////////////////////////

void writeSerialString(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    Serial.write(completeString[i]);
  }
};

void writes1Serial(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    s1Serial.write(completeString[i]);
  }
    Debug.SERIAL_EVENT("Writing to Serial 1\n");

};


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
};

void sendESPNOWCommand(String starget, String scomm){
  String senderID = "DA";   // change to match location (BC/BS/DC/DP/LD)
  String scommEval = "";
  bool hasCommand;
  if (scommEval == scomm){
    hasCommand = 0;
  } else {hasCommand = 1;};

  if (starget == "DG"){
    setupSendStruct(&commandsToSendtoDroidLoRa, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(droidLoRaMACAddress, (uint8_t *) &commandsToSendtoDroidLoRa, sizeof(commandsToSendtoDroidLoRa));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW LoRa Droid Neighbor\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data\n");}
  } else if (starget == "BC"){
    setupSendStruct(&commandsToSendtoBodyController, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(bodyControllerMACAddress, (uint8_t *) &commandsToSendtoBodyController, sizeof(commandsToSendtoBodyController));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW Neighbor \n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data\n");}
  } else if (starget == "BS"){
    setupSendStruct(&commandsToSendtoBodyServoController, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
       esp_err_t result = esp_now_send(bodyServosControllerMACAddress, (uint8_t *) &commandsToSendtoBodyServoController, sizeof(commandsToSendtoBodyServoController));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data\n");}
  }  else if (starget == "DC"){
    setupSendStruct(&commandsToSendtoDomeController, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
       esp_err_t result = esp_now_send(domeControllerMACAddress, (uint8_t *) &commandsToSendtoDomeController, sizeof(commandsToSendtoDomeController));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data\n");}
  } else if (starget == "DP"){
    setupSendStruct(&commandsToSendtoDomePlateController, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
       esp_err_t result = esp_now_send(domePlateControllerMACAddress, (uint8_t *) &commandsToSendtoDomePlateController, sizeof(commandsToSendtoDomePlateController));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data\n");}
  } else if (starget == "BR"){
    setupSendStruct(&commandsToSendtoBroadcast, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
       esp_err_t result = esp_now_send(broadcastMACAddress, (uint8_t *) &commandsToSendtoBroadcast, sizeof(commandsToSendtoBroadcast));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data\n");}
  }  else if (starget == "HP"){
    setupSendStruct(&commandsToSendtoHPController, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
       esp_err_t result = esp_now_send(hpControllerMACAddress, (uint8_t *) &commandsToSendtoHPController, sizeof(commandsToSendtoHPController));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data\n");}
  }else if (starget == "DL"){
    setupSendStruct(&commandsToSendtoDomeLogicsController, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
       esp_err_t result = esp_now_send(domeLogicsControllerMACAddress, (uint8_t *) &commandsToSendtoDomeLogicsController, sizeof(commandsToSendtoDomeLogicsController));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data\n");}
  }else if (starget == "DA"){
    setupSendStruct(&commandsToSendtoDomeAccessoryController, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
       esp_err_t result = esp_now_send(domeAccessoryControllerMACAddress, (uint8_t *) &commandsToSendtoDomeAccessoryController, sizeof(commandsToSendtoDomeAccessoryController));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data\n");}
  } else {Debug.ESPNOW("No valid destination \n");}
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
void launchSaber(){
  Debug.SERVO("Launched Light Saber\n");   
    fVarSpeedMin = varSpeedMin;                                                               // sets Global Variable from the local variable to allow the lambda function to utilize it
  fVarSpeedMax = varSpeedMax;    
  // sendESPNOWCommand("DC", "D20108");      
  // DelayCall::schedule([]{SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllOpen, SABER_LAUNCHER, varSpeedMin, varSpeedMax);}, 4000);
  SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, SABER_LAUNCHER);

    D_command[0]   = '\0';

}
void armSaber(){
  Debug.SERVO("Armed Light Saber\n");            
  // DelayCall::schedule([]{SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllClose, SABER_LAUNCHER, varSpeedMin, varSpeedMax);}, 7000);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, SABER_LAUNCHER);

    D_command[0]   = '\0';

}

void fansOn(){
  digitalWrite(FANS, HIGH);
      D_command[0]   = '\0';
};

void fansOff(){
  digitalWrite(FANS, LOW);
      D_command[0]   = '\0';
};

void smokeOn(){
  digitalWrite(SMOKE, HIGH);
  D_command[0]   = '\0';
};

void smokeOff(){
  digitalWrite(SMOKE, LOW);
  D_command[0]   = '\0';
};

void smokeSequece(){
  Debug.DBG("Smoke Sequence Initialized\n");
  fansOn();
  inputString = ":D108"; stringComplete = true;
  // ShortCircuitStrobe(1, red, blue);
  smokeOn();
  DelayCall::schedule([]{fansOff();}, 14000);
  DelayCall::schedule([]{clearStrobe();}, 14000);
  DelayCall::schedule([]{smokeOff();}, 14000);
  Debug.DBG("Ended Sequence");
}

void showStrobe() {
        strobe_LED.show();
}
 
void solidStrobe(uint32_t c) {
    for(int i=0;i<STROBE_LED_COUNT;i++) { strobe_LED.setPixelColor(i,c); }
    strobe_LED.show();
};

void altColorsStrobe(int type, uint32_t color1,uint32_t color2) {
  uint32_t c;
  int interval;
  if(type<1) {interval = 100;}
  else {interval = 100*type;}
  if(StrobeFrame>1) {StrobeFrame=0;}
  if((millis() - StrobeMillis) > interval) {
      if(StrobeFrame==1) {c=color2;}
      else {c=color1;}
      StrobeFrame++;
      StrobeMillis=millis();
      for(int i=0;i<STROBE_LED_COUNT;i++) {
      strobe_LED.setPixelColor(i,c);
      }
      showStrobe();
  }
}

  void ShortCircuitStrobe(byte type, uint32_t color1, uint32_t color2) {
    if(StrobeCount==0) {SCruntimeStrobe = millis();}
    long runelapsed = millis() - SCruntimeStrobe;
      uint32_t type2Colors[2] = {color1, color2};
      int pixels[STROBE_LED_COUNT];
      for (int i=0;i < STROBE_LED_COUNT; i++) {pixels[i] = i;}
      randomize(pixels,STROBE_LED_COUNT);
      int interval = 10000/STROBE_LED_COUNT;
      long elapsed = millis() - StrobeMillis;
      if(StrobeFrame<STROBE_LED_COUNT) {
        if(elapsed>=interval) {StrobeFrame++;StrobeMillis=millis();}
        for (int i=0;i < STROBE_LED_COUNT; i++) {strobe_LED.setPixelColor(i,off);}
        for (int i=0;i < STROBE_LED_COUNT-StrobeFrame; i++) {
          if(type == 2) {strobe_LED.setPixelColor(pixels[i],random(255),random(255),random(255));}
          else {strobe_LED.setPixelColor(pixels[i],type2Colors[random(2)]);}
        }
      StrobeCount++;
      if(runelapsed>=1+StrobeCount) {showStrobe();SCruntimeStrobe = millis();}
      }
  }

      void clearStrobe() {
        // if(StrobeFrame>0) {StrobeFrame=1;}
        // if(StrobeFrame==0) {
          for(int i=0;i<STROBE_LED_COUNT;i++) {
            strobe_LED.setPixelColor(i,off);
          }
          // StrobeFrame++;
          showStrobe();
        // }
      }


  void swap (int *a, int *b)
  {
      int temp = *a;
      *a = *b;
      *b = temp;
  }

  void randomize ( int arr[], int n )
    {
        // Use a different seed value so that we don't get same
        // result each time we run this program
        srand(millis());

        // Start from the last element and swap one by one. We don't
        // need to run for the first element that's why i > 0
        for (int i = n-1; i > 0; i--)
        {
            // Pick a random index from 0 to i
            int j = rand() % (i+1);

            // Swap arr[i] with the element at random index
          swap(&arr[i], &arr[j]);
        }
    };

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                               /////
///////                             Miscellaneous Functions                                           /////
///////                                                                                               /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////
///*****          Scan I2C for devices                        *****///
//////////////////////////////////////////////////////////////////////

void scan_i2c()
{
    unsigned nDevices = 0;
    for (byte address = 1; address < 127; address++)
    {
        String name = "<unknown>";
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
        if (address == 0x70)
        {
            // All call address for PCA9685
            name = "PCA9685:all";
        }
        if (address == 0x40)
        {
            // Adafruit PCA9685
            name = "PCA9685";
        }
        if (address == 0x14)
        {
            // IA-Parts magic panel
            name = "IA-Parts Magic Panel";
        }
        if (address == 0x20)
        {
            // IA-Parts periscope
            name = "IA-Parts Periscope";
        }
        if (address == 0x16)
        {
            // PSIPro
            name = "PSIPro";
        }

        if (error == 0)
        {
            Serial.print("I2C device found at address 0x");
            if (address < 16)
                Serial.print("0");
            Serial.print(address, HEX);
            Serial.print(" ");
            Serial.println(name);
            nDevices++;
        }
        else if (error == 4)
        {
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


/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////                                                                                       /////////     
/////////                             END OF FUNCTIONS                                          /////////
/////////                                                                                       /////////     
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////


void setup(){
  //Initialize the Serial Ports
  Serial.begin(115200);
  s1Serial.begin(SERIAL1_BAUD_RATE, SERIAL_8N1, SERIAL1_RX_PIN, SERIAL1_TX_PIN);
  
  delay(500);
  
  Serial.println("\n\n----------------------------------------");
  Serial.print("Booting up the ");Serial.println(HOSTNAME);
  Serial.println("----------------------------------------");

pinMode(FANS, OUTPUT);
pinMode(SMOKE, OUTPUT);

  //Initialize I2C for the Servo Expander Board
  Wire.begin();
  
  //Initialize the ReelTwo Library
  SetupEvent::ready();
  
  //Reserve the inputStrings
  inputString.reserve(100);                                                              // Reserve 100 bytes for the inputString:
  autoInputString.reserve(100);

  //initialize WiFi for ESP-NOW
  WiFi.mode(WIFI_STA);
  esp_wifi_set_mac(WIFI_IF_STA, &bodyServosControllerMACAddress[0]);
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

  // Register peer configuration
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

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
  //  // Body Servo Controller
  // memcpy(peerInfo.peer_addr, bodyServosControllerMACAddress, 6);
  // if (esp_now_add_peer(&peerInfo) != ESP_OK){
  //   Serial.println("Failed to add Broadcast ESP-NOW peer");
  //   return;
  // }
// Dome Controller
  memcpy(peerInfo.peer_addr, domeControllerMACAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add Broadcast ESP-NOW peer");
    return;
  } 
// Dome Plate Controller
  memcpy(peerInfo.peer_addr, domePlateControllerMACAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add Broadcast ESP-NOW peer");
    return;
  }
// HP Controller
  memcpy(peerInfo.peer_addr, hpControllerMACAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add Broadcast ESP-NOW peer");
    return;
  }

  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
  ESP_LED.begin();
  ESP_LED.show();
  colorWipeStatus("ES",red,10);

  strobe_LED.begin();
  strobe_LED.show();

  
}   // end of setup

void loop(){

  if (millis() - MLMillis >= mainLoopDelayVar){
      MLMillis = millis();
      AnimatedEvent::process();

    if(startUp) {
      // writeBcSerial("SMPt003");
      // closeAllDoors(1,0,0,0,0);
      startUp = false;
      Serial.print("Startup complete\nStarting main loop\n\n\n");
  colorWipeStatus("ES",blue,10);

    }  
    keepAlive();
    if(Serial.available()){serialEvent();}
    // if(bcSerial.available()){bcSerialEvent();}
    if(s1Serial.available()){s1SerialEvent();}

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
                        //  DelayCall::schedule([] {ESP.restart();}, 3000);
                        ESP.restart();
                        Local_Command[0]   = '\0';                                                           break;
                  case 3: connectWiFi();                                                                          break;
                  case 4: ; break;  //reserved for future use
                  case 5: ; break;  //reserved for future use
                  case 6: ; break;  //reserved for future use
                  case 7: ; break;  //reserved for future use
                  case 8: ; break;  //reserved for future use                                                         break;  //reserved for future use
                  case 9:  ;break;  //reserved for future use

                }
              }

        }else if (inputBuffer[0] == ':'){
       if(  inputBuffer[1]=='D' ||        // Door Designator
            inputBuffer[1]=='d' ||        // Door Designator
            inputBuffer[1]=='E' ||        // Command designator for ESP-NOW functions
            inputBuffer[1]=='E' ||        // Command designator for ESP-NOW functions
            inputBuffer[1]=='N' ||        // Command for Sending ESP-NOW Messages
            inputBuffer[1]=='n' ||        // Command for Sending ESP-NOW Messages
            inputBuffer[1]=='S' ||        // Command for sending Serial Strings out Serial ports
            inputBuffer[1]=='s'           // Command for sending Serial Strings out Serial ports


         ){commandLength = strlen(inputBuffer);                     //  Determines length of command character array.
            Debug.LOOP("Command: %s with a length of %d \n", inputBuffer, commandLength);
            if(commandLength >= 3) {
              if(inputBuffer[1]=='D' || inputBuffer[1]=='d') {                                                            // specifies the overall door command
              doorBoard = inputBuffer[2]-'0';                                                                           // Sets the board to call the command on.
              doorFunction = (inputBuffer[3]-'0')*10+(inputBuffer[4]-'0');                                              // Sets the door command to a specific function
              if (doorFunction == 1 || doorFunction == 2){                                                              // Checks the door command to see if it's calling to open a single door
                door = (inputBuffer[5]-'0')*10+(inputBuffer[6]-'0');                                                    // Sets the specific door to move
                if (inputBuffer[7] == 'D' || inputBuffer[7] == 'd'){
                  Debug.LOOP("with DelayCall \n");
                  delayCallTime =  (inputBuffer[8]-'0')*10000+(inputBuffer[9]-'0')*1000+(inputBuffer[10]-'0')*100+(inputBuffer[11]-'0')*10+(inputBuffer[12]-'0');  // converts 5 digit character to uint32_t
                  doorEasingMethod = 0;                                                                                                                           // Sets Easing Method to 0-Off
                  cVarSpeedMin = 0;
                  cVarSpeedMax = 0;                                                                                                                        // Sets Easing duration to 0-Off
                } else if (inputBuffer[7] == 'E' ||inputBuffer[7] == 'e'){
                  Debug.LOOP("with Easing \n");
                  doorEasingMethod = (inputBuffer[8]-'0')*10+(inputBuffer[9]-'0');
                  doorEasingDuration = (inputBuffer[10]-'0')*1000+(inputBuffer[11]-'0')*100+(inputBuffer[12]-'0')*10+(inputBuffer[13]-'0');                
                  delayCallTime = 0;
                } else if (inputBuffer[7] == 'B' || inputBuffer[7] == 'b'){
                  Debug.LOOP("with Both Easing and Delay Call \n");
                  doorEasingMethod = (inputBuffer[8]-'0')*10+(inputBuffer[9]-'0');
                  cVarSpeedMin = (inputBuffer[10]-'0')*1000+(inputBuffer[11]-'0')*100+(inputBuffer[12]-'0')*10+(inputBuffer[13]-'0');                
                  cVarSpeedMax = (inputBuffer[14]-'0')*1000+(inputBuffer[15]-'0')*100+(inputBuffer[16]-'0')*10+(inputBuffer[17]-'0');
                  delayCallTime =  (inputBuffer[18]-'0')*10000+(inputBuffer[19]-'0')*1000+(inputBuffer[20]-'0')*100+(inputBuffer[21]-'0')*10+(inputBuffer[22]-'0');
                }else{
                  Debug.LOOP("No easing or Delay time specified \n");
                  delayCallTime = 0;
                  doorEasingMethod = 0;
                  cVarSpeedMin = 0;
                  cVarSpeedMax = 0;                
                }
              }
              else if (doorFunction != 1 || doorFunction != 2) {
                Debug.LOOP("Other Door Function Called \n");
                if (inputBuffer[5] == 'D' || inputBuffer[5] == 'd'){
                  Debug.LOOP("with DelayCall \n");
                  delayCallTime =  (inputBuffer[6]-'0')*10000+(inputBuffer[7]-'0')*1000+(inputBuffer[8]-'0')*100+(inputBuffer[9]-'0')*10+(inputBuffer[10]-'0');
                  doorEasingMethod = 0;
                  cVarSpeedMin = 0;
                  cVarSpeedMax = 0;
                } else if (inputBuffer[5] == 'E' ||inputBuffer[5] == 'e'){
                  Debug.LOOP("with Easing \n");
                  doorEasingMethod = (inputBuffer[6]-'0')*10+(inputBuffer[7]-'0');
                  if (commandLength >= 13){
                    Debug.LOOP("Variable Speed Selected\n");
                    cVarSpeedMin = (inputBuffer[8]-'0')*1000+(inputBuffer[9]-'0')*100+(inputBuffer[10]-'0')*10+(inputBuffer[11]-'0');                
                    cVarSpeedMax = (inputBuffer[12]-'0')*1000+(inputBuffer[13]-'0')*100+(inputBuffer[14]-'0')*10+(inputBuffer[15]-'0');  
                  } else {
                    Debug.LOOP("No Variable Speed selected\n");
                    cVarSpeedMin = (inputBuffer[8]-'0')*1000+(inputBuffer[9]-'0')*100+(inputBuffer[10]-'0')*10+(inputBuffer[11]-'0');                
                    cVarSpeedMax = cVarSpeedMin; 
                  }              
                  delayCallTime = 0;
                } else if (inputBuffer[5] == 'B' || inputBuffer[5] == 'b'){
                  Debug.LOOP("Both Easing and Delay Call \n");
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
                  Debug.LOOP("No easing or DelayCall time specified \n");
                  delayCallTime = 0;
                  doorEasingMethod = 0;
                  cVarSpeedMin = 0;
                  cVarSpeedMax = 0;
                }
              }
            }   

                if(inputBuffer[1]=='E' || inputBuffer[1]=='e') {
                  for (int i=2; i<=commandLength; i++){
                    char inCharRead = inputBuffer[i];
                    ESPNOWStringCommand += inCharRead;                   // add it to the inputString:
                  }
                  Debug.LOOP("\nFull Command Recieved: %s \n",ESPNOWStringCommand.c_str());
                  ESPNOWTarget = ESPNOWStringCommand.substring(0,2);
                  Debug.LOOP("ESP NOW Target: %s\n", ESPNOWTarget.c_str());
                  ESPNOWSubStringCommand = ESPNOWStringCommand.substring(2,commandLength+1);
                  Debug.LOOP("Command to Forward: %s\n", ESPNOWSubStringCommand.c_str());
                  sendESPNOWCommand(ESPNOWTarget, ESPNOWSubStringCommand);
                  // reset ESP-NOW Variables
                  ESPNOWStringCommand = "";
                  ESPNOWSubStringCommand = "";
                  ESPNOWTarget = "";                  
                  } 
              if(inputBuffer[0]=='S' || inputBuffer[0]=='s') {
                for (int i=1; i<commandLength-1;i++ ){
                  char inCharRead = inputBuffer[i];
                  serialStringCommand += inCharRead;  // add it to the inputString:
                }
                serialPort = serialStringCommand.substring(0,2);
                serialSubStringCommand = serialStringCommand.substring(2,commandLength);
                Debug.LOOP("Serial Command: %s to Serial Port: %s\n", serialSubStringCommand.c_str(), serialPort);                
                if (serialPort == "BC"){
                  // writeBcSerial(serialSubStringCommand);
                } else if (serialPort == "FU"){
                  writes1Serial(serialSubStringCommand);
                } 
                
                serialStringCommand = "";
                serialSubStringCommand = "";
                serialPort = "";
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

              Debug.LOOP("Door Function Called: %d\n",doorFunction);
              Debug.LOOP("Easing Method: %d \n",doorEasingMethod);
              Debug.LOOP("VarSpeed - Min:%d, Max:%d \n",cVarSpeedMin, cVarSpeedMax);
              Debug.LOOP("DelayCall Duration: %d\n",delayCallTime);
            }


            }
          }
        }

        
          if(D_command[0]) {
            if((D_command[0] == 1 || D_command[0] == 2) && D_command[1] >= 11) {
              Debug.LOOP("Incorrect Door Value Specified, Command Aborted!");
              D_command[0] = '\0';
            }
            else {
              switch (D_command[0]) {
                case 3: launchSaber(); break;
                case 4: armSaber(); break;
                case 5: fansOn(); break;
                case 6: fansOff(); break;
                case 7: solidStrobe(blue); break;
                case 8: altColorsStrobe(1, red, white); break;
                case 9: ShortCircuitStrobe(1, red, blue); break;
                case 14: smokeSequece(); break;
                case 98: clearStrobe();break;
                default: break;
              }
            }
          }
      ///***  Clear States and Reset for next command.  ***///
        stringComplete =false;
        autoComplete = false;
        inputBuffer[0] = '\0';
        
        // reset Local ESP Command Variables
        int localCommandFunction;


      // reset Door Variables
        int door = -1;
        int doorFunction;
        int doorBoard;
        int doorEasingMethod;
        uint32_t cVarSpeedMin;
        uint32_t cVarSpeedMax;
        uint32_t delayCallTime; 

      Debug.LOOP("command Proccessed\n");

    }


    

    if(isStartUp) {
      isStartUp = false;
      delay(500);

    }
  }
}
