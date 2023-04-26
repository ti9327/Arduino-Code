
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///*****                                                                                                       *****///
///*****                            Created by Greg Hulette.                                                   *****///
///*****                                                                                                       *****///
///*****   I started with the code from flthymcnsty from from which I used the basic command structure and     *****///
///*****  serial input method.                                                                                 *****///
///*****                                                                                                       *****///
///*****                                     So exactly what does this all do.....?                            *****///
///*****                       - Receives commands via Serial or ESP-NOW                                       *****///
///*****                       - Sends Serial commands to the LED Controller(ATMEGA2560 onboard)               *****///
///*****                       - Sends Serial commands to the Stealth Controller                               *****///
///*****                       - Sends Serial commands to the HCR                                              *****///
///*****                       - Connects the 25 wire sliip ring to the dome plate controller                  *****///
///*****                       - Provides USB connectivity to the dome plate  and dome controllers             *****///
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

//Used for Status LEDs
#include <Adafruit_NeoPixel.h>

//Used for pin definition
#include "body_controller_esp32_pin_map.h"

// Debug Functions  - Using my own library for this
#include <DebugR2.h>

//ReelTwo libaries - Using my forked version of this libarary
#include <ReelTwo.h>
#include "core/DelayCall.h"

// Used for Software Serial to allow more useful naming
#include <SoftwareSerial.h>

// Used to parse status from the ATMEGA2560 status messages
#include "ArduinoJson.h"




//////////////////////////////////////////////////////////////////////
///*****       Preferences/Items to change        *****///
//////////////////////////////////////////////////////////////////////
 //ESPNOW Password - This must be the same across all devices
  String ESPNOWPASSWORD = "GregsAstromech";  //Must be less than 20 characters

  ////R2 Control Network Details for OTA only
  const char* ssid = "R2D2_Control_Network";
  const char* password =  "astromech";

  // Keepalive timer to send status messages to the Kill Switch (Droid)
  int keepAliveDuration= 4000;  // 4 seconds

// used to sync timing with the dome controller better, allowing time for the ESP-NOW messages to travel to the dome
// Change this to work with how your droid performs
  int defaultESPNOWSendDuration = 50;

  // Serial Baud Rates
  #define BL_BAUD_RATE 9600
  #define RD_BAUD_RATE 115200 
  #define ST_BAUD_RATE 9600  //Should be lower than 57600
  #define MP_BAUD_RATE 9600  //Should be lower than 57600
  #define AUX1_BAUD_RATE 57600 //Should be lower than 57600
  #define AUX2_BAUD_RATE 57600  //Should be lower than 57600


//////////////////////////////////////////////////////////////
///*****        Command Varaiables, Containers & Flags        *****///
//////////////////////////////////////////////////////////////////////
  String HOSTNAME = "Body Controller"; 
  
  char inputBuffer[100];
  String inputString;    
       // a string to hold incoming data
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

  String LEDCommandString;

  int mp3Track;
  String mp3Comm;

  debugClass Debug;
String debugInputIdentifier ="";




  // variables for storing status and settings from ATMEGA2560
  int BL_LDP_Bright;
  int BL_MAINT_Bright;
  int BL_VU_Bright;
  int BL_CS_Bright;
  int BL_vuOffsetInt;
  int BL_vuBaselineInt;
  int BL_vuOffsetExt;
  int BL_vuBaselineExt;
  float BL_BatteryVoltage;
  int BL_BatteryPercentage;

  bool bodyLEDControllerStatus = 0;
  


  //////////////////////////////////////////////////////////////////////
  ///*****       Startup and Loop Variables                     *****///
  //////////////////////////////////////////////////////////////////////
  
  boolean startUp = true;
  boolean isStartUp = true;
  
  unsigned long mainLoopTime; // We keep track of the "Main Loop time" in this variable.
  unsigned long MLMillis;
  byte mainLoopDelayVar = 5;

 //Timers for Status updates
  unsigned long keepAliveMillisDuration = 15000;
  unsigned long blkeepAliveAge;
  unsigned long blkeepaliveAgeMillis;
  unsigned long keepAliveMillis;








  //////////////////////////////////////////////////////////////////////
  ///******       Serial Ports Specific Setup                   *****///
  //////////////////////////////////////////////////////////////////////

  
  #define rdSerial Serial1
  #define blSerial Serial2
  SoftwareSerial stSerial;
  SoftwareSerial mpSerial;
  SoftwareSerial aux1Serial;
  SoftwareSerial aux2Serial;
 


  //////////////////////////////////////////////////////////////////////
  ///******      Arduino Mega Reset Pin Specific Setup          *****///
  //////////////////////////////////////////////////////////////////////

  #define RST RESET_PIN_2560

//////////////////////////////////////////////////////////////////////
///*****             Camera Lens Variables and settings       *****///
//////////////////////////////////////////////////////////////////////
  
  unsigned long loopTime; // We keep track of the "time" in this variable.

// -------------------------------------------------
// Define some constants to help reference objects,
// pins, leds, colors etc by name instead of numbers
// -------------------------------------------------
//    Status LED VARIABLES
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

  #define STATUS_LED_COUNT 1

  Adafruit_NeoPixel ESP_LED = Adafruit_NeoPixel(STATUS_LED_COUNT, STAUS_LED_PIN, NEO_GRB + NEO_KHZ800);


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
    // variables for storing status and settings from ATMEGA2560. Declared earlier in code but added here for clarity
  // int BL_LDP_Bright;
  // int BL_MAINT_Bright;
  // int BL_VU_Bright;
  // int BL_CS_Bright;
  // int BL_vuOffsetInt;
  // int BL_vuBaselineInt;
  // int BL_vuOffsetExt;
  // int BL_vuBaselineExt;
  // float BL_BatteryVoltage;
  // int BL_BatteryPercentage;


// Define variables to store incoming commands
  String  incomingTargetID;  
  String  incomingSenderID;
  String  incomingCommand;
  bool    incomingCommandIncluded;
  String  incomingPassword;
  // int incomingstructBL_LDP_Bright;
  // int incomingstructBL_MAINT_Bright;
  // int incomingstructBL_VU_Bright;
  // int incomingstructBL_CS_Bright;
  // int incomingstructBL_vuOffsetInt;
  // int incomingstructBL_vuBaselineInt;
  // int incomingstructBL_vuOffsetExt;
  // int incomingstructBL_vuBaselineExt;
  // float incomingstructBL_BatteryVoltage;
  // int incomingstructBL_BatteryPercentage;
  // bool incomingstructbodyLEDControllerStatus;
  
  
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

typedef struct bodyControllerStatus_struct_message{
      char structPassword[25];
      char structSenderID[15];
      char structTargetID[5];
      int structBL_LDP_Bright;
      int structBL_MAINT_Bright;
      int structBL_VU_Bright;
      int structBL_CS_Bright;
      int structBL_vuOffsetInt;
      int structBL_vuBaselineInt;
      int structBL_vuOffsetExt;
      int structBL_vuBaselineExt;
      float structBL_BatteryVoltage;
      int structBL_BatteryPercentage;
      bool structbodyLEDControllerStatus;
      bool structCommandIncluded;
      char structCommand[100];
  } bodyControllerStatus_struct_message;

// Create a struct_message calledcommandsTosend to hold variables that will be sent
  espnow_struct_message commandsToSendtoBroadcast;
  bodyControllerStatus_struct_message commandsToSendtoDroidLoRa;
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

  if (incomingTargetID == "BC" || incomingTargetID == "BR"){
    inputString = incomingCommand;
    stringComplete = true; 

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
IPAddress local_IP(192,168,4,109);
IPAddress subnet(255,255,255,0);
IPAddress gateway(192,168,4,100);

const uint8_t oldLocalMACAddress[] = {0x24, 0x0A, 0xC4, 0xED, 0x30, 0x12};    //used when connecting to WiFi for OTA

AsyncWebServer server(80);


/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////                                                                                       /////////     
/////////                             Start OF FUNCTIONS                                        /////////
/////////                                                                                       /////////     
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////


void colorWipeStatus(String statusled, uint32_t c, int brightness) {
  if(statusled == "ES"){
    ESP_LED.setBrightness(brightness);
    ESP_LED.setPixelColor(0, c);
    ESP_LED.show();
  } 
else{
  Debug.DBG("No LED was chosen \n");
  }
  };


/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////                                                                                               /////
/////                             Serial Communication Functions                                    /////
/////                                                                                               /////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

      /////////////////////////////////////////////////////////
      ///*****          Serial Event Function          *****///
      /////////////////////////////////////////////////////////
      /// This routine is called each loop() runs, so using ///
      /// delay inside loop can delay response.  Multiple   ///
      /// bytes of data may be available.                   ///
      /////////////////////////////////////////////////////////

void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
      stringComplete = true;            // set a flag so the main loop can do something about it.
    };
  };
  Debug.DBG("InputString: %s \n",inputString);
};


void serialAux1Event() {
  while (aux1Serial.available()) {
    // get the new byte:
    char inChar = (char)aux1Serial.read();
    // add it to the inputString:
    inputString += inChar;
    if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
      stringComplete = true;            // set a flag so the main loop can do something about it.
    };
  };
  Debug.ESPNOW("InputString: %s \n",inputString);
};

void serialBlEvent() {
  while (blSerial.available()) {
    // get the new byte:
    StaticJsonDocument<2048> doc;
colorWipeStatus("ES", green, 10);
    // Read the JSON document from the "link" serial port
    DeserializationError err = deserializeJson(doc, blSerial);

    if (err == DeserializationError::Ok) 
    {
      bodyLEDControllerStatus = doc["bodyLEDControllerStatus"].as<bool>();
      if (bodyLEDControllerStatus == 1){
        bodyLEDControllerStatus=1;
        blkeepAliveAge = millis();
        Debug.JSON("Body LED Controler Keepalive Received \n");
        }
      BL_LDP_Bright = doc["LDPBright"].as<int>();
      BL_MAINT_Bright = doc["MaintBright"].as<int>();
      BL_VU_Bright = doc["VUBright"].as<int>();
      BL_CS_Bright = doc["CoinBright"].as<int>();
      BL_vuOffsetInt = doc["VUIntOffset"].as<int>();
      BL_vuBaselineInt = doc["VUIntBaseline"].as<int>();
      BL_vuOffsetExt = doc["VUExtOffset"].as<int>();
      BL_vuBaselineExt = doc["VUExtBaseline"].as<int>();
      BL_BatteryVoltage = doc["BatteryVoltage"].as<float>();
      BL_BatteryPercentage = doc["BatteryPercent"].as<int>();
      // Print the values
      // (we must use as<T>() to resolve the ambiguity)
      Debug.JSON("Body LED Controller Status = %d\n", bodyLEDControllerStatus); //Serial.println(doc["BL_Status"].as<String>());
      Debug.JSON("LDP Brightness = %i\n", BL_LDP_Bright); //Serial.println(doc["LDPBright"].as<int>());
      Debug.JSON("Maint Brightness = %i\n", BL_MAINT_Bright); //Serial.println(doc["MaintBright"].as<int>());
      Debug.JSON("VU Brightness = %i\n", BL_VU_Bright); //Serial.println(doc["VUBright"].as<int>());
      Debug.JSON("Coin Slots Brightness = %i\n",BL_CS_Bright); //Serial.println(doc["CoinBright"].as<int>());
      Debug.JSON("Spectrum Int Offset = %i\n",BL_vuOffsetInt); //Serial.println(doc["VUIntOffset"].as<int>());
      Debug.JSON("Spectrum Int Baseline = %i\n",BL_vuBaselineInt); //Serial.println(doc["VUIntBaseline"].as<int>());
      Debug.JSON("Spectrum Ext Offset = %i\n",BL_vuOffsetExt); //Serial.println(doc["VUExtOffset"].as<int>());
      Debug.JSON("Spectrum Ext Baseline = %i\n",BL_vuBaselineExt); //Serial.println(doc["VUExtOffset"].as<int>());
      Debug.JSON("Battery Voltage = %.2f\n",BL_BatteryVoltage); //Serial.println(doc["BatteryVoltage"].as<float>());
      Debug.JSON("Battery Percentage = %i\n",BL_BatteryPercentage);//Serial.println(doc["BatteryPercent"].as<int>());
    } 
    else 
    {
      // Print error to the "debug" serial port
      if (Debug.debugflag_json == 1){
        Serial.print("deserializeJson() returned ");
      Serial.println(err.c_str());
      }
    }
  };
  colorWipeStatus("ES", blue, 10);

};


void serialStEvent() {
  while (stSerial.available()) {
    // get the new byte:
    char inChar = (char)stSerial.read();
    // add it to the inputString:
    inputString += inChar;
    if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
      stringComplete = true;            // set a flag so the main loop can do something about it.
    };
  };
  Debug.DBG("InputString: %s \n",inputString);
};

String mp3TriggerResponseString;

void serialMpEvent() {
  while (mpSerial.available()) {
    char inChar = (char)mpSerial.read();
    mp3TriggerResponseString += inChar;
    if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
    };
  };
  Debug.DBG("MP3 Trigger Response: %s \n",mp3TriggerResponseString);
  mp3TriggerResponseString = "";
};

  /////////////////////////////////////////////////////////
  ///*****          Serial Write Function          *****///
  /////////////////////////////////////////////////////////
  /// These functions recieve a string and transmits    ///
  /// one character at a time and adds a '/r' to the    ///
  /// end of the string.                                ///
  /////////////////////////////////////////////////////////

void writeSerialString(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++){
      Serial.write(completeString[i]);
  };
};


void writeBlSerial(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++){
    blSerial.write(completeString[i]);
  };
};


void writeaux1Serial(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++){
    aux1Serial.write(completeString[i]);
  };
  Debug.DBG("String to Send over ESPNOW Serial: %s \n" , completeString.c_str());
};


void writeStSerial(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++){
    stSerial.write(completeString[i]);
  };
};

/////////////////////////////////////////////////////////////////////
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

void setupSendStatusStruct(bodyControllerStatus_struct_message* msg, String pass, String sender, String targetID, bool hascommand, String cmd)
{
    snprintf(msg->structPassword, sizeof(msg->structPassword), "%s", pass.c_str());
    snprintf(msg->structSenderID, sizeof(msg->structSenderID), "%s", sender.c_str());
    snprintf(msg->structTargetID, sizeof(msg->structTargetID), "%s", targetID.c_str());
    msg->structBL_LDP_Bright = BL_LDP_Bright;
    msg->structBL_MAINT_Bright = BL_MAINT_Bright;
    msg->structBL_VU_Bright = BL_VU_Bright;
    msg->structBL_CS_Bright = BL_CS_Bright;
    msg->structBL_vuOffsetInt = BL_vuOffsetInt;
    msg->structBL_vuBaselineInt = BL_vuBaselineInt;
    msg->structBL_vuOffsetExt = BL_vuOffsetExt;
    msg->structBL_vuBaselineExt = BL_vuBaselineExt;
    msg->structBL_BatteryVoltage = BL_BatteryVoltage;
    msg->structBL_BatteryPercentage = BL_BatteryPercentage;
    msg->structbodyLEDControllerStatus = bodyLEDControllerStatus;
    msg->structCommandIncluded = hascommand;
    snprintf(msg->structCommand, sizeof(msg->structCommand), "%s", cmd.c_str());
}
int ESPNowSuccessCounter = 1;
int ESPNowFailureCounter = 1;


void sendESPNOWCommand(String starget, String scomm){
  String senderID = "BC";   // change to match location (BC/BS/DC/DP/LD)
  String scommEval = "";
  bool hasCommand;
  if (scommEval = scomm){
    hasCommand = 0;
  } else {hasCommand = 1;};

   if (starget == "LD"){
    setupSendStatusStruct(&commandsToSendtoDroidLoRa, ESPNOWPASSWORD, senderID, starget, hasCommand, scomm);
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
  } else {Debug.ESPNOW("No valid destination \n");}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                               /////
///////                             Miscellaneous Functions                                           /////
///////                                                                                               /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
///*****             Debugging Functions                      *****///
//////////////////////////////////////////////////////////////////////

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
    request->send(200, "text/plain", "Please go to http://192.168.4.110/update to upload file");
  });
  
  AsyncElegantOTA.begin(&server);    // Start AsyncElegantOTA
  server.begin();

  Local_Command[0]   = '\0';
}   
//////////////////////////////////////////////////////////////////////
///*****    Resets Arduino Mega due to bug in my PCB          *****///
//////////////////////////////////////////////////////////////////////

void resetArduino(int delayperiod){
  Debug.DBG("Body LED Controller Reset Function\n");
  digitalWrite(RST,LOW);
  delay(delayperiod);
  digitalWrite(RST,HIGH);
}

//////////////////////////////////////////////////////////////////////
///*****         Function for MP3 Trigger                     *****///
//////////////////////////////////////////////////////////////////////
void mp3Trigger(String comm, int track){
  mpSerial.print(comm);
  mpSerial.write(track);
}


//////////////////////////////////////////////////////////////////////
///*****    Checks the age of the Status Variables            *****///
//////////////////////////////////////////////////////////////////////

void checkAgeofkeepAlive(){    //checks for the variable's age

    if (bodyLEDControllerStatus==1){
    if (millis()-blkeepAliveAge>=keepAliveMillisDuration){
      bodyLEDControllerStatus=0;
      BL_LDP_Bright =0;
      BL_MAINT_Bright = 0;
      BL_VU_Bright = 0;
      BL_CS_Bright =0;
      BL_vuOffsetInt = 0;
      BL_vuBaselineInt = 0;
      BL_vuOffsetExt = 0;
      BL_vuBaselineExt =0;
      BL_BatteryPercentage = 0;
      BL_BatteryVoltage = 0.0;
      Debug.ESPNOW("Body LED Controller Offline\n");
    }
  }
}


void printKeepaliveStatus(){
  if (Debug.debugflag_status == 0)
  {
    Debug.debugflag_status = 1;
    // Debug.STATUS("Dome Controller Status: %d\n", domeControllerStatus);
    // Debug.STATUS("Dome Plate Controller Status: %d\n", domePlateControllerStatus);
    // Debug.STATUS("Body Servo Controller Status: %d\n", bodyServoControllerStatus);
    // Debug.STATUS("Body  Controller Status: %d\n", bodyControllerStatus);
    Debug.STATUS("Body LED Controller Status: %d\n", bodyLEDControllerStatus);
    Debug.STATUS("LDP Bright: %i\n", BL_LDP_Bright);
    Debug.STATUS("Maintenence Bright: %i\n", BL_MAINT_Bright);
    Debug.STATUS("VU Bright: %i\n", BL_VU_Bright);
    Debug.STATUS("Coin SLots Bright: %i\n", BL_CS_Bright);
    Debug.STATUS("vuOffsetInt: %i\n", BL_vuOffsetInt);
    Debug.STATUS("vuBaselineInt: %i\n", BL_vuBaselineInt);
    Debug.STATUS("vuOffsetExt: %i\n", BL_vuOffsetExt);
    Debug.STATUS("vuBaselineExt: %i\n", BL_vuBaselineExt);
    Debug.STATUS("BL_BatteryVoltage: %f\n", BL_BatteryVoltage);
    Debug.STATUS("BL_BatteryPercentage: %i\n", BL_BatteryPercentage);
    Debug.debugflag_status = 0;
  } else
  {  
    Debug.STATUS("Body LED Controller Status: %d\n", bodyLEDControllerStatus);
    Debug.STATUS("LDP Bright: %i\n", BL_LDP_Bright);
    Debug.STATUS("Maintenence Bright: %i\n", BL_MAINT_Bright);
    Debug.STATUS("VU Bright: %i\n", BL_VU_Bright);
    Debug.STATUS("Coin SLots Bright: %i\n", BL_CS_Bright);
    Debug.STATUS("vuOffsetInt: %i\n", BL_vuOffsetInt);
    Debug.STATUS("vuBaselineInt: %i\n", BL_vuBaselineInt);
    Debug.STATUS("vuOffsetExt: %i\n", BL_vuOffsetExt);
    Debug.STATUS("vuBaselineExt: %i\n", BL_vuBaselineExt);
    Debug.STATUS("BL_BatteryVoltage: %f\n", BL_BatteryVoltage);
    Debug.STATUS("BL_BatteryPercentage: %i\n", BL_BatteryPercentage);
  }
  Local_Command[0]   = '\0';
}

void keepAlive(){
  if (millis() - keepAliveMillis >= keepAliveDuration + random(1, 1000)){
    keepAliveMillis = millis();
    sendESPNOWCommand("LD", "");
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
  Serial.begin(115200);
  rdSerial.begin(RD_BAUD_RATE,SERIAL_8N1,SERIAL_RX_RD_PIN,SERIAL_TX_RD_PIN);
  blSerial.begin(BL_BAUD_RATE,SERIAL_8N1,SERIAL_RX_BL_PIN,SERIAL_TX_BL_PIN);
  stSerial.begin(ST_BAUD_RATE,SWSERIAL_8N1,SERIAL_RX_ST_PIN,SERIAL_TX_ST_PIN,false,95);
  mpSerial.begin(MP_BAUD_RATE,SWSERIAL_8N1,SERIAL_RX_MP_PIN,SERIAL_TX_MP_PIN,false,95);
  aux1Serial.begin(AUX1_BAUD_RATE,SWSERIAL_8N1,SERIAL_RX_AUX1_PIN,SERIAL_TX_AUX1_PIN,false,95);
  aux2Serial.begin(AUX2_BAUD_RATE,SWSERIAL_8N1,SERIAL_RX_AUX2_PIN,SERIAL_TX_AUX2_PIN,false,95);
  
  Serial.println("\n\n\n----------------------------------------");
  Serial.println("Booting up the Body ESP Controller");
  
  //Configure the Reset Pins for the arduinoReset() function
  pinMode(RST, OUTPUT);
  digitalWrite(RST,HIGH);

  //Initialize I2C for the Servo Expander Board
//  Wire.begin();
  
  //Initialize the ReelTwo Library
//  SetupEvent::ready();

  ESP_LED.begin();
  ESP_LED.show();
  colorWipeStatus("ES",red,255);
  Serial.println("LED Setup Complete");


  //Reserve the inputStrings
  inputString.reserve(100);                                                              // Reserve 100 bytes for the inputString:
  autoInputString.reserve(100);

  //Initialize the Soft Access Point
  WiFi.mode(WIFI_STA);
  esp_wifi_set_mac(WIFI_IF_STA, &bodyControllerMACAddress[0]);
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
//  // Body Controller
//   memcpy(peerInfo.peer_addr, bodyControllerMACAddress, 6);
//   if (esp_now_add_peer(&peerInfo) != ESP_OK){
//     Serial.println("Failed to add Broadcast ESP-NOW peer");
//     return;
//   }
   // Body Servo Controller
  memcpy(peerInfo.peer_addr, bodyServosControllerMACAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add Broadcast ESP-NOW peer");
    return;
  }
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

  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);

 
  //Initialize the AsycWebServer
   server.begin();

  //Reset Arudino Mega
  resetArduino(500);


}  //end of Setup


void loop(){
  keepAlive();
  checkAgeofkeepAlive();


  if (millis() - MLMillis >= mainLoopDelayVar){
    MLMillis = millis();
    AnimatedEvent::process();
    if(startUp) {
      colorWipeStatus("ES",blue,10);

      startUp = false;
      Serial.println("Startup completed, now running loop");
      // Play Startup Sound
      mp3Trigger("v",16);
      mp3Trigger("t",1);
      mp3Trigger("v",0);

    }
    if(Serial.available()){serialEvent();}
    if(blSerial.available()){serialBlEvent();}
    if(aux1Serial.available()){serialAux1Event();}
    if(stSerial.available()){serialStEvent();}
    if(mpSerial.available()){serialMpEvent();}

    
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
                  case 3: break;  //reserved for commonality. Used for connecting to WiFi and enabling OTA on ESP-NOW Boards 
                  case 4: break;  //reserved for future use
                  case 5:   ;                                                                    break;  //reserved for future use
                  case 6:   ;                                                                   break;  //reserved for future use
                  case 7: ;                  break;  //reserved for future use
                  case 8: printKeepaliveStatus();                                                           break;  //reserved for future use
                  case 9:  break;  //reserved for future use

                }
              }

        }else if (inputBuffer[0] == ':'){
     
         if( inputBuffer[1]=='E'     ||        // Command designatore for internal ESP functions
          inputBuffer[1]=='e'     ||        // Command designatore for internal ESP functions
          inputBuffer[1]=='S'     ||        // Command for sending Serial Strings out Serial ports
          inputBuffer[1]=='s'     ||        // Command for sending Serial Strings out Serial ports
          inputBuffer[1]=='N'     ||        // Command for receiving status/info from other boards
          inputBuffer[1]=='n'     ||         // Command for receiving status/info from other boards
          inputBuffer[1]=='L'     ||        // Command for receiving status/info from other boards
          inputBuffer[1]=='l'               // Command for receiving status/info from other boards

        ){commandLength = strlen(inputBuffer);                                                                                  //  Determines length of command character array.
          Debug.DBG("Command: %s with a length of %d \n", inputBuffer, commandLength);

          if(commandLength >= 3) {
     
              if(inputBuffer[1]=='E' || inputBuffer[1]=='e') {
                for (int i=1; i<=commandLength; i++){
                  char inCharRead = inputBuffer[i];
                  ESPNOWStringCommand += inCharRead;                   // add it to the inputString:
                  }
                  Debug.LOOP("\nFull Command Recieved: %s \n",ESPNOWStringCommand.c_str());
                  ESPNOWTarget = ESPNOWStringCommand.substring(0,2);
                  Debug.LOOP("ESP NOW Target: %s\n", ESPNOWTarget.c_str());
                  ESPNOWSubStringCommand = ESPNOWStringCommand.substring(2,commandLength+1);
                  Debug.LOOP("Command to Forward: %s\n", ESPNOWSubStringCommand.c_str());
                  sendESPNOWCommand(ESPNOWTarget, ESPNOWSubStringCommand);
                  String  ESPNOWStringCommand = "";
                  String ESPNOWSubStringCommand = "";
                  String ESPNOWTarget = "";
                } 
              if(inputBuffer[1]=='S' || inputBuffer[1]=='s') {
              // serialPort =  (inputBuffer[1]-'0')*10+(inputBuffer[2]-'0');
              for (int i=1; i<commandLength;i++ ){
                char inCharRead = inputBuffer[i];
                serialStringCommand += inCharRead;  // add it to the inputString:
              }
              Debug.DBG("Full Serial Command Captured: %s\n", serialStringCommand.c_str());
              serialPort = serialStringCommand.substring(0,2);
              serialSubStringCommand = serialStringCommand.substring(2,commandLength);
              Debug.DBG("Serial Command: %s to Serial Port: %s\n", serialSubStringCommand.c_str(), serialPort);
              if (serialPort == "BL"){
                writeBlSerial(serialSubStringCommand);
                Debug.DBG("Sending out BL Serial\n");
              } else if (serialPort == "EN"){
                writeaux1Serial(serialSubStringCommand);
                Debug.DBG("Sending out EN Serial\n");
              } else if (serialPort == "ST"){
                writeStSerial(serialSubStringCommand);
                Debug.DBG("Sending out ST Serial\n");
              }else if (serialPort == "MP"){
                mp3Comm = serialStringCommand.substring(3,4);
                mp3Track = (inputBuffer[5]-'0')*100+(inputBuffer[6]-'0')*10+(inputBuffer[7]-'0');
                Debug.DBG("Command: %s, Track: %i\n",mp3Comm, mp3Track);
                mp3Trigger(mp3Comm,mp3Track);
                Debug.DBG("Sending out MP Serial\n ");
              } else { Debug.DBG("No valid Serial identified\n");}
              serialStringCommand = "";
              serialPort = "";
              serialSubStringCommand = "";
              int mp3Track;
            }
            if(inputBuffer[1]=='L' || inputBuffer[1]=='l') {
              for (int i=2; i<commandLength;i++ ){
                char inCharRead = inputBuffer[i];
                LEDCommandString += inCharRead;  // add it to the inputString:
              }              
              writeBlSerial(LEDCommandString);
              Debug.LOOP("Sent ATMEGA2560 command of %s \n", LEDCommandString);
              LEDCommandString ="";

            }
          }
        }
        }

      ///***  Clear States and Reset for next command.  ***///
        stringComplete =false;
        autoComplete = false;
        inputBuffer[0] = '\0';
        inputBuffer[1] = '\0';




    }


    
    if(isStartUp) {
      isStartUp = false;
      delay(500);
    }
  }
}  // end of main loop
