//////////////////////////////////////\\///////////////////////////////////////////////////////////////////////////////
///*****                                                                                                        *****///
///*****                                          Created by Greg Hulette.                                      *****///
///*****                                                                                                        *****///
///*****   I started with the code from flthymcnsty from from which I used the basic command structure and      *****///
///*****  serial input method.                                                                                  *****///
///*****                                                                                                        *****///                                                                                                                                                           *****///
///*****                                 So exactly what does this all do.....?                                 *****///
///*****                       - Receives commands via Serial or ESP-NOW                                        *****///
///*****                       - Sends Serial commands to the Uppity Spinner and other gadgets                  *****///                                                     *****///
///*****                                                                                                        *****///                                                                                                                                                           *****///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////







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

//pin definitions
#include "dome_plate_controller_pin_map.h"

// Debug Functions  - Using my own library for this
#include <DebugR2.h>

//ReelTwo libaries - Using my forked version of this libarary
#include "ReelTwo.h"
#include "core/DelayCall.h"

// Used for Software Serial to allow more useful naming
#include <SoftwareSerial.h>







//////////////////////////////////////////////////////////////////////
///*****          Preferences/Items to change                 *****///
//////////////////////////////////////////////////////////////////////
 // ESPNOW Password - This must be the same across all devices
  String ESPNOWPASSWORD = "GregsAstromech";

  // R2 Control Network Details
  const char* ssid = "R2D2_Control_Network";
  const char* password =  "astromech";

  // Keepalive timer to send status messages to the Kill Switch (Droid)
  int keepAliveDuration= 5000;  // 5 seconds

// used to sync timing with the dome controller better, allowing time for the ESP-NOW messages to travel to the dome
// Change this to work with how your droid performs
  int defaultESPNOWSendDuration = 50;  

    // Serial Baud Rates
  #define PL_BAUD_RATE 9600
  #define SERIAL1_BAUD_RATE 115200 
  #define SERIAL2_BAUD_RATE 9600  //Should be lower than 57600
  #define SERIAL3_BAUD_RATE 9600  //Should be lower than 57600
  #define SERIAL4_BAUD_RATE 57600 //Should be lower than 57600



//////////////////////////////////////////////////////////////////////
///*****        Command Varaiables, Containers & Flags        *****///
//////////////////////////////////////////////////////////////////////
  String HOSTNAME = "Dome Plate Controller";
  
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

  uint32_t ESPNOW_command[6]  = {0,0,0,0,0,0};
  int espNowCommandFunction = 0;
  String espNowCommandFunctionString;
  String tempESPNOWTargetID;
  String inputStringCommand;

// Flags to enable/disable debugging in runtime
  boolean debugflag = 1;    // Normally set to 0, but leaving at 1 during coding and testing.
  boolean debugflag1 = 0;  // Used for optional level of debuging
  boolean debugflag_espnow = 0;
  boolean debugflag_servo = 0;
  boolean debugflag_serial_event = 0;
  boolean debugflag_loop = 0;
  boolean debugflag_http = 0;
  boolean debugflag_lora = 0;


  //////////////////////////////////////////////////////////////////////
  ///*****       Startup and Loop Variables                     *****///
  //////////////////////////////////////////////////////////////////////
  
  boolean startUp = true;
  boolean isStartUp = true;
  
  unsigned long mainLoopTime; // We keep track of the "Main Loop time" in this variable.
  unsigned long MLMillis;
  byte mainLoopDelayVar = 5;

  //////////////////////////////////////////////////////////////////////
  ///******       Serial Ports Specific Setup                   *****///
  //////////////////////////////////////////////////////////////////////

  #define RXFU 19
  #define TXFU 18 
  #define RXPL 25
  #define TXPL 27

  #define plSerial Serial1
  #define fuSerial Serial2
  
  #define PL_BAUD_RATE 9600
  #define FU_BAUD_RATE 115200


/////////////////////////////////////////////////////////////////////////
///*****                  ESP NOW Set Up                         *****///
/////////////////////////////////////////////////////////////////////////

//  MAC Addresses used in the Droid.  Not really needed because we broadcast everything but good to know for troublshooting.
//  Droid LoRa =              {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
//  Body Controller =         {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};
//  Body Servos Controller =  {0x02, 0x00, 0x00, 0x00, 0x00, 0x03};
//  Dome Controller =         {0x02, 0x00, 0x00, 0x00, 0x00, 0x04};
//  Dome Plate Controller =    {0x02, 0x00, 0x00, 0x00, 0x00, 0x05};



//    MAC Address to broadcast to all senders at once - Mainly Used in my code
  uint8_t broadcastMACAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

//    MAC Address for the Local ESP to use - This prevents having to capture the MAC address of reciever boards.
  uint8_t newLocalMACAddress[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x05};
  uint8_t oldLocalMACAddress[] = {0x24, 0x0A, 0xC4, 0xED, 0x30, 0x15};    //used when connecting to WiFi for OTA

//  // Define variables to store commands to be sent
  String senderID;
  String targetID;
  String command;
  String commandSubString;
  String espnowpassword;
//

//  // Define variables to store incoming commands
  String incomingTargetID;  
  String incomingSenderID;
  String incomingCommand;
  String incomingPassword;
//    
//  // Variable to store if sending data was successful
  String success;
//
//  //Structure example to send data
//  //Must match the receiver structure
  typedef struct struct_message {
      char structPassword[20];
      char structSenderID[2];
      char structTargetID[2];
      char structCommand[100];
  } struct_message;
//
//  // Create a struct_message calledcommandsTosend to hold variables that will be sent
  struct_message commandsToSendtoDome;
  struct_message commandsToSendtoPeriscope;
  struct_message commandsToSendtoBroadcast;

// Create a struct_message to hold incoming commands from the Dome
  struct_message commandsToReceiveFromDome;
  struct_message commandsToReceiveFromPeriscope;
  struct_message commandsToReceiveFromBroadcast;

  esp_now_peer_info_t peerInfo;

//  // Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (debugflag == 1){
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
// void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
//   memcpy(&commandsToReceiveFromBroadcast, incomingData, sizeof(commandsToReceiveFromBroadcast));
//   incomingTargetID = commandsToReceiveFromBroadcast.structTargetID;
//   incomingSenderID = commandsToReceiveFromBroadcast.structSenderID;
//   incomingCommand = commandsToReceiveFromBroadcast.structCommand;
//   DBG("Bytes received from ESP-NOW Message: %i\n", len);
//   DBG("Sender ID = %s\n",incomingSenderID);
//   DBG("Target ID= %s\n", incomingTargetID);
//   DBG("Command = %s\n" , incomingCommand.c_str());  
//   sendESPNOWCommand("BS","PC-ONLINE");
//   if (incomingDestinationID == "Periscope"){
//     DBG("ESP-NOW Command Accepted\n");
//     DBG("Target ID= %s\n",incomingTargetID);
//     if (incomingTargetID == "PL"){
//         DBG("Sending out plSerial\n");
//         writePlSerial(incomingCommand);
//     } else if (incomingTargetID == "FU"){
//         DBG("Sending out fuSerial\n");
//         writeFuSerial(incomingCommand);
//     } else if (incomingTargetID == "PC"){
//         DBG("Execute Local Command = \n", incomingCommand);
//         if (incomingCommand == "Status"){
//           DBG("Status is good\n");                                                                                                                                       
//           sendESPNOWCommand("BS","PC-ONLINE");
//         }else if(incomingCommand != "Status"){
//         inputString = incomingCommand;
//         stringComplete = true; 
//         }
//       }
//      else {
//         DBG("Wrong Target ID Sent\n");
//       }
//   }
//   else {DBG("ESP-NOW Message Ignored\n");}
// }
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&commandsToReceiveFromBroadcast, incomingData, sizeof(commandsToReceiveFromBroadcast));
  incomingPassword = commandsToReceiveFromBroadcast.structPassword;
  if (incomingPassword != ESPNOWPASSWORD){
  DBG_ESPNOW("Wrong ESP-NOW Password was sent.  Message Ignored\n");  
  } else {
  incomingTargetID = commandsToReceiveFromBroadcast.structTargetID;
    if (incomingTargetID == "DP"){
      incomingSenderID = commandsToReceiveFromBroadcast.structSenderID;
      incomingCommand = commandsToReceiveFromBroadcast.structCommand;
      DBG_ESPNOW("Bytes received from ESP-NOW Message: %i\n", len);
      DBG_ESPNOW("Target ID= %s\n", incomingTargetID);
      DBG_ESPNOW("Sender ID = %s\n",incomingSenderID);
      DBG_ESPNOW("Command = %s\n" , incomingCommand); 
      inputString = incomingCommand;
      stringComplete = true; 
    } else {
        DBG_ESPNOW("ESP-NOW Message Ignored\n");
    }
  }
} 
//////////////////////////////////////////////////////////////////////
///***   WiFi Specific Setup  (Only used when OTA is enabled)   ***///
//////////////////////////////////////////////////////////////////////
  
  //Raspberry Pi              192.168.4.100
  //Body Controller ESP       192.168.4.101
  //ESP-NOW Master ESP        192.168.4.110   (Only used for OTA)
  //Dome Controller ESP       192.168.4.111   (Only used for OTA)  
  //Periscope Controller ESP  192.168.4.112   (Only used for OTA)   ************
  //Remote                    192.168.4.107
  //Developer Laptop          192.168.4.125
  
  // IP Address config of local ESP 
  IPAddress local_IP(192,168,4,112);
  IPAddress subnet(255,255,255,0);
  IPAddress gateway(192,168,4,100);
  
 ////R2 Control Network Details     
  const char* ssid = "R2D2_Control_Network";
  const char* password =  "astromech";
  
  AsyncWebServer server(80);
  


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                               /////
///////                             Serial & ESP-NOW Communication Functions                          /////
///////                                                                                               /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//      /////////////////////////////////////////////////////////
//      ///*****          Serial Event Function          *****///
//      /////////////////////////////////////////////////////////
//      /// This routine is run between loop() runs, so using ///
//      /// delay inside loop can delay response.  Multiple   ///
//      /// bytes of data may be available.                   ///
//      /////////////////////////////////////////////////////////

                                            
void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  DBG("%s\n", inputString);
}
void plSerialEvent() {
  while (plSerial.available()) {
    char inChar = (char)plSerial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  DBG("%s\n", inputString);
}

void fuSerialEvent() {
  while (fuSerial.available()) {
    char inChar = (char)fuSerial.read();
    inputString += inChar;
      if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
        stringComplete = true;            // set a flag so the main loop can do something about it.
      }
  }
  DBG("%s\n", inputString);
}


  //      /////////////////////////////////////////////////////////
  //      ///*****          Serial Write Function          *****///
  //      /////////////////////////////////////////////////////////
  //      /// These functions recieve a string and transmits    ///
  //      /// one character at a time and adds a '/r' to the    ///
  //      /// end of the string.                                ///
  //      /////////////////////////////////////////////////////////

void writeSerialString(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    Serial.write(completeString[i]);
  }
}
void writePlSerial(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    plSerial.write(completeString[i]);
  }
}

void writeFuSerial(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++)
  {
    fuSerial.write(completeString[i]);
  }
}


//////////////////////////////////////////////////////////////////////
///*****             ESP-NOW Functions                        *****///
//////////////////////////////////////////////////////////////////////
// 
void setupSendStruct(struct_message* msg, String pass, String sender, String targetID, String cmd)
{
    snprintf(msg->structPassword, sizeof(msg->structPassword), "%s", pass.c_str());
    snprintf(msg->structSenderID, sizeof(msg->structSenderID), "%s", sender.c_str());
    snprintf(msg->structTargetID, sizeof(msg->structTargetID), "%s", targetID.c_str());
    snprintf(msg->structCommand, sizeof(msg->structCommand), "%s", cmd.c_str());
}

void sendESPNOWCommand(String starget, String scomm){
  String senderID = "DP";     // change to match location (BC, BS, DP, DC, LD)
  setupSendStruct(&commandsToSendtoBroadcast ,ESPNOWPASSWORD, senderID, starget, scomm);
  esp_err_t result = esp_now_send(broadcastMACAddress, (uint8_t *) &commandsToSendtoBroadcast, sizeof(commandsToSendtoBroadcast));
  if (result == ESP_OK) {
    DBG_ESPNOW("Sent the command: %s to ESP-NOW Neighbors\n", scomm.c_str());
  }
  else {
    DBG_ESPNOW("Error sending the data\n");
  }
  ESPNOW_command[0] = '\0';
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

void DBG(const char *format, ...) {
        if (!debugflag)
                return;
        va_list ap;
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
}

void DBG_1(const char *format, ...) {
        if (!debugflag1)
                return;
        va_list ap;
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
}
void DBG_2(const char *format, ...) {
        if (!debugflag1)
                return;
        va_list ap;
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
}

void DBG_ESPNOW(const char *format, ...) {
        if (!debugflag_espnow)
                return;
        va_list ap;
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
}

void DBG_SERVO(const char *format, ...) {
        if (!debugflag_servo)
                return;
        va_list ap;
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
}

void DBG_SERIAL_EVENT(const char *format, ...) {
        if (!debugflag_serial_event)
                return;
        va_list ap;
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
}

void DBG_LOOP(const char *format, ...) {
        if (!debugflag_loop)
                return;
        va_list ap;
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
}

void DBG_HTTP(const char *format, ...) {
        if (!debugflag_loop)
                return;
        va_list ap;
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
}

void DBG_LORA(const char *format, ...) {
        if (!debugflag_loop)
                return;
        va_list ap;
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
}

void toggleDebug(){
  debugflag = !debugflag;
  if (debugflag == 1){
    Serial.println("Debugging Enabled");
    }
  else{
    Serial.println("Debugging Disabled");
  }
    ESP_command[0]   = '\0';
}

void toggleDebug1(){
  debugflag1 = !debugflag1;
  if (debugflag1 == 1){
    Serial.println("Parameter Debugging Enabled");
    }
  else{
    Serial.println("Parameter Debugging Disabled");
  }
    ESP_command[0]    = '\0';
}

void toggleDebug_ESPNOW(){
  debugflag_espnow = !debugflag_espnow;
  if (debugflag_espnow == 1){
    Serial.println("ESP-NOW Debugging Enabled");
    }
  else{
    Serial.println("ESP-NOW Debugging Disabled");
  }
    ESP_command[0]   = '\0';
}

void toggleDebug_Servo(){
  debugflag_servo = !debugflag_servo;
  if (debugflag_servo == 1){
    Serial.println("Servo Debugging Enabled");
    }
  else{
    Serial.println("Servo Debugging Disabled");
  }
    ESP_command[0]   = '\0';
}

void toggleDebug_SerialEvent(){
  debugflag_serial_event = !debugflag_serial_event;
  if (debugflag_serial_event == 1){
    Serial.println("Serial Events Debugging Enabled");
    }
  else{
    Serial.println("Serial Events Debugging Disabled");
  }
    ESP_command[0]   = '\0';
}

void toggleDebug_Loop(){
  debugflag_loop = !debugflag_loop;
  if (debugflag_loop == 1){
    Serial.println("Main Loop Debugging Enabled");
    }
  else{
    Serial.println("Main Loop Debugging Disabled");
  }
    ESP_command[0]   = '\0';
}
void toggleDebug_HTTP(){
  debugflag_http = !debugflag_http;
  if (debugflag_http == 1){
    Serial.println("HTTP Debugging Enabled");
    }
  else{
    Serial.println("HTTP Debugging Disabled");
  }
    ESP_command[0]   = '\0';
}

void toggleDebug_LORA(){
  debugflag_lora = !debugflag_lora;
  if (debugflag_lora == 1){
    Serial.println("LoRa Debugging Enabled");
    }
  else{
    Serial.println("LoRa Debugging Disabled");
  }
    ESP_command[0]   = '\0';
}

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
    request->send(200, "text/plain", "Please go to http://192.168.4.112/update to upload file");
  });
  
  AsyncElegantOTA.begin(&server);    // Start AsyncElegantOTA
  server.begin();

  ESP_command[0]   = '\0';
} 

//////////////////////////////////////////////////////////////////////
///*****    Send Keepalive Messages for Status                *****///
//////////////////////////////////////////////////////////////////////
  // KeepAlive Message to show status on website.
  int keepAliveDuration= 5000;  // 5 seconds
  int keepAliveMillis;

void keepAlive(){
  if (millis() - keepAliveMillis >= keepAliveDuration){
    keepAliveMillis = millis();
    sendESPNOWCommand("RL","PCKA");
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


  Serial.begin(115200);
  plSerial.begin(PL_BAUD_RATE,SERIAL_8N1,RXPL,TXPL);
  fuSerial.begin(FU_BAUD_RATE,SERIAL_8N1,RXFU,TXFU);

  Serial.println("\n\n\n----------------------------------------");
  Serial.print("Booting up the ");Serial.println(HOSTNAME);

  //Reserve the inputStrings
  inputString.reserve(100);                                                              // Reserve 100 bytes for the inputString:
  autoInputString.reserve(100);

  //initialize WiFi for ESP-NOW
  WiFi.mode(WIFI_STA);
  esp_wifi_set_mac(WIFI_IF_STA, &newLocalMACAddress[0]);
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
  memcpy(peerInfo.peer_addr, broadcastMACAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add Broadcast ESP-NOW peer");
    return;
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

    keepAlive();
    // looks for new serial commands (Needed because ESP's do not have an onSerialEvent function)
    if(Serial.available()){serialEvent();}
    if(plSerial.available()){plSerialEvent();}
    if(fuSerial.available()){fuSerialEvent();}

    if (stringComplete) {autoComplete=false;}
    if (stringComplete || autoComplete) {
      if(stringComplete) {inputString.toCharArray(inputBuffer, 100);inputString="";}
      else if (autoComplete) {autoInputString.toCharArray(inputBuffer, 100);autoInputString="";}
      if( inputBuffer[0]=='E' ||        // Command designatore for internal ESP functions
          inputBuffer[0]=='e' ||        // Command designatore for internal ESP functions
          inputBuffer[0]=='N' ||        // Command for Sending ESP-NOW Messages
          inputBuffer[0]=='n' ||        // Command for Sending ESP-NOW Messages
          inputBuffer[0]=='S' ||        // Command for sending Serial Strings out Serial ports
          inputBuffer[0]=='s'           // Command for sending Serial Strings out Serial ports
        ){commandLength = strlen(inputBuffer);                     //  Determines length of command character array.
          DBG("Command Length is: %i\n" , commandLength);
          if(commandLength >= 3) {
            if(inputBuffer[0]=='E' || inputBuffer[0]=='e') {espCommandFunction = (inputBuffer[1]-'0')*10+(inputBuffer[2]-'0');};
            if(inputBuffer[0]=='N' || inputBuffer[0]=='n') {
              for (int i=1; i<=commandLength; i++){
                char inCharRead = inputBuffer[i];
                inputStringCommand += inCharRead;                   // add it to the inputString:
              }
              DBG("\nFull Command Recieved: %s ",inputStringCommand);
              espNowCommandFunctionString = inputStringCommand.substring(0,2);
              espNowCommandFunction = espNowCommandFunctionString.toInt();
              DBG("ESP NOW Command State: %i\n", espNowCommandFunction);
              targetID = inputStringCommand.substring(2,4);
              DBG("Target ID: %s\n", targetID);
              commandSubString = inputStringCommand.substring(4,commandLength);
              DBG("Command to Forward: %s\n", commandSubString);
            }
            if(inputBuffer[0]=='S' || inputBuffer[0]=='s') {
              serialPort =  (inputBuffer[1]-'0')*10+(inputBuffer[2]-'0');
              for (int i=3; i<commandLength-2;i++ ){
                char inCharRead = inputBuffer[i];
                serialStringCommand += inCharRead;  // add it to the inputString:
              }
              DBG("Serial Command: %s to Serial Port: %s\n", serialStringCommand, serialPort);
              if (serialPort == "PL"){
                writePlSerial(serialStringCommand);
              } else if (serialPort == "FU"){
                writeFuSerial(serialStringCommand);
              } else if (serialPort == "PC"){
                inputString = serialStringCommand;
                stringComplete = true; 
              }
              serialStringCommand = "";
              serialPort = "";
            } 
            if(inputBuffer[0]=='E' || inputBuffer[0] == 'e') {
              ESP_command[0]   = '\0';                                                            // Flushes Array
              ESP_command[0] = espCommandFunction;
            }
          }
        }

      ///***  Clear States and Reset for next command.  ***///
        stringComplete =false;
        autoComplete = false;
        inputBuffer[0] = '\0';
      
        // reset Local ESP Command Variables
        int espCommandFunction;

              // reset ESP-NOW Variables
        inputStringCommand = "";
        targetID = "";

      DBG("command Proccessed\n");

    }  

      if(ESP_command[0]){
        switch (ESP_command[0]){
        case 1: Serial.println(HOSTNAME);   
                ESP_command[0]   = '\0';                                                        break;
        case 2: Serial.println("Resetting the ESP in 3 Seconds");
                DelayCall::schedule([] {ESP.restart();}, 3000);
                ESP_command[0]   = '\0';                                                        break;
        case 3: connectWiFi(); break;        
        case 4: break;  //reserved for future use
        case 5: break;  //reserved for future use
        case 6: break;  //reserved for future use
        case 7: break;  //reserved for future use
        case 8: break;  //reserved for future use
        case 9: break;  //reserved for future use
        case 10: toggleDebug();                                                                 break;
        case 11: toggleDebug1();                                                                break;

      }
    }
    if(ESPNOW_command[0]){
      switch(ESPNOW_command[0]){
        case 1: sendESPNOWCommand(tempESPNOWTargetID,commandSubString); break; 
        case 2: break;  //reserved for future use
        case 3: break;  //reserved for future use      
      }
    }
  if(isStartUp) {
      isStartUp = false;
      delay(500);

    }
  }
}