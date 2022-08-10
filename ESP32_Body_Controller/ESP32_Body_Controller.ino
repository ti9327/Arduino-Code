// Used for OTA
#include "ESPAsyncWebServer.h"
#include <AsyncElegantOTA.h>
#include <elegantWebpage.h>
#include <Hash.h>

//Used for WiFi
#include "esp_wifi.h"

//Used for PC9685 - Servo Expansion Board
#include <Wire.h>

// Used for Software Serial to allow more useful naming
#include <SoftwareSerial.h>

//ReelTwo libaries
//#define USE_DEBUG
//#define USE_SERVO_DEBUG
#include "ReelTwo.h"
#include "core/DelayCall.h"
#include "ServoDispatchPCA9685.h"
#include "ServoSequencer.h"
#include "core/Animation.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///*****                                                                                                       *****///
///*****                            Created by Greg Hulette.                                                   *****///
///*****                                                                                                       *****///
///*****   I started with the code from flthymcnsty from from which I used the basic command structure and     *****///
///*****  serial input method.  This code also relies on the ReelTwo library for all it's servo movements.     *****///
///*****                                                                                                       *****///
///*****                                     So exactly what does this all do.....?                            *****///
///*****                       - Controls the Body servos                                                      *****///
///*****                       - Creates the WiFI network                                                      *****///
///*****                       - Sends Serial commands to the LED Controller and the Stealth Board             *****///
///*****                       - Sends Serial commands to the ESP-NOW Master to relay to other Controllers     *****///
///*****                                                                                                       *****///
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
///*****        Command Varaiables, Containers & Flags        *****///
//////////////////////////////////////////////////////////////////////
    
  char inputBuffer[100];
  String inputString;         // a string to hold incoming data
  volatile boolean stringComplete  = false;      // whether the serial string is complete
  String autoInputString;         // a string to hold incoming data
  volatile boolean autoComplete    = false;    // whether an Auto command is setA

  int commandLength;
  int paramVar = 9;
  
  String serialPort;
  String serialStringCommand;
  String serialSubStringCommand;
  int mp3Track;
  String mp3Comm;
  
  uint32_t ESP_command[6]  = {0,0,0,0,0,0};
  int espCommandFunction     = 0;

  int debugflag = 1;
  int debugflag1 = 0;  // Used for debugging params recieved from clients

  
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

  #define RXEN 15
  #define TXEN 16 
  #define RXBL 25
  #define TXBL 26
  #define RXST 12
  #define TXST 14
  #define RXMP 17
  #define TXMP 18
  
  #define enSerial Serial1
  #define blSerial Serial2
  SoftwareSerial stSerial;
  SoftwareSerial mpSerial;

  #define EN_BAUD_RATE 115200
  #define BL_BAUD_RATE 115200
  #define ST_BAUD_RATE 9600
  #define MP_BAUD_RATE 9600



  //////////////////////////////////////////////////////////////////////
  ///******      Arduino Mega Reset Pin Specific Setup          *****///
  //////////////////////////////////////////////////////////////////////

  #define RST 4

  //////////////////////////////////////////////////////////////////////
  ///******             WiFi Specific Setup                     *****///
  //////////////////////////////////////////////////////////////////////

//Raspberry Pi              192.168.4.100
//Body Controller ESP       192.168.4.101   ************
//ESP-NOW Master ESP        192.168.4.110   (Only used for OTA)
//Dome Controller ESP       192.168.4.111   (Only used for OTA) 
//Periscope Controller ESP  192.168.4.112   (Only used for OTA)
//Remote                    192.168.4.107
//Developer Laptop          192.168.4.125

// IP Address config of local ESP
IPAddress local_IP(192,168,4,101);
IPAddress subnet(255,255,255,0);
IPAddress gateway(192,168,4,100);
uint8_t newMACAddress[] = {0x02, 0x00, 0xC0, 0xA8, 0x04, 0x65};

 ////R2 Control Network Details
const char* ssid = "R2D2_Control_Network";
const char* password =  "astromech";
int channel =  6;
int broadcastSSID = 0;  //0 for yes, 1 for no
int maxConnections = 8;

AsyncWebServer server(80);

void setup(){
  //Initialize the Serial Ports
  Serial.begin(115200);
  enSerial.begin(EN_BAUD_RATE,SERIAL_8N1,RXEN,TXEN);
  blSerial.begin(BL_BAUD_RATE,SERIAL_8N1,RXBL,TXBL);
  stSerial.begin(ST_BAUD_RATE,SWSERIAL_8N1,RXST,TXST,false,95);
  mpSerial.begin(MP_BAUD_RATE,SWSERIAL_8N1,RXMP,TXMP,false,95);

  Serial.println("\n\n\n----------------------------------------");
  Serial.println("Booting up the Body ESP Controller");
  
  //Configure the Reset Pins for the arduinoReset() function
  pinMode(4, OUTPUT);
  digitalWrite(4,HIGH);

  //Initialize I2C for the Servo Expander Board
  Wire.begin();
  
  //Initialize the ReelTwo Library
  SetupEvent::ready();

  //Reserve the inputStrings
  inputString.reserve(100);                                                              // Reserve 100 bytes for the inputString:
  autoInputString.reserve(100);

  //Initialize the Soft Access Point
  WiFi.mode(WIFI_AP);
  Serial.println(WiFi.softAP(ssid,password,channel,broadcastSSID,maxConnections) ? "AP Ready" : "Failed!");
  delay(200);
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "AP IP Configured" : "Failed!");
  delay(200);
  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());

  esp_wifi_set_mac(WIFI_IF_AP, &newMACAddress[0]);
  delay(2000);
  Serial.print("Local AP MAC address = ");
  Serial.println(WiFi.softAPmacAddress());
 
 //Setup the webpage and accept the GET requests, and parses the variables 
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      
    int paramsNr = request->params();               // Gets the number of parameters sent
//    DBG("Parameter %i \n",paramsNr);                       // Variable for selecting which Serial port to send out
    for(int i=0;i<paramsNr;i++){                     //Loops through all the paramaters
      AsyncWebParameter* p = request->getParam(i);

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////                                                                //////////////////////////        
//////////  These If statements choose where to send the commands         //////////////////////////
//////////  This way we can control multiple serial ports from one ESP32. //////////////////////////
//////////                                                                //////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
        
    if ((p->name())== "param0" & (p->value()) == "Serial0"){
        DBG_1("Serial0 Chosen with If Statement\n");
        paramVar = 0;
        };
    if ((p->name())== "param0" & (p->value()) == "enSerial"){
        DBG_1("Serial 1 Chosen with If Statement\n");
        paramVar = 1;
        };
    if ((p->name())== "param0" & (p->value()) == "blSerial"){
      DBG_1("Serial 2 Chosen with If Statement\n");
          paramVar = 2;
    };
        if ((p->name())== "param0" & (p->value()) == "stSerial"){
          DBG_1("Serial 2 Chosen with If Statement\n");
          paramVar = 3;
    };
    if ((p->name())== "param0" & (p->value()) == "ESP"){
          DBG_1("ESP(Self) Chosen with If Statement\n");
          paramVar = 4;
    };
    if ((p->name())== "param0" & (p->value()) == "ArduinoReset"){
        DBG_1("Reset Only Arduino Chosen with If Statement\n");
          resetArduino(500);
        };
    if ((p->name())== "param0" & (p->value()) == "ESPReset"){
        DBG_1("Reset ESP and Arduino Chosen with If Statement\n");
        ESP.restart();
        };
        
        DBG_1("Param name: %s\n", (p->name()));
        DBG_1("Param value: %s\n", (p->value()).c_str());
  
        if (paramVar == 0){
          DBG_1("Writing to Serial 0\n");      
          writeSerialString(p->value());
        };
        if (paramVar == 1){
          DBG_1("Writing to enSerial\n"); 
//          delay(100);     
        if ((p->name())== "param0" & (p->value()) == "enSerial"){
            DBG_1("Skipping param 0 in the EspNowSerial Write\n");
          } 
          else {
            writeEnSerial(p->value());
          };
        } ;      
          if (paramVar == 2){
          DBG_1("Writing to blSerial\n");      
          writeBlSerial(p->value());
        };
        if (paramVar == 3){
          DBG_1("Writing to stSerial\n");      
          writeStSerial(p->value());
        };
        if (paramVar == 4){
          DBG_1("Executing on self\n");      
          inputString = (p->value());
          stringComplete = true;  
        };

        DBG_1("------\n");
//        delay(50);
    }

    request->send(200, "text/plain", "Message Received on Body Controller");
  });
  
  //Enable Access-Control-Allow-Origin to mitigate errors from website polling
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  AsyncElegantOTA.begin(&server);    // Start ElegantOTA

  //Initialize the AsycWebServer
  server.begin();

  //Reset Arudino Mega
  resetArduino(500);


}

void loop(){
  if (millis() - MLMillis >= mainLoopDelayVar){
    MLMillis = millis();
    AnimatedEvent::process();
    if(startUp) {
      startUp = false;
      Serial.println("Startup");
      // Play Startup Sound
      mp3Trigger("v",16);
      mp3Trigger("t",1);
      mp3Trigger("v",0);

    }
    if(Serial.available()){serialEvent();}
    if(blSerial.available()){serialBlEvent();}
    if(enSerial.available()){serialEnEvent();}
    if(stSerial.available()){serialStEvent();}
    if(mpSerial.available()){serialMpEvent();}

    
    if (stringComplete) {autoComplete=false;}
    if (stringComplete || autoComplete) {
      if(stringComplete) {inputString.toCharArray(inputBuffer, 100);inputString="";}
      else if (autoComplete) {autoInputString.toCharArray(inputBuffer, 100);autoInputString="";}
      if( inputBuffer[0]=='E'     ||        // Command designatore for internal ESP functions
          inputBuffer[0]=='e'     ||        // Command designatore for internal ESP functions
          inputBuffer[0]=='S'     ||        // Command for sending Serial Strings out Serial ports
          inputBuffer[0]=='s'               // Command for sending Serial Strings out Serial ports
        
        ){commandLength = strlen(inputBuffer);                                                                                  //  Determines length of command character array.

          if(commandLength >= 3) {
            if(inputBuffer[0]=='E' || inputBuffer[0]=='e') {
              espCommandFunction = (inputBuffer[1]-'0')*10+(inputBuffer[2]-'0');
              };      
            if(inputBuffer[0]=='S' || inputBuffer[0]=='s') {
              // serialPort =  (inputBuffer[1]-'0')*10+(inputBuffer[2]-'0');
              for (int i=1; i<commandLength;i++ ){
                char inCharRead = inputBuffer[i];
                serialStringCommand += inCharRead;  // add it to the inputString:
              }
              DBG("Full Serial Command Captured: %s\n", serialStringCommand.c_str());
              serialPort = serialStringCommand.substring(0,2);
              serialSubStringCommand = serialStringCommand.substring(2,commandLength);
               DBG("Serial Command: %s to Serial Port: %s\n", serialSubStringCommand.c_str(), serialPort);
              if (serialPort == "BL"){
                writeBlSerial(serialSubStringCommand);
                DBG("Sending out BL Serial\n");
              } else if (serialPort == "EN"){
                writeEnSerial(serialSubStringCommand);
                DBG("Sending out EN Serial\n");
              } else if (serialPort == "ST"){
                writeStSerial(serialSubStringCommand);
                DBG("Sending out ST Serial\n");
              }else if (serialPort == "MP"){
                mp3Comm = serialStringCommand.substring(2,3);
                mp3Track = (inputBuffer[4]-'0')*100+(inputBuffer[5]-'0')*10+(inputBuffer[6]-'0');
                DBG("Command: %s, Track: %i\n",mp3Comm, mp3Track);
                mp3Trigger(mp3Comm,mp3Track);
                DBG("Sending out MP Serial\n ");
              } else { DBG("No valid Serial identified\n");}
              serialStringCommand = "";
              serialPort = "";
              serialSubStringCommand = "";
              int mp3Track;
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


      DBG("command Proccessed\n");
    }

    if(ESP_command[0]){
      switch (ESP_command[0]){
        case 1: Serial.println("Body ESP Controller");   
                ESP_command[0]   = '\0';                                                        break;
        case 2: Serial.println("Resetting the ESP in 3 Seconds");
                DelayCall::schedule([] {ESP.restart();}, 3000);
                ESP_command[0]   = '\0';                                                        break;
        case 3: break;  //reserved for commonality. Used for connecting to WiFi and enabling OTA on ESP-NOW Boards 
        case 4: break;  //reserved for future use
        case 5: break;  //reserved for future use
        case 6: break;  //reserved for future use
        case 7: break;  //reserved for future use
        case 8: break;  //reserved for future use
        case 9:  break;  //reserved for future use
        case 10: toggleDebug();                                                                 break;
        case 11: toggleDebug1();                                                                break;

      }
    }


    
    if(isStartUp) {
      isStartUp = false;
      delay(500);
    }
  }
}  // end of main loop


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
  DBG("InputString: %s \n",inputString);
};


void serialEnEvent() {
  while (enSerial.available()) {
    // get the new byte:
    char inChar = (char)enSerial.read();
    // add it to the inputString:
    inputString += inChar;
    if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
      stringComplete = true;            // set a flag so the main loop can do something about it.
    };
  };
  DBG("InputString: %s \n",inputString);
};


void serialBlEvent() {
  while (blSerial.available()) {
    // get the new byte:
    char inChar = (char)blSerial.read();
    // add it to the inputString:
    inputString += inChar;
    if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
      stringComplete = true;            // set a flag so the main loop can do something about it.
    };
  };
  DBG("InputString: %s \n",inputString);
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
  DBG("InputString: %s \n",inputString);
};

String mp3TriggerResponseString;

void serialMpEvent() {
  while (mpSerial.available()) {
    char inChar = (char)mpSerial.read();
    mp3TriggerResponseString += inChar;
    if (inChar == '\r') {               // if the incoming character is a carriage return (\r)
    };
  };
  DBG("MP3 Trigger Response: %s \n",mp3TriggerResponseString);
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


void writeEnSerial(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++){
    enSerial.write(completeString[i]);
  };
  DBG("String to Send over ESPNOW Serial: %s \n" , completeString.c_str());
};


void writeStSerial(String stringData){
  String completeString = stringData + '\r';
  for (int i=0; i<completeString.length(); i++){
    stSerial.write(completeString[i]);
  };
};


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


void toggleDebug(){
  debugflag = !debugflag;
  if (debugflag == 1){
    Serial.println("Debugging Enabled \n"); 
    }
  else{
    Serial.println("Debugging Disabled");
  }
    ESP_command[0]   = '\0';
}


void toggleDebug1(){
  debugflag1 = !debugflag1;
  if (debugflag1 == 1){
    Serial.println("Parameter Debugging Enabled \n");
    }
  else{
    Serial.println("Parameter Debugging Disabled\n");
  }
    ESP_command[0]   = '\0';
}


//////////////////////////////////////////////////////////////////////
///*****    Resets Arduino Mega due to bug in my PCB          *****///
//////////////////////////////////////////////////////////////////////

void resetArduino(int delayperiod){
  DBG("Opening of reset function");
  digitalWrite(4,LOW);
  delay(delayperiod);
  digitalWrite(4,HIGH);
  DBG("reset witin function");
}


void mp3Trigger(String comm, int track){
  mpSerial.print(comm);
  mpSerial.write(track);
}
