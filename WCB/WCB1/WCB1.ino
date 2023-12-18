/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///*****                                                                                                        *****////
///*****                                          Created by Greg Hulette.                                      *****////                                                                                 
///*****                                                                                                        *****////
///*****                                 So exactly what does this all do.....?                                 *****////
///*****                       - Receives commands via Serial or ESP-NOW                                        *****////
///*****                       - Sends Serial commands to other locally connected devices                       *****////
///*****                       - Sends Serial commands to other remotely connected devices                      *****////
///*****                       - Serial Commands sent ends with a Carriage Return "\r"                          *****//// 
///*****                                                                                                        *****////
///*****                                                                                                        *****//// 
///*****                       Wireless Command Syntax:                                                         *****////                        
///*****                       :(xx):(yy)(zzz...)                                                                *****////
///*****                       xx: 2 Digit Identifier for the destination  (i.e. W1 - W9, BR)                   *****////
///*****                          xx: W1 = WCB1                                                                 *****//// 
///*****                          xx: W2 = WCB2                                                                 *****//// 
///*****                          xx: W3 = WCB3                                                                 *****//// 
///*****                          xx: W4 = WCB4                                                                 *****//// 
///*****                          xx: W5 = WCB5                                                                 *****//// 
///*****                          xx: W6 = WCB6                                                                 *****//// 
///*****                          xx: W7 = WCB7                                                                 *****//// 
///*****                          xx: W8 = WCB8                                                                 *****//// 
///*****                          xx: W9 = WCB9                                                                 *****//// 
///*****                          xx: BR = Broadcast                                                            *****//// 
///*****                       yy :Target's Serial Port (i.e. S1-S5)                                            *****////
///*****                          yy: S1 = Serial 1 Port                                                        *****//// 
///*****                          yy: S2 = Serial 2 Port                                                        *****//// 
///*****                          yy: S3 = Serial 3 Port                                                        *****//// 
///*****                          yy: S4 = Serial 4 Port                                                        *****//// 
///*****                          yy: S5 = Serial 5 Port                                                        *****//// 
///*****                       zzz...: String to send out the destination serial port                           *****////
///*****                          zzz...: any string of characters up to 90 characters long                     *****//// 
///*****                                                                                                        *****////
///*****          Example1: :W2S2:PP100  (Sends to WCB2's Serial 2 port and sends string ":PP100" + "\r")       *****////
///*****          Example2: :W3S3:R01    (Sends to WCB3's Serial 3 port and sends string ":R01" + "\r")         *****////
///*****          Example3: :W5S4MD904   (Sends to WCB5's Serial 4 port and sends string "MD904" + "\r")        *****////
///*****                                                                                                        *****////
///*****                                                                                                        *****////
///*****                       Local Serial Command Syntax:                                                     *****////                        
///*****                       :(yy)(zzz...)                                                                    *****////
///*****                       yy :Target Serial Port (i.e. S1-S5)                                              *****////
///*****                          yy: S1 = Serial 1 Port                                                        *****//// 
///*****                          yy: S2 = Serial 2 Port                                                        *****//// 
///*****                          yy: S3 = Serial 3 Port                                                        *****//// 
///*****                          yy: S4 = Serial 4 Port                                                        *****//// 
///*****                          yy: S5 = Serial 5 Port                                                        *****//// 
///*****                       zzz...: String to send out the destination serial port                           *****////
///*****                          zzz...: any string of characters up to 90 characters long                     *****//// 
///*****                                                                                                        *****////
///*****          Example1: :S2:PP100  (Sends to local Serial 2 port and sends string ":PP100" + "\r")          *****////
///*****          Example2: :S3:R01    (Sends to local Serial 3 port and sends string ":SE05" + "\r")           *****////
///*****          Example3: :S4MD904   (Sends to local Serial 4 port and sends string "MD904" + "\r")           *****////
///*****                                                                                                        *****////                      
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
///*****        Libraries used in this sketch                 *****///
//////////////////////////////////////////////////////////////////////

// Standard Arduino library
#include <Arduino.h>

//Used for ESP-NOW
#include <WiFi.h>
#include "esp_wifi.h"
#include <esp_now.h>

//pin definitions
#include "wcb_pin_map.h"

// Debug Functions  - Using my own library for this.  This should be in the same folder as this sketch.
#include "DebugR2.h" 

// Used for Software Serial to allow more serial port capacity
#include <SoftwareSerial.h>


//////////////////////////////////////////////////////////////////////
///*****          Preferences/Items to change                 *****///
//////////////////////////////////////////////////////////////////////
 int WCB_Quantity = 2;          // Change to match the amount of WCB's that you are using.  This alleviates initialing ESP-NOW peers if they are not used.

  // Uncomment the board that you are loading this sketch onto.  Uncomment in order starting from 1.
  #define WCB1 
  // #define WCB2 
  // #define WCB3 
  // #define WCB4 
  // #define WCB5 
  // #define WCB6 
  // #define WCB7 
  // #define WCB8 
  // #define WCB9

  // ESPNOW Password - This must be the same across all devices and unique to your droid/setup. (PLEASE CHANGE THIS)
  String ESPNOWPASSWORD = "WCB_Astromech_xxxxx";

  // Serial Baud Rates
  #define SERIAL1_BAUD_RATE 115200
  #define SERIAL2_BAUD_RATE 9600 
  #define SERIAL3_BAUD_RATE 9600  
  #define SERIAL4_BAUD_RATE 9600  //Should be lower than 57600, I'd recommend 9600 or lower for best reliability
  #define SERIAL5_BAUD_RATE 9600  //Should be lower than 57600, I'd recommend 9600 or lower for best reliability


  // Mac Address Customization: MUST BE THE SAME ON ALL BOARDS - Allows you to easily change 2nd and 3rd octects of the mac addresses so that there are more unique addresses out there.  
  // Can be any 2-digit hexidecimal number.  Just match the hex number with the string.  Each digit can be 0-9 or A-F.  Example is "0x1A", or "0x56" or "0xB2"

  const uint8_t umac_oct2 = 0x01;     
  String umac_oct2_String = "01:";      // Must match the unique Mac Address "umac_oct2" variable withouth the "0x"

  const uint8_t umac_oct3 = 0x00;
  String umac_oct3_String = "00:";      // Must match the unique Mac Address "umac_oct3" variable withouth the "0x"


//////////////////////////////////////////////////////////////////////
///*****        Command Varaiables, Containers & Flags        *****///
//////////////////////////////////////////////////////////////////////
  
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

  debugClass Debug;
  String debugInputIdentifier ="";

  #ifdef WCB1
    String ESPNOW_SenderID = "W1";
    String HOSTNAME = "Wireless Control Board 1 (W1)";
  #elif defined (WCB2)
    String ESPNOW_SenderID = "W2";
    String HOSTNAME = "Wireless Control Board 2 (W2)";
  #elif defined (WCB3)
    String ESPNOW_SenderID = "W3";
    String HOSTNAME = "Wireless Control Board 3 (W3)";
  #elif defined (WCB4)
    String ESPNOW_SenderID = "W4";
    String HOSTNAME = "Wireless Control Board 4 (W4)";
  #elif defined (WCB5)
    String ESPNOW_SenderID = "W5";
    String HOSTNAME = "Wireless Control Board 5 (W5)";
  #elif defined (WCB6)
    String ESPNOW_SenderID = "W6";
    String HOSTNAME = "Wireless Control Board 6 (W6)";
  #elif defined (WCB7)
    String ESPNOW_SenderID = "W7";
    String HOSTNAME = "Wireless Control Board 7 (W7)";
  #elif defined (WCB8)
    String ESPNOW_SenderID = "W8";
    String HOSTNAME = "Wireless Control Board 8 (W8)";
  #elif defined (WCB9)
    String ESPNOW_SenderID = "W9";
    String HOSTNAME = "Wireless Control Board 9 (W9)";
  #endif


//////////////////////////////////////////////////////////////////////
///*****       Startup and Loop Variables                     *****///
//////////////////////////////////////////////////////////////////////
  
  boolean startUp = true;
  boolean isStartUp = true;
  
  //Main Loop Timers
  unsigned long mainLoopTime; 
  unsigned long MLMillis;
  byte mainLoopDelayVar = 5;

 
//////////////////////////////////////////////////////////////////
///******       Serial Ports Definitions                  *****///
//////////////////////////////////////////////////////////////////


  #define s2Serial Serial1
  #define s3Serial Serial2
  SoftwareSerial s4Serial;
  SoftwareSerial s5Serial;
  
  
/////////////////////////////////////////////////////////////////////////
///*****                  ESP NOW Set Up                         *****///
/////////////////////////////////////////////////////////////////////////
  
  //  ESP-NOW MAC Addresses used in the Droid. 
  const uint8_t WCB1MacAddress[] =  {0x02, umac_oct2, umac_oct3, 0x00, 0x00, 0x01};
  const uint8_t WCB2MacAddress[] =  {0x02, umac_oct2, umac_oct3, 0x00, 0x00, 0x02};
  const uint8_t WCB3MacAddress[] =  {0x02, umac_oct2, umac_oct3, 0x00, 0x00, 0x03};
  const uint8_t WCB4MacAddress[]=   {0x02, umac_oct2, umac_oct3, 0x00, 0x00, 0x04};
  const uint8_t WCB5MacAddress[] =  {0x02, umac_oct2, umac_oct3, 0x00, 0x00, 0x05};
  const uint8_t WCB6MacAddress[] =  {0x02, umac_oct2, umac_oct3, 0x00, 0x00, 0x06};
  const uint8_t WCB7MacAddress[] =  {0x02, umac_oct2, umac_oct3, 0x00, 0x00, 0x07};
  const uint8_t WCB8MacAddress[] =  {0x02, umac_oct2, umac_oct3, 0x00, 0x00, 0x08};
  const uint8_t WCB9MacAddress[] =  {0x02, umac_oct2, umac_oct3, 0x00, 0x00, 0x09};
  const uint8_t broadcastMACAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  // Uses these Strings for comparators
  String WCB1MacAddressString = "02:" + umac_oct2_String + umac_oct3_String + "00:00:01";
  String WCB2MacAddressString = "02:" + umac_oct2_String + umac_oct3_String + "00:00:02";
  String WCB3MacAddressString = "02:" + umac_oct2_String + umac_oct3_String + "00:00:03";
  String WCB4MacAddressString = "02:" + umac_oct2_String + umac_oct3_String + "00:00:04";
  String WCB5MacAddressString = "02:" + umac_oct2_String + umac_oct3_String + "00:00:05";
  String WCB6MacAddressString = "02:" + umac_oct2_String + umac_oct3_String + "00:00:06";
  String WCB7MacAddressString = "02:" + umac_oct2_String + umac_oct3_String + "00:00:07";
  String WCB8MacAddressString = "02:" + umac_oct2_String + umac_oct3_String + "00:00:08";
  String WCB9MacAddressString = "02:" + umac_oct2_String + umac_oct3_String + "00:00:09";
  String broadcastMACAddressString =  "FF:FF:FF:FF:FF:FF";

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
  espnow_struct_message commandsToSendtoWCB1;
  espnow_struct_message commandsToSendtoWCB2;
  espnow_struct_message commandsToSendtoWCB3;
  espnow_struct_message commandsToSendtoWCB4;
  espnow_struct_message commandsToSendtoWCB5;
  espnow_struct_message commandsToSendtoWCB6;
  espnow_struct_message commandsToSendtoWCB7;
  espnow_struct_message commandsToSendtoWCB8;
  espnow_struct_message commandsToSendtoWCB9;
  espnow_struct_message commandsToSendtoBroadcast;


  // Create a espnow_struct_message to hold variables that will be received
  espnow_struct_message commandsToReceiveFromWCB1;
  espnow_struct_message commandsToReceiveFromWCB2;
  espnow_struct_message commandsToReceiveFromWCB3;
  espnow_struct_message commandsToReceiveFromWCB4;
  espnow_struct_message commandsToReceiveFromWCB5;
  espnow_struct_message commandsToReceiveFromWCB6;
  espnow_struct_message commandsToReceiveFromWCB7;
  espnow_struct_message commandsToReceiveFromWCB8;
  espnow_struct_message commandsToReceiveFromWCB9;
  espnow_struct_message commandsToReceiveFromBroadcast;


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

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  // colorWipeStatus("ES", orange ,255);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  String IncomingMacAddress(macStr);
  if (IncomingMacAddress == WCB1MacAddressString) {
    memcpy(&commandsToReceiveFromWCB1, incomingData, sizeof(commandsToReceiveFromWCB1));
    incomingPassword = commandsToReceiveFromWCB1.structPassword;
    if (incomingPassword != ESPNOWPASSWORD){
    Debug.ESPNOW("Wrong ESP-NOW Password was sent.  Message Ignored\n");  
    } else {
      incomingSenderID = commandsToReceiveFromWCB1.structSenderID;
      incomingTargetID = commandsToReceiveFromWCB1.structTargetID;
      incomingCommandIncluded = commandsToReceiveFromWCB1.structCommandIncluded;
      incomingCommand = commandsToReceiveFromWCB1.structCommand;
      processESPNOWIncomingMessage();
    } 
  } else if (IncomingMacAddress == WCB2MacAddressString){
    memcpy(&commandsToReceiveFromWCB2, incomingData, sizeof(commandsToReceiveFromWCB2));
    incomingPassword = commandsToReceiveFromWCB2.structPassword;
    if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password of %s was sent.  Message Ignored\n", incomingPassword.c_str());  
      } else {
        incomingSenderID = commandsToReceiveFromWCB2.structSenderID;
        incomingTargetID = commandsToReceiveFromWCB2.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromWCB2.structCommandIncluded;
        incomingCommand = commandsToReceiveFromWCB2.structCommand;
        processESPNOWIncomingMessage();
      }
  } else if (IncomingMacAddress == WCB3MacAddressString){
    memcpy(&commandsToReceiveFromWCB3, incomingData, sizeof(commandsToReceiveFromWCB3));
    incomingPassword = commandsToReceiveFromWCB3.structPassword;
    if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password of %s was sent.  Message Ignored\n", incomingPassword.c_str());  
      } else {
        incomingSenderID = commandsToReceiveFromWCB3.structSenderID;
        incomingTargetID = commandsToReceiveFromWCB3.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromWCB3.structCommandIncluded;
        incomingCommand = commandsToReceiveFromWCB3.structCommand;
        processESPNOWIncomingMessage();
      }
  } else if (IncomingMacAddress == WCB4MacAddressString){
    memcpy(&commandsToReceiveFromWCB4, incomingData, sizeof(commandsToReceiveFromWCB4));
    incomingPassword = commandsToReceiveFromWCB4.structPassword;
    if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password of %s was sent.  Message Ignored\n", incomingPassword.c_str());  
      } else {
        incomingSenderID = commandsToReceiveFromWCB4.structSenderID;
        incomingTargetID = commandsToReceiveFromWCB4.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromWCB4.structCommandIncluded;
        incomingCommand = commandsToReceiveFromWCB4.structCommand;
        processESPNOWIncomingMessage();
      }
  } else if (IncomingMacAddress == WCB5MacAddressString){
    memcpy(&commandsToReceiveFromWCB5, incomingData, sizeof(commandsToReceiveFromWCB5));
    incomingPassword = commandsToReceiveFromWCB4.structPassword;
    if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password of %s was sent.  Message Ignored\n", incomingPassword.c_str());  
      } else {
        incomingSenderID = commandsToReceiveFromWCB5.structSenderID;
        incomingTargetID = commandsToReceiveFromWCB5.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromWCB5.structCommandIncluded;
        incomingCommand = commandsToReceiveFromWCB5.structCommand;
        processESPNOWIncomingMessage();
      }
  } else if (IncomingMacAddress == WCB6MacAddressString){
    memcpy(&commandsToReceiveFromWCB5, incomingData, sizeof(commandsToReceiveFromWCB5));
    incomingPassword = commandsToReceiveFromWCB5.structPassword;
    if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password of %s was sent.  Message Ignored\n", incomingPassword.c_str());  
      } else {
        incomingSenderID = commandsToReceiveFromWCB5.structSenderID;
        incomingTargetID = commandsToReceiveFromWCB5.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromWCB5.structCommandIncluded;
        incomingCommand = commandsToReceiveFromWCB5.structCommand;
        processESPNOWIncomingMessage();
      }
  } else if (IncomingMacAddress == WCB6MacAddressString){
    memcpy(&commandsToReceiveFromWCB6, incomingData, sizeof(commandsToReceiveFromWCB6));
    incomingPassword = commandsToReceiveFromWCB6.structPassword;
    if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password of %s was sent.  Message Ignored\n", incomingPassword.c_str());  
      } else {
        incomingSenderID = commandsToReceiveFromWCB6.structSenderID;
        incomingTargetID = commandsToReceiveFromWCB6.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromWCB6.structCommandIncluded;
        incomingCommand = commandsToReceiveFromWCB6.structCommand;
        processESPNOWIncomingMessage();
      }
  } else if (IncomingMacAddress == WCB7MacAddressString){
    memcpy(&commandsToReceiveFromWCB7, incomingData, sizeof(commandsToReceiveFromWCB7));
    incomingPassword = commandsToReceiveFromWCB7.structPassword;
    if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password of %s was sent.  Message Ignored\n", incomingPassword.c_str());  
      } else {
        incomingSenderID = commandsToReceiveFromWCB7.structSenderID;
        incomingTargetID = commandsToReceiveFromWCB7.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromWCB7.structCommandIncluded;
        incomingCommand = commandsToReceiveFromWCB7.structCommand;
        processESPNOWIncomingMessage();
      }
  } else if (IncomingMacAddress == WCB8MacAddressString){
    memcpy(&commandsToReceiveFromWCB8, incomingData, sizeof(commandsToReceiveFromWCB8));
    incomingPassword = commandsToReceiveFromWCB8.structPassword;
    if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password of %s was sent.  Message Ignored\n", incomingPassword.c_str());  
      } else {
        incomingSenderID = commandsToReceiveFromWCB8.structSenderID;
        incomingTargetID = commandsToReceiveFromWCB8.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromWCB8.structCommandIncluded;
        incomingCommand = commandsToReceiveFromWCB8.structCommand;
        processESPNOWIncomingMessage();
      }
  } else if (IncomingMacAddress == WCB9MacAddressString){
    memcpy(&commandsToReceiveFromWCB9, incomingData, sizeof(commandsToReceiveFromWCB9));
    incomingPassword = commandsToReceiveFromWCB9.structPassword;
    if (incomingPassword != ESPNOWPASSWORD){
        Debug.ESPNOW("Wrong ESP-NOW Password of %s was sent.  Message Ignored\n", incomingPassword.c_str());  
      } else {
        incomingSenderID = commandsToReceiveFromWCB9.structSenderID;
        incomingTargetID = commandsToReceiveFromWCB9.structTargetID;
        incomingCommandIncluded = commandsToReceiveFromWCB9.structCommandIncluded;
        incomingCommand = commandsToReceiveFromWCB9.structCommand;
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
  } 
  else {Debug.ESPNOW("ESP-NOW Mesage ignored \n");}  
  IncomingMacAddress ="";  
} 

void processESPNOWIncomingMessage(){
  Debug.ESPNOW("incoming target: %s\n", incomingTargetID.c_str());
  Debug.ESPNOW("incoming sender: %s\n", incomingSenderID.c_str());
  Debug.ESPNOW("incoming command included: %d\n", incomingCommandIncluded);
  Debug.ESPNOW("incoming command: %s\n", incomingCommand.c_str());
  if (incomingTargetID == ESPNOW_SenderID || incomingTargetID == "BR"){
    inputString = incomingCommand;
    stringComplete = true; 
    Debug.ESPNOW("Recieved command from %s \n", incomingSenderID);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////                                                                                       /////////     
/////////                             Start OF FUNCTIONS                                        /////////
/////////                                                                                       /////////     
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////                              Communication Functions                                          /////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
///*****          Serial Write Function          *****///
/////////////////////////////////////////////////////////

void writeSerialString(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    Serial.write(completeString[i]);
  }
}

void writes2SerialString(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    s2Serial.write(completeString[i]);
  }
}

void writes3SerialString(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    s3Serial.write(completeString[i]);
  }
}

void writes4SerialString(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    s4Serial.write(completeString[i]);
  }
}

void writes5SerialString(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    s5Serial.write(completeString[i]);
  }
}

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
}

void s2SerialEvent() {
  while (s2Serial.available()) {
    char inChar = (char)s2Serial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  Debug.SERIAL_EVENT("Serial 1 Input: %s \n",inputString);
}
void s3SerialEvent() {
  while (s3Serial.available()) {
    char inChar = (char)s3Serial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  Debug.SERIAL_EVENT("Serial 2 Input: %s \n",inputString);
}
void s4SerialEvent() {
  while (s4Serial.available()) {
    char inChar = (char)s4Serial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  Debug.SERIAL_EVENT("Serial 3 Input: %s \n",inputString);
}
void s5SerialEvent() {
  while (s5Serial.available()) {
    char inChar = (char)s5Serial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  Debug.SERIAL_EVENT("Serial 4 Input: %s \n",inputString);
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
};

void sendESPNOWCommand(String starget, String scomm){
  String scommEval = "";
  bool hasCommand;
  if (scommEval == scomm){
    hasCommand = 0;
  } else {hasCommand = 1;};

  if (starget == "W1"){
    setupSendStruct(&commandsToSendtoWCB1, ESPNOWPASSWORD, ESPNOW_SenderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(WCB1MacAddress, (uint8_t *) &commandsToSendtoWCB1, sizeof(commandsToSendtoWCB1));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW WCB1 Neighbor\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data to ESP-NOW WCB1 Neighbor \n");}
  }  else if (starget == "W2"){
    setupSendStruct(&commandsToSendtoWCB2, ESPNOWPASSWORD, ESPNOW_SenderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(WCB2MacAddress, (uint8_t *) &commandsToSendtoWCB2, sizeof(commandsToSendtoWCB2));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW WCB2 Neighbor\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data to ESP-NOW WCB2 Neighbor\n");}
  } else if (starget == "W3"){
    setupSendStruct(&commandsToSendtoWCB3, ESPNOWPASSWORD, ESPNOW_SenderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(WCB3MacAddress, (uint8_t *) &commandsToSendtoWCB3, sizeof(commandsToSendtoWCB3));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW WCB3 Neighbor\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data to ESP-NOW WCB3 Neighbor\n");}
  } else if (starget == "W4"){
    setupSendStruct(&commandsToSendtoWCB4, ESPNOWPASSWORD, ESPNOW_SenderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(WCB4MacAddress, (uint8_t *) &commandsToSendtoWCB4, sizeof(commandsToSendtoWCB4));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW WCB4 Neighbor\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data to ESP-NOW WCB4 Neighbor\n");}
  } else if (starget == "W5"){
    setupSendStruct(&commandsToSendtoWCB5, ESPNOWPASSWORD, ESPNOW_SenderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(WCB5MacAddress, (uint8_t *) &commandsToSendtoWCB5, sizeof(commandsToSendtoWCB5));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW WCB5 Neighbor\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data to ESP-NOW WCB5 Neighbor\n");}
  } else if (starget == "W6"){
    setupSendStruct(&commandsToSendtoWCB6, ESPNOWPASSWORD, ESPNOW_SenderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(WCB6MacAddress, (uint8_t *) &commandsToSendtoWCB6, sizeof(commandsToSendtoWCB6));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW WCB6 Neighbor\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data to ESP-NOW WCB6 Neighbor\n");}
  } else if (starget == "W7"){
    setupSendStruct(&commandsToSendtoWCB7, ESPNOWPASSWORD, ESPNOW_SenderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(WCB7MacAddress, (uint8_t *) &commandsToSendtoWCB7, sizeof(commandsToSendtoWCB7));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW WCB7 Neighbor\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the datato ESP-NOW WCB7 Neighbor\n");}
  } else if (starget == "W8"){
    setupSendStruct(&commandsToSendtoWCB8, ESPNOWPASSWORD, ESPNOW_SenderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(WCB8MacAddress, (uint8_t *) &commandsToSendtoWCB8, sizeof(commandsToSendtoWCB8));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW WCB8 Neighbor\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data to ESP-NOW WCB8 Neighbor\n");}
  } else if (starget == "W9"){
    setupSendStruct(&commandsToSendtoWCB9, ESPNOWPASSWORD, ESPNOW_SenderID, starget, hasCommand, scomm);
    esp_err_t result = esp_now_send(WCB9MacAddress, (uint8_t *) &commandsToSendtoWCB9, sizeof(commandsToSendtoWCB9));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW WCB9 Neighbor\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data to ESP-NOW WCB9 Neighbor\n");}
  } else if (starget == "BR"){
    setupSendStruct(&commandsToSendtoBroadcast, ESPNOWPASSWORD, ESPNOW_SenderID, starget, hasCommand, scomm);
       esp_err_t result = esp_now_send(broadcastMACAddress, (uint8_t *) &commandsToSendtoBroadcast, sizeof(commandsToSendtoBroadcast));
    if (result == ESP_OK) {Debug.ESPNOW("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
    }else {Debug.ESPNOW("Error sending the data\n");}
  } else {Debug.ESPNOW("No valid destination \n");}
};



/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////                                                                                       /////////     
/////////                             END OF FUNCTIONS                                          /////////
/////////                                                                                       /////////     
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////



void setup(){
  // initializes the 5 serial ports
  Serial.begin(SERIAL1_BAUD_RATE);
  s2Serial.begin(SERIAL2_BAUD_RATE,SERIAL_8N1,SERIAL2_RX_PIN,SERIAL2_TX_PIN);
  s3Serial.begin(SERIAL3_BAUD_RATE,SERIAL_8N1,SERIAL3_RX_PIN,SERIAL3_TX_PIN);  
  s4Serial.begin(SERIAL4_BAUD_RATE,SWSERIAL_8N1,SERIAL4_RX_PIN,SERIAL4_TX_PIN,false,95);  
  s5Serial.begin(SERIAL5_BAUD_RATE,SWSERIAL_8N1,SERIAL5_RX_PIN,SERIAL5_TX_PIN,false,95);  

  // prints out a bootup message of the local hostname
  Serial.println("\n\n----------------------------------------");
  Serial.print("Booting up the ");Serial.println(HOSTNAME);
  Serial.println("----------------------------------------");

  //Reserve the memory for inputStrings
  inputString.reserve(100);                                                              // Reserve 100 bytes for the inputString:
  autoInputString.reserve(100);

  //initialize WiFi for ESP-NOW
  WiFi.mode(WIFI_STA);

  // Sets the local Mac Address for ESP-NOW 
  #ifdef WCB1
    esp_wifi_set_mac(WIFI_IF_STA, &WCB1MacAddress[0]);
  #elif defined (WCB2)
    esp_wifi_set_mac(WIFI_IF_STA, &WCB2MacAddress[0]);
  #elif defined (WCB3)
    esp_wifi_set_mac(WIFI_IF_STA, &WCB3MacAddress[0]);
  #elif defined (WCB4)
    esp_wifi_set_mac(WIFI_IF_STA, &WCB4MacAddress[0]);
  #elif defined (WCB5)
    esp_wifi_set_mac(WIFI_IF_STA, &WCB5MacAddress[0]);
  #elif defined (WCB6)
    esp_wifi_set_mac(WIFI_IF_STA, &WCB6MacAddress[0]);
  #elif defined (WCB7)
    esp_wifi_set_mac(WIFI_IF_STA, &WCB7MacAddress[0]);
  #elif defined (WCB8)
    esp_wifi_set_mac(WIFI_IF_STA, &WCB8MacAddress[0]);
  #elif defined (WCB9)
    esp_wifi_set_mac(WIFI_IF_STA, &WCB9MacAddress[0]);
  #endif

  //Prints out local Mac on bootup
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

  // Add ESP-NOW peers  
  
  // Broadcast
  memcpy(peerInfo.peer_addr, broadcastMACAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add Broadcast ESP-NOW peer");
    return;
  }
if (WCB_Quantity >= 1 ){
    // WCB1 ESP-NOW Peer
    #ifndef WCB1
      memcpy(peerInfo.peer_addr, WCB1MacAddress, 6);
      if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Failed to add WCB1 ESP-NOW peer");
        return;
    }  
    #endif
}
if (WCB_Quantity >= 2 ){
  // WCB2 ESP-NOW Peer
  #ifndef WCB2
    memcpy(peerInfo.peer_addr, WCB2MacAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add WCB2 ESP-NOW peer");
      return;
    }  
  #endif
}
  
if (WCB_Quantity >= 3 ){
  // WCB3 ESP-NOW Peer
  #ifndef WCB3
    memcpy(peerInfo.peer_addr, WCB3MacAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add WCB3 ESP-NOW peer");
      return;
    }  
  #endif
}
if (WCB_Quantity >= 4 ){
  // WCB4 ESP-NOW Peer
  #ifndef WCB4
  memcpy(peerInfo.peer_addr, WCB4MacAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add WCB4 ESP-NOW peer");
    return;
  }  
  #endif
}

if (WCB_Quantity >= 5 ){
  // WCB5 ESP-NOW Peer
  #ifndef WCB5
  memcpy(peerInfo.peer_addr, WCB5MacAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add WCB5 ESP-NOW peer");
    return;
  }  
  #endif
}

if (WCB_Quantity >= 6 ){
  // WCB6 ESP-NOW Peer
  #ifndef WCB6
  memcpy(peerInfo.peer_addr, WCB6MacAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add WCB6 ESP-NOW peer");
    return;
  }  
  #endif
}

if (WCB_Quantity >= 7 ){

  // WCB7 ESP-NOW Peer
  #ifndef WCB7
  memcpy(peerInfo.peer_addr, WCB7MacAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add WCB7 ESP-NOW peer");
    return;
  }  
  #endif
}
if (WCB_Quantity >= 8 ){
    // WCB8 ESP-NOW Peer
    #ifndef WCB8
    memcpy(peerInfo.peer_addr, WCB8MacAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add WCB8 ESP-NOW peer");
      return;
    }  
    #endif
}
if (WCB_Quantity >= 9 ){
  // WCB9 ESP-NOW Peer
    #ifndef WCB9
    memcpy(peerInfo.peer_addr, WCB9MacAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add WCB9 ESP-NOW peer");
      return;
    } 
    #endif
}

  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
  
}   // end of setup


void loop(){
  if (millis() - MLMillis >= mainLoopDelayVar){
    MLMillis = millis();
    if(startUp) {
      startUp = false;
      Serial.print("Startup complete\nStarting main loop\n\n\n");
    }

    // looks for new serial commands (Needed because ESP's do not have an onSerialEvent function)
    if(Serial.available()){serialEvent();}
    if(s2Serial.available()){s2SerialEvent();}
    if(s3Serial.available()){s3SerialEvent();}
    if(s4Serial.available()){s4SerialEvent();}
    if(s5Serial.available()){s5SerialEvent();}

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
                  case 3: ; break;  //reserved for future use
                  case 4: ; break;  //reserved for future use
                  case 5: ; break;  //reserved for future use
                  case 6: ; break;  //reserved for future use
                  case 7: ; break;  //reserved for future use
                  case 8: ; break;  //reserved for future use                                                         break;  //reserved for future use
                  case 9: ; break;  //reserved for future use

                }
              }

      }else if (inputBuffer[0] == ':'){
        if( inputBuffer[1]=='W' ||        // Command for Sending ESP-NOW Messages
            inputBuffer[1]=='w' ||        // Command for Sending ESP-NOW Messages
            inputBuffer[1]=='S' ||        // Command for sending Serial Strings out Serial ports
            inputBuffer[1]=='s'           // Command for sending Serial Strings out Serial ports
        ){commandLength = strlen(inputBuffer);                     //  Determines length of command character array.
          Debug.DBG("Command Length is: %i\n" , commandLength);
          if(commandLength >= 3) {
            if(inputBuffer[1]=='W' || inputBuffer[1]=='w') {
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
              // reset ESP-NOW Variables
              ESPNOWStringCommand = "";
              ESPNOWSubStringCommand = "";
              ESPNOWTarget = "";  
              }  
            if(inputBuffer[1]=='S' || inputBuffer[1]=='s') {
              for (int i=1; i<commandLength;i++ ){
                char inCharRead = inputBuffer[i];
                serialStringCommand += inCharRead;  // add it to the inputString:
              }
              serialPort = serialStringCommand.substring(0,2);
              serialSubStringCommand = serialStringCommand.substring(2,commandLength);
              Debug.LOOP("Serial Command: %s to Serial Port: %s\n", serialSubStringCommand.c_str(), serialPort);  
              if (serialPort == "S1"){
                writeSerialString(serialStringCommand);
              } else if (serialPort == "S2"){
                writes2SerialString(serialStringCommand);
              } else if (serialPort == "S3"){
                writes3SerialString(serialStringCommand);
              }else if (serialPort == "S4"){
                writes4SerialString(serialStringCommand);
              } else if (serialPort == "S5"){
                writes5SerialString(serialStringCommand);
              }else {Debug.LOOP("No valid serial port given \n");}
              serialStringCommand = "";
              serialPort = "";
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
      int espCommandFunction;

      Debug.LOOP("command Proccessed\n");
    
    }  
    if(isStartUp) {
      isStartUp = false;
      delay(500);
    }
  }
}
