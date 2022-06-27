//#define USE_DEBUG
//#define USE_SERVO_DEBUG

#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <WiFiClient.h>
#include <WiFiAP.h>
#include "esp_wifi.h"
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <esp_now.h>
#include <SoftwareSerial.h>


//reeltwo libaries
#include "ReelTwo.h"
#include "core/DelayCall.h"
#include "ServoDispatchPCA9685.h"
#include "ServoSequencer.h"



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///*****                                                                                                                                                           *****///
///*****                                                     Created by Greg Hulette.  I started with the code from flthymcnsty
///*****                                                                                                                                                           *****///
///*****                                                         So exactly what does this all do.....?                                                            *****///
///*****                       - Controls the Dome servos                                                                                     *****///
///*****                       - Controls the Camera Light                                                                                     *****///
///*****                       - Sends Serial commands to the HPs and RSeries                                   *****///
///*****                                                                                                                                                           *****///
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  String inputString;         // a string to hold incoming data
  volatile boolean stringComplete  = false;      // whether the serial string is complete

/////////////////////////////////////////////////////////////////////////
///*****              ESP NOW Set Up                       *****///
/////////////////////////////////////////////////////////////////////////

  // REPLACE WITH THE MAC Address of your receiver 
//    MAC Address of your receivers 
    uint8_t bodyPeerMACAddress[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    uint8_t periscopePeerMACAddress[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x03};

//    MAC Address to broadcast to all senders at once
    uint8_t broadcastMACAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

//    MAC Address for the Local ESP to use - This prevents having to capture the MAC address of reciever boards.
    uint8_t newLocalMACAddress[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};

  // Define variables to store commands to be sent
    String senderID;
    String destinationID;
    String targetID;
    String command;

  // Define variables to store incoming commands
    String incomingDestinationID;
    String incomingTargetID;  
    String incomingSenderID;
    String incomingCommand;
    
  // Variable to store if sending data was successful
    String success;

  //Structure example to send data
  //Must match the receiver structure
    typedef struct struct_message {
        String structSenderID;
        String structDestinationID;
        String structTargetID;  
        String structCommand;
    } struct_message;

  // Create a struct_message calledcommandsTosend to hold variables that will be sent
    struct_message commandsToSendtoBody;
    struct_message commandsToSendtoPeriscope;
    struct_message commandsToSendtoBroadcast;

  // Create a struct_message to hold incoming commands from the Body
    struct_message commandsToReceiveFromBody;
    struct_message commandsToReceiveFromPeriscope;
    struct_message commandsToReceiveFromBroadcast;

    esp_now_peer_info_t peerInfo;

  // Callback when data is sent
    void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
      char macStr[18];
      Serial.print("Packet to: ");
      // Copies the sender mac address to a string
      snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
               mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
      Serial.print(macStr);
      Serial.print(" send status:\t");
      Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    }

  // Callback when data is received
      void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
      memcpy(&commandsToReceiveFromBroadcast, incomingData, sizeof(commandsToReceiveFromBroadcast));
      Serial.print("Bytes received from Dome: ");
      Serial.println(len);
      incomingDestinationID = commandsToReceiveFromBroadcast.structDestinationID;
      incomingTargetID = commandsToReceiveFromBroadcast.structTargetID;
      incomingSenderID = commandsToReceiveFromBroadcast.structSenderID;
      incomingCommand = commandsToReceiveFromBroadcast.structCommand;
      Serial.print("Sender ID = ");
      Serial.println(incomingSenderID);
      Serial.print("Destination ID= ");
      Serial.println(incomingDestinationID);
     
      if (incomingDestinationID =="Dome"){
        Serial.println("Accepted");
        Serial.print("Target ID= ");
        Serial.println(incomingTargetID);
        if (incomingTargetID == "DS"){
        Serial.print("Execute Local Command = ");
        Serial.println(incomingCommand); 
        inputString = incomingCommand;
        stringComplete = true;   
        } else if (incomingTargetID == "RS"){
          Serial.println("Sending out rsSerial");
          writeRSSerial(incomingCommand);
          
        } else if (incomingTargetID == "HP"){
          Serial.println("Sending out hpSerial");
          writeHPSerial(incomingCommand);
          
        }
        
      } else {Serial.println("Ignored");}
  
        
    }
 

/////////////////////////////////////////////////////////////////////////
///*****              ReelTwo Servo Set Up                       *****///
/////////////////////////////////////////////////////////////////////////

#define SMALL_PANEL_ONE       0x0001 //b0000000001
#define SMALL_PANEL_TWO       0x0002 //b0000000010
#define SMALL_PANEL_THREE     0x0004 //b0000000100
#define MEDIUM_PANEL_PAINTED  0x0008 //b0000001000
#define MEDIUM_PANEL_SILVER   0x0010 //b0000010000
#define BIG_PANEL             0x0020 //b0000100000
#define PIE_PANEL_ONE         0x0040 //b0001000000
#define PIE_PANEL_TWO         0x0080 //b0010000000
#define PIE_PANEL_THREE       0x0100 //b0100000000
#define PIE_PANEL_FOUR        0x0200 //b1000000000

#define SMALL_PANELS_MASK     (SMALL_PANEL_ONE|SMALL_PANEL_TWO|SMALL_PANEL_THREE)
#define MEDIUM_PANELS_MASK    (MEDIUM_PANEL_PAINTED|MEDIUM_PANEL_SILVER)
#define DOME_PANELS_MASK      (SMALL_PANELS_MASK|MEDIUM_PANELS_MASK|BIG_PANEL)
#define PIE_PANELS_MASK       (PIE_PANEL_ONE|PIE_PANEL_TWO|PIE_PANEL_THREE|PIE_PANEL_FOUR)
#define ALL_DOME_PANELS_MASK  (DOME_PANELS_MASK|PIE_PANELS_MASK)

// Group ID is used by the ServoSequencer and some ServoDispatch functions to
// identify a group of servos.

//     Pin  Min, ,Max,  Group ID
const ServoSettings servoSettings[] PROGMEM = {
     { 1,  600, 2400, SMALL_PANEL_ONE },       /* 0: door 1 small left door by radar eye */
     { 2,  600, 2400, SMALL_PANEL_TWO },       /* 1: door 2 small middle door by radar eye */
     { 3,  600, 2400, SMALL_PANEL_THREE },     /* 2: door 3 small right door by radar eye */
     { 4,  600, 2400, MEDIUM_PANEL_PAINTED },  /* 3: door 4 medium painted door */
     { 5,  600, 2400, MEDIUM_PANEL_SILVER },   /* 4: door 5 Medium Unpainted door*/
     { 6,  600, 2400, BIG_PANEL },             /* 5: door 6 Big Lower door */
     { 7,  600, 2400, PIE_PANEL_ONE },         /* 6: door 7 Pie Panel near Periscope */
     { 8,  600, 2400, PIE_PANEL_TWO },         /* 7: door 8 Pie Panel clockwise from Periscope*/
     { 9,  600, 2400, PIE_PANEL_THREE },       /* 8: door 9 Pie Panel clockwise-2 from Periscope */
     { 10,  600, 2400, PIE_PANEL_FOUR }        /* 9: door 10 Pie Panel clockwise-3 from Periscope */
};

ServoDispatchPCA9685<SizeOfArray(servoSettings)> servoDispatch(servoSettings);
ServoSequencer servoSequencer(servoDispatch);

//////////////////////////////////////////////////////////////////////
///*****        Command Varaiables, Containers & Flags        *****///
//////////////////////////////////////////////////////////////////////

  char inputBuffer[10];
  String autoInputString;         // a string to hold incoming data
  volatile boolean autoComplete    = false;    // whether an Auto command is setA
  int displayState;
  int typeState;
  int commandLength;
  int paramVar = 9;  

  uint32_t ESP_command[6]  = {0,0,0,0,0,0};
  int commandState     = 0;
  
//////////////////////////////////////////////////////////////////////
///*****   Door Values, Containers, Flags & Timers   *****///
//////////////////////////////////////////////////////////////////////
   int door = -1;
  // Door Command Container
   uint32_t D_command[6]  = {0,0,0,0,0,0};
   int doorState     = 0;
  // Door Counters
//   long int Dcounts[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
//   long int Dcount  = 0;
//   long int D1count  = 0;
//   long int Dpcount = 0;
//   long int qwDuration = 800;
  // Door Timer
//   unsigned long Dmillis;
//   unsigned long Doorsmillis[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
//   unsigned long D1millis;
//   unsigned long Doors1millis[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
//   unsigned long D2millis;
//   unsigned long Doors2millis[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
//  // Door Flags
//   boolean DaltToggle = true;
//   boolean DWToggle   = false;
//   boolean GaltToggle = true;

//////////////////////////////////////////////////////////////////////
///*****       Startup and Loop Variables                     *****///
//////////////////////////////////////////////////////////////////////
  
  boolean startUp = true;
  boolean isStartUp = true;
  
  unsigned long mainLoopTime; // We keep track of the "Main Loop time" in this variable.
  unsigned long MLMillis;
  byte mainLoopDelayVar = 5;

//////////////////////////////////////////////////////////////////////
///*****             Camera Lens Variables and settings       *****///
//////////////////////////////////////////////////////////////////////
  
  unsigned long loopTime; // We keep track of the "time" in this variable.

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

  #define NUM_CAMERA_PIXELS 12
  #define CAMERA_LENS_DATA_PIN 12
  //#define CAMERA_LENS_CLOCK_PIN 13
  int dim = 75;
  unsigned long CLMillis;
  byte CLSpeed = 50;
  
  byte CL_command[4] = {0,0,0,0};
  
  int colorState1;
  int colorState2;

  // Set some primary and secondary default color values as a fall back in case no colors
  // are provided in input commands. This makes the ssytem much more user friendly.

    byte defaultPrimaryColorInt     = 5;          //1 Integer color value from list above
    byte defaultSecondaryColorInt   = 1;          //5 Integer color value from list above

  Adafruit_NeoPixel stripCL = Adafruit_NeoPixel(NUM_CAMERA_PIXELS, CAMERA_LENS_DATA_PIN, NEO_GRB + NEO_KHZ800);

  boolean countUp=false;

  //////////////////////////////////////////////////////////////////////
  ///******       Serial Ports Specific Setup                   *****///
  //////////////////////////////////////////////////////////////////////
  
  #define RXHP 19
  #define TXHP 18 
  #define RXRS 25
  #define TXRS 27 
  #define BAUD_RATE 9600
  SoftwareSerial hpSerial;
  SoftwareSerial rsSerial;
  
  //////////////////////////////////////////////////////////////////////
  ///******             WiFi Specific Setup                     *****///
  //////////////////////////////////////////////////////////////////////
  
  //Raspberry Pi              192.168.4.100
  //Body Controller ESP       192.168.4.101  
  //Dome Controller ESP       192.168.4.102  ************
  //Periscope Controller ESP  192.168.4.103
  //Stealth Controller ESP    192.168.4.104  (Probably not going to be different then Body Controller ESP IP)
  //Dome Servo Controller     192.168.4.105  (Probably not going to be different then Dome Controller ESP IP)
  //Body Servo Controller     192.168.4.106  (Probably not going to be different then Body Controller ESP IP)
  //Remote                    192.168.4.107
  //Developer Laptop          192.168.4.125
  
  // IP Address config of local ESP
  IPAddress local_IP(192,168,4,102);
  IPAddress subnet(255,255,255,0);
  IPAddress gateway(192,168,4,100);
  
//uint8_t newMACAddress[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};

   ////R2 Control Network Details
  const char* ssid = "R2D2_Control_Network";
  const char* password =  "astromech";
  
  AsyncWebServer server(80);
  


void setup()
{

    //***  COMMUNICATION SET UP ***///
   Serial.begin(9600);                                                                   // Initialize Serial Connection at 9600:
   hpSerial.begin(BAUD_RATE,SWSERIAL_8N1,RXHP,TXHP,false,95);
   rsSerial.begin(BAUD_RATE,SWSERIAL_8N1,RXRS,TXRS,false,95);
   Wire.begin();                                                               // Start I2C Bus as Slave I2C Address
  Serial.println("\n\n\n----------------------------------------");
  Serial.println("Booting up the Dome Controller");

   SetupEvent::ready();

   inputString.reserve(20);                                                              // Reserve 100 bytes for the inputString:
   autoInputString.reserve(20);



   //***     RANDOM SET UP     ***///
   randomSeed(millis());                         //  Sets a Random Seed so Random is More Randomy

  stripCL.begin();
  stripCL.show(); // Initialize all pixels to 'off'
  colorWipe(red, 255); // red during bootup
  Serial.println("LED Setup Complete");
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
  
  // Register peer
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
//  peerInfo.ifidx=WIFI_IF_AP;

  // Add peers  
    memcpy(peerInfo.peer_addr, bodyPeerMACAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add Body ESP-NOW peer");
      return;
    }

    memcpy(peerInfo.peer_addr, periscopePeerMACAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add Periscope ESP-NOW peer");
      return;
    }
    memcpy(peerInfo.peer_addr, broadcastMACAddress, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add Broadcast ESP-NOW peer");
      return;
    }  
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);

}  // end of Setup

//
void loop() {
if (millis() - MLMillis >= mainLoopDelayVar){
        MLMillis = millis();
  loopTime = millis();
   AnimatedEvent::process();
//   mainLoop();
 if(startUp) {
      closeAllDoors();
      startUp = false;
      Serial.println("Startup");
   }
   if(Serial.available()){
   serialEvent();
   }
   cameraLED(blue, 5); // blue
//       Serial.println("looping");

//
  if (stringComplete) {autoComplete=false;}
  if (stringComplete || autoComplete) {
    if(stringComplete) {inputString.toCharArray(inputBuffer, 10);inputString="";}
     else if (autoComplete) {autoInputString.toCharArray(inputBuffer, 10);autoInputString="";}
//     if(inputBuffer[0]=='S'  || inputBuffer[0]=='s') {inputBuffer[0]='E' || inputBuffer[0]=='e';}
     if( inputBuffer[0]=='D' ||        // Door Designator
         inputBuffer[0]=='d' ||        // Door Designator
         inputBuffer[0]=='R' ||        //Radar Eye LED
         inputBuffer[0]=='r'  ||         //Radar Eye LED
         inputBuffer[0] =='E' ||          // Command designatore for internal ESP functions
         inputBuffer[0] =='e'           // Command designatore for internal ESP functions

         ) {
            commandLength = strlen(inputBuffer);                     //  Determines length of command character array.

            if(commandLength >= 3) {
                if(inputBuffer[0]=='D' || inputBuffer[0]=='d') {doorState = (inputBuffer[1]-'0')*10+(inputBuffer[2]-'0');
//                Serial.println("Here");
                }    
              else if(inputBuffer[0]=='E' || inputBuffer[0]=='e') {
                commandState = (inputBuffer[1]-'0')*10+(inputBuffer[2]-'0');
                  }                                                                               //  Converts 2 Door Sequence Indentifier Characters to Integer
                                                                                                  //  Converts 2 Door Sequence Indentifier Characters to Integer
                else {displayState = (inputBuffer[1]-'0')*10+(inputBuffer[2]-'0');}              //  Converts Sequence character values into an integer.

                if(commandLength >= 4) {
                  if(inputBuffer[0]=='D' || inputBuffer[0]=='d' || inputBuffer[0]=='R' || inputBuffer[0]=='r') {typeState = inputBuffer[3]-'0';}
                }
                else {
                     typeState = -1;
                }

                if(commandLength >= 5) {
                  if(inputBuffer[0]=='D' || inputBuffer[0]=='d') {door = (inputBuffer[3]-'0')*10+(inputBuffer[4]-'0');}
                  else {colorState1 = inputBuffer[4]-'0';} 
                 }
                else {colorState1 = -1;}

                if(colorState1 < 0 || colorState1 > 9) {
                    colorState1 = defaultPrimaryColorInt;
                 }
                if(commandLength >= 6) {colorState2 = inputBuffer[5]-'0';}


                else {colorState2 = -1;}

                if(colorState2 < 0 || colorState2 > 9) {
                     colorState2 = defaultSecondaryColorInt;
                 }

               
                if(inputBuffer[0]=='D' || inputBuffer[0]=='d') {
                  D_command[0]   = '\0';                                                            // Flushes Array
                  D_command[0] = doorState;
                  if(door>=0) {
                     D_command[1] = door;
                     Serial.println(door);
                  }
                }

                if(inputBuffer[0]=='R' || inputBuffer[0]=='r'){
                  CL_command[0]   = '\0';                                                            // Flushes Array
                  CL_command[0] = displayState;
                  CL_command[1] = typeState;
                  CL_command[2] = colorState1;
                  CL_command[3] = colorState2;
                  CLMillis = millis();
                      
                }
                
                if(inputBuffer[0]=='E' || inputBuffer[0] == 'e') {
                  ESP_command[0]   = '\0';                                                            // Flushes Array
                  ESP_command[0] = commandState;
                }
              }
            }


      ///***  Clear States and Reset for next command.  ***///
       stringComplete =false;
       autoComplete = false;
       inputBuffer[0] = '\0';
       int displayState;
       int typeState;
       int speedState;
       int colorState1;
       int colorState2;
       int door = -1;
       int doorState;
//       Serial.println("command taken");

     }


      if(ESP_command[0]){
          switch (ESP_command[0]){
            case 1: Serial.println("Controller: Dome ESP Controller");   
                    ESP_command[0]   = '\0'; break;
            case 2: Serial.println("Resetting the ESP in 5 Seconds");
                    DelayCall::schedule([] {ESP.restart();}, 5000);
                    ESP_command[0]   = '\0'; break;
            case 3: testESPNOW();
          }
        }

  if(D_command[0]) {
       if((D_command[0] == 1 || D_command[0] == 2) && D_command[1] >= 11) {
         //Serial.println("Incorrect Door Value Specified, Command Aborted!");
         D_command[0] = '\0';
//         Serial.println("wrong if");
       }
       else {
         switch (D_command[0]) {
           case 1:  openDoor(D_command[1]);                                             break;
           case 2:  closeDoor(D_command[1]);                                            break;
           case 3:  openAllDoors();                                                     break;
           case 4:  closeAllDoors();                                                    break;
           case 5:  alternateDoors();                                                   break;
           case 6:  cycleDoors();                                                       break;
           case 7:  waveAllDoors();                                                     break;
           case 8:  quickWaveAllDoors();                                                break;
           case 10: allOpenClose();                                                     break;
           case 11: allOpenCloseLong();                                                 break;
           case 12: allFlutter();                                                       break;
           case 13: allOpenCloseRepeat();                                               break;
           case 14: panelWave();                                                     break;
           case 15: panelWaveFast();                                                     break;
           case 16: openCloseWave();                                                     break;
           case 17: marchingAnts();                                                     break;
           case 18: panelAlternate();                                                     break;
           case 19: panelDance();                                                     break;
           case 20: longDisco();                                                     break;
           case 21: longHarlemShake();                                                     break;
//           case 22: ();                                                     break;
//           case 23: ();                                                     break;
//           case 24: ();                                                     break;
//           case 50: testESPNOW();                                                                 break;
           case 98: closeAllDoors();                                                    break;
           case 99: closeAllDoors();                                                    break;
           default: break;
          }
       }
     }
   if(CL_command[0]){
    switch(CL_command[0]){
      case 1: cameraLED(basicColors[CL_command[2]], CL_command[3]);
      }
   }

  if(isStartUp) {
        isStartUp = false;
//        Ucount  = 0;
//        closeArm(BOTH, SLOW);
        delay(500);
//        digitalWrite(OEPIN, HIGH);
    }
}

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                               /////
///////                        Main Loop for Running the System in Normal Mode                        /////
///////                                                                                               /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void mainLoop() {
  
  }

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                               /////
///////                                  End of Main Loop Function                                    /////
///////                                                                                               /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                               /////
///////                                      Radar eye Camera LED Functions                           /////
///////                                                                                               /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void cameraLED(uint32_t color, int CLSpeed){
  int CLRLow = 1;
  int CLRHigh = 50;
       CLSpeed = map(CLSpeed, 0, 9, 1, 250);
      if (millis() - CLMillis >= CLSpeed){
        CLMillis = millis();
      if(countUp == false){                   // check to see if the boolean flag is false.  If false, starting dimming the LEDs
      
          dim=dim - random(CLRLow, CLRHigh);  // set the brightness to current minus a random number between 5 and 40. I think that
                                              //adding a random causes a less smooth transition which makes it look a little better
          colorWipe(color, dim);              // Set the LEDs to the color and brightness using the colorWheel function
          }
      
        if(dim <= 10){                        //Check to see if the brightness is at or below 20.  Modifying the "20" will
                                              //allow the dim variable to go below zero causing the flicker.  The closer you
                                              //set the "20" to zero, the more flickering will happen. I use half the larger
                                              //dim random number to allow a small flicker without being too annoying.
      
           countUp = true;                    // if the dim variable is at or below "20", change the countUp flag to true      
          }
        if(countUp == true){                 // check to see if the boolean flag is true.  If true, starting brightening the LEDs
            dim=dim + random(CLRLow, CLRHigh); // set the brightness to current plus a random number between 5 and 40.  I think that
                                              //adding a random causes a less smooth transition which makes it look a little better
            colorWipe(color, dim);           // Set the LEDs to the color and brightness using the colorWheel function
         }
          if(dim>=250){                       //Check to see if the brightness is at or above 235.  Modifying the "235" will
                                               //allow the dim variable to go above 255 causing the flicker.  The closer you
                                              //set the "235" to 255, the more flickering will happen. I use half the larger
                                              //dim random number to allow a small flicker without being too annoying.
            countUp = false;                  // if the dim variable is at or above "235", change the countUp flag to false
          }
      }
      
  }

  //  Color Changing Function for the Camera Lens LEDs
  void colorWipe(uint32_t c, int brightness) {
    for(uint16_t i=0; i<NUM_CAMERA_PIXELS; i++) {
        stripCL.setBrightness(brightness);
        stripCL.setPixelColor(i, c);
        stripCL.show();
       }
}






///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                               /////
///////                                       Door Functions                                          /////
///////                                                                                               /////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
  void openDoor(int doorpos) {
    Serial.println("Open Specific Door");
       switch (doorpos){
       case 1: Serial.println("Open SMALL_PANEL_ONE");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, SMALL_PANEL_ONE);  break;
       case 2: Serial.println("Open SMALL_PANEL_TWO");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, SMALL_PANEL_TWO);  break;
       case 3: Serial.println("Open SMALL_PANEL_THREE");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, SMALL_PANEL_THREE);break;
       case 4: Serial.println("Open MEDIUM_PANEL_PAINTED");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, MEDIUM_PANEL_PAINTED);  break;
       case 5: Serial.println("Open MEDIUM_PANEL_SILVER");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, MEDIUM_PANEL_SILVER);  break;
       case 6: Serial.println("Open BIG_PANEL");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, BIG_PANEL);  break;
       case 7: Serial.println("Open PIE_PANEL_ONE");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PIE_PANEL_ONE);break;
       case 8: Serial.println("Open PIE_PANEL_TWO");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PIE_PANEL_TWO);  break;
       case 9: Serial.println("Open PIE_PANEL_THREE");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PIE_PANEL_THREE);break;
       case 10: Serial.println("Open PIE_PANEL_FOUR");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PIE_PANEL_FOUR);  break;

        }
     D_command[0]   = '\0';
  };


  void closeDoor(int doorpos) {
    Serial.println("Close Specific Door");

    switch(doorpos){
      case 1: Serial.println("Close SMALL_PANEL_ONE");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, SMALL_PANEL_ONE);  break;
       case 2: Serial.println("Close SMALL_PANEL_TWO");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, SMALL_PANEL_TWO);  break;
       case 3: Serial.println("Close SMALL_PANEL_THREE");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, SMALL_PANEL_THREE);break;
       case 4: Serial.println("Close MEDIUM_PANEL_PAINTED");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, MEDIUM_PANEL_PAINTED);  break;
       case 5: Serial.println("Close MEDIUM_PANEL_SILVER");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, MEDIUM_PANEL_SILVER);  break;
       case 6: Serial.println("Close BIG_PANEL");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, BIG_PANEL);  break;
       case 7: Serial.println("Close PIE_PANEL_ONE");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PIE_PANEL_ONE);break;
       case 8: Serial.println("Close PIE_PANEL_TWO");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PIE_PANEL_TWO);  break;
       case 9: Serial.println("Close PIE_PANEL_THREE");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PIE_PANEL_THREE);break;
       case 10: Serial.println("Close PIE_PANEL_FOUR");SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PIE_PANEL_FOUR);  break;
    }
   D_command[0]   = '\0';
  }


  void openAllDoors() {
    Serial.println("Open all Doors");
        SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, ALL_DOME_PANELS_MASK);
    D_command[0] = '\0';
   }

  
  void closeAllDoors() {
    Serial.println("Close all doors");
        SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, ALL_DOME_PANELS_MASK);
    D_command[0] = '\0';
  }

  void alternateDoors() {
    Serial.println("Alternate All Doors");
    D_command[0]   = '\0';
  }

  void cycleDoors() {
    Serial.println("Cycle All Doors");
    D_command[0]   = '\0';
  }

  void waveAllDoors() {
    Serial.println("Open Doors 1 at a time");
    D_command[0]   = '\0';
  }

  void waveAllDoorsClose() {
    Serial.println("Close Doors 1 at a time");
    D_command[0]   = '\0';
  }

  void quickWaveAllDoors() {
    Serial.println("Open Doors 1 at a time");
    D_command[0]   = '\0';
  }
 

//
void shortCircuit(int count) {

}



  //////////////  Functions to call ReelTwo animations

  void allOpenClose(){
      Serial.println("Open and Close All Doors");
      SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpenClose, ALL_DOME_PANELS_MASK);
//      DelayCall::schedule([] {sendESPNOWCommand("ESP","d10");}, 4000);
 D_command[0]   = '\0';                                           
      }
      
  void allOpenCloseLong(){
      Serial.println("Open and Close Doors Long");
      SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpenCloseLong, ALL_DOME_PANELS_MASK);
//      DelayCall::schedule([] {sendESPNOWCommand("ESP","d11");}, 4000);

      D_command[0]   = '\0';                                                 
      }
          
  void allFlutter(){
      Serial.println("Flutter All Doors");
      SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllFlutter, ALL_DOME_PANELS_MASK);
      D_command[0]   = '\0';   
      }
  void allOpenCloseRepeat(){
      Serial.println("Open and Close All Doors Repeat");
      SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllFOpenCloseRepeat, ALL_DOME_PANELS_MASK);
      D_command[0]   = '\0';             
             }
  void panelWave(){
       Serial.println("Wave");
       SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelWave, ALL_DOME_PANELS_MASK);
       D_command[0]   = '\0';                                             
       }
  void panelWaveFast(){
       Serial.println("Wave Fast");
       SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelWaveFast, ALL_DOME_PANELS_MASK);
       D_command[0]   = '\0';                                             
       }
  void openCloseWave() {
       Serial.println("Open Close Wave ");
       SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelOpenCloseWave, ALL_DOME_PANELS_MASK);
       D_command[0]   = '\0';                                             
       }                                          
 
  void marchingAnts() {
       Serial.println("Marching Ants");
       SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelMarchingAnts, ALL_DOME_PANELS_MASK);
       D_command[0]   = '\0';                                             
       }                                             
  void panelAlternate() {
       Serial.println("Panel Alternate");
       SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAlternate, ALL_DOME_PANELS_MASK);
       D_command[0]   = '\0';                                             
       }                                                            

  void panelDance() {
       Serial.println("Panel Dance");
       SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelDance, ALL_DOME_PANELS_MASK);
       D_command[0]   = '\0';                                             
       }

  void longDisco() {
         Serial.println("Panel Dance");
         SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelLongDisco, ALL_DOME_PANELS_MASK);
         D_command[0]   = '\0';                                             
         }

  void longHarlemShake() {
         Serial.println("Panel Dance");
         SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelLongHarlemShake, ALL_DOME_PANELS_MASK);
         D_command[0]   = '\0';                                             
         }                                                       
                                                     


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////                                                                                               /////
///////                             Serial & I2C Communication Functions                              /////
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
//
      void serialEvent() {
        //int count = 0;
       while (Serial.available()) {
          // get the new byte:
          char inChar = (char)Serial.read();
          // add it to the inputString:
         inputString += inChar;
          if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
            stringComplete = true;            // set a flag so the main loop can do something about it.
          }

        }
               Serial.println(inputString);
      }

      void writeString(String stringData){
        String completeString = stringData + '\r';
        for (int i=0; i<completeString.length(); i++)
        {
          Serial.write(completeString[i]);
        }
      }
      void writeString1(String stringData){
        String completeString = stringData + '\r';
        for (int i=0; i<completeString.length(); i++)
        {
          Serial1.write(completeString[i]);
        }
      }
      
      void writeString2(String stringData){
        String completeString = stringData + '\r';
        for (int i=0; i<completeString.length(); i++)
        {
          Serial2.write(completeString[i]);
        }
      }
      void writeRSSerial(String stringData){
        String completeString = stringData + '\r';
        for (int i=0; i<completeString.length(); i++)
        {
          rsSerial.write(completeString[i]);
        }
        Serial.println("Printing to rsSerial");
      }

      void writeHPSerial(String stringData){
        String completeString = stringData + '\r';
        for (int i=0; i<completeString.length(); i++)
        {
          hpSerial.write(completeString[i]);
        }
        Serial.println("Printing to hpSerial");
      }

      //////////////////////////////////////////////////////////////////////
///*****             ESP-NOW Functions                        *****///
//////////////////////////////////////////////////////////////////////
 
  void sendESPNOWBodyCommand(String sdest,String scomm){
//    Serial.println("sendESPNOWCommand Function called");
//    destinationID = sdest;
//    command = scomm;
   commandsToSendtoBroadcast.structDestinationID = sdest;
    commandsToSendtoBroadcast.structSenderID = "Dome";
    commandsToSendtoBroadcast.structCommand = scomm;
    // Send message via ESP-NOW
    esp_err_t result = esp_now_send(broadcastMACAddress, (uint8_t *) &commandsToSendtoBroadcast, sizeof(commandsToSendtoBroadcast));
   if (result == ESP_OK) {
    Serial.println("Sent with success");
    }
    else {
      Serial.println(result);
      Serial.println("Error sending the data");
    }
   }

  void parseESPNOWCommand(String idest, String icomm){
    
  }

  void testESPNOW(){
    sendESPNOWBodyCommand("ESP","d03");
//    Serial.println("testESPNOW Function called");
    ESP_command[0] = '\0';

  }
