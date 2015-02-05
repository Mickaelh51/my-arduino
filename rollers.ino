#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <WString.h>
#include <RTClib.h>
#include <Wire.h>
#include "EmonLib.h" // Include Emon Library
EnergyMonitor emon1; // Create an instance


//################################### VARIABLES ###################################


byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFF, 0x11 };
byte ip[] = {***,***,***,***};
byte gateway[] = {***,***,***,***};
byte subnet[] = {255,255,255,0};
byte mqttserver[] = { ***, ***, ***, *** };

EthernetClient ethClient;
PubSubClient MQTTclient (mqttserver, 1883, callback, ethClient);

RTC_DS1307 rtc;

long debounce = 200;   // the debounce time, increase if the output flickers
int ShutterAfter;
float cosphi = 0.928;

//number of line in array or in loop ! (normly is 6)
const int totalnumber = 6;
long TimeSendSensors = 0;

//long now.unixtime();
long TimerSync;
long LastSync = 0;
//long LastSync;

//Ex: {22,24} == {shutter UP in2, shutter DOWN in1}

int PinMatrixShutter[totalnumber][2] = {
   {22,24}, //veranda 1 shutter == Number 1 in shutter box
   
   {26,28}, // veranda 2 shutter == Number 5 in shutter box
   
   {30,32}, // kitchen shutter == Number 2 in shutter box
   
   {34,36}, // lounge shutter == Number 3 in shutter box
    
   {38,40}, // room shutter == Number 4 in shutter box
   
   {42,44} // office shutter == Number 6 in shutter box
};

//List of ID shutter in Mysql DB
int IdShutterDb[totalnumber] = {
   22, //veranda 1 Id DB
   
   23, // veranda 2 Id DB
   
   24, // kitchen Id DB
   
   26, // lounge Id DB
    
   25, // room Id DB
   
   27 // office Id DB
};

//Ex: {31,33} == {button UP, button DOWN}

int PinMatrixSwitch[totalnumber][2] = {
   {31,33}, //veranda 1 switch == Number 1
   
   {35,37}, // veranda 2 switch == number 3
   
   {39,41}, // kitchen switch == number 7
   
   {43,45}, // lounge switch == number 5
    
   {47,49}, // room switch = number 6
   
   {18,19} // office switch == number 2
};
 
boolean StatesPinRelayX[totalnumber][2] = {
   {false,false},
   {false,false},
   {false,false},
   {false,false},
   {false,false},
   {false,false}
};

boolean StatesPinSwitch[totalnumber][2] = {
   {false,false},
   {false,false},
   {false,false},
   {false,false},
   {false,false},
   {false,false}
};

boolean StatesUpRelayX[totalnumber][2] = {
   {false,false},
   {false,false},
   {false,false},
   {false,false},
   {false,false},
   {false,false}
};

String Stateopenhabshutter[totalnumber][2] = {
    {"I_Shutter_Veranda1", "STOP"},
    {"I_Shutter_Veranda2", "STOP"},
    {"I_Shutter_Kitchen", "STOP"},
    {"I_Shutter_Lounge", "STOP"},
    {"I_Shutter_Bedroom", "STOP"},
    {"I_Shutter_Office", "STOP"}
};

long TimeAntiflaping[totalnumber][2] = {
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0}
};

long TimeUpDown[totalnumber][2] = {
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0}
};

long TimeUpRelayX[totalnumber][2] = {
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0}
};

boolean Reading[totalnumber][2] = { 
  {LOW, LOW},
  {LOW, LOW},
  {LOW, LOW},
  {LOW, LOW},
  {LOW, LOW},
  {LOW, LOW}
};

boolean StatePinSwitchLightTerrace = false;
long TimeAntiflapingLTerrace = 0;
long TimeUpRelayLiTerrace = 0;
boolean StateRelayLiTerrace = false;
boolean StateLightTerrace = false;
String ItemLightTerrace = "I_Light_Outdoor_Terrace";
char BuffWatts[30];
char BuffIrms[30];
char message_buff[60];

int NumberSwitch = 0;

//Create to RTC module
DateTime now;

//################################### VOID SETUP ###################################

void setup() {
 Serial.begin(9600);
 
 //for Ethernet start
 Ethernet.begin(mac, ip, gateway, subnet);

 delay(5000);
  
 while (MQTTclient.connect("domo4-mqtt")!= 1)
 {
   Serial.println("In Setup Error connecting to MQTT");
   delay(3000);
 } 
 
  MQTTclient.subscribe("domo4sub");
  
 Wire.begin();
 rtc.begin();
 
 if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(__DATE__, __TIME__));
    rtc.adjust(DateTime(__DATE__, __TIME__));
 }

 //LastSync = now.unixtime;
//rtc.adjust(DateTime(1098575237));
 SyncRTC();
 
 //Curent: input pin, calibration.
 emon1.current(8, 51.5);
  
 
 //INIT//
 
 //init for relais and buttons
 for(int i = 0; i < totalnumber; i++){
   for(int j=0; j<2 ; j++){
     pinMode(PinMatrixShutter[i][j], OUTPUT);
     pinMode(PinMatrixSwitch[i][j], INPUT);
   }
 }
 
 //to force all relais to HIGH
 for(int i = 0; i < totalnumber; i++){
   for(int j=0; j<2 ; j++){
     digitalWrite(PinMatrixShutter[i][j], HIGH);
   }
 }
 
 //to Light Terrace
 pinMode(46,OUTPUT);
 pinMode(17,INPUT);
 digitalWrite(46,HIGH);
 //RESTSEND(ItemLightTerrace, "OFF");
 
 Serial.println("setup complete");

}

//################################### VOID LOOP ###################################

void loop()
{
  
  now = rtc.now();
  
  //if(now.unixtime() - TimerSync >= 3600) { SyncRTC(); }
  
  if (!MQTTclient.connected())
  {
      MQTTclient.connect("domo4-mqtt");
      MQTTclient.subscribe("domo4-sub");
  }
  
  double Irms = emon1.calcIrms(1480); // Calculate Irms only
  float watts = Irms*230.0*cosphi;
 
  //Reset all timer when millis restart to 0 (every 50 days)
  RESETTIMER ();
  
  //Read all shutters buttons
  for(int i = 0; i < totalnumber; i++){
    for(int j=0; j<2 ; j++){
      Reading[i][j] = digitalRead(PinMatrixSwitch[i][j]);
      
      //Serial.print ("button: "); Serial.print(PinMatrixSwitch[i][j]); Serial.print (" is "); Serial.println(Reading[i][j]); delay(500);
      
      //if switch is pushing more of 5s, then we desactivate this relay
      //it's max time, same time at old system (HAGER) = 60 seconds
      if(now.unixtime() - TimeUpDown[i][j] >= 60)
      {
        DownRelayX(i,j);
      }
    }
  }
  
  //After 2s we can do down relay to light terrace, because we have a Telerupteur
  if(now.unixtime() - TimeUpRelayLiTerrace >= 1) { DownRelayLiTerrace(); }
  
  //Read lights terrace button
  boolean PinLightTerrace = digitalRead(17);
  if(PinLightTerrace == HIGH && StatePinSwitchLightTerrace == false && millis() - TimeAntiflapingLTerrace > debounce) 
  { 
    MQTTPublishString(String("PUSH"), "verrandaswitch1");
    TimeAntiflapingLTerrace = millis();
  }
 
 
  //work with result above
  for (int i = 0; i < totalnumber; i++)
  {
    for(int j=0; j<2 ; j++)
    {  
      if(j == 0 ){ ShutterAfter = 1; } else { ShutterAfter = 0; }
      
      if(Reading[i][j] == HIGH && StatesPinSwitch[i][j] == false && StatesUpRelayX[i][ShutterAfter] == false && millis() - TimeAntiflaping[i][j] > debounce)
      {  
        if(StatesPinRelayX[i][j] == false)
        {
           DownRelayX(i,ShutterAfter);
           StatesUpRelayX[i][j] = true;
           TimeUpRelayX[i][j] = now.unixtime();
           if(j == 0){ MQTTPublishString(String("UP"),Stateopenhabshutter[i][0]); } else if (j == 1) { MQTTPublishString(String("DOWN"),Stateopenhabshutter[i][0]); }
        }
        else
        {
           DownRelayX(i,j);
        }
        StatesPinSwitch[i][j] = true;
        TimeAntiflaping[i][j] = millis();
      }
      
      //detect if pushing + 2500 ms and send command to all shutters, if pushing is < 4s, then not enter in IF
      if(Reading[i][j] == HIGH && StatesPinSwitch[i][j] == true && StatesUpRelayX[i][ShutterAfter] == false && millis() - TimeAntiflaping[i][j] > 2500 && millis() - TimeAntiflaping[i][j] < 4000)
      {
          for(int i = 0; i < totalnumber; i++)
          {
            DownRelayX(i,ShutterAfter);
            UpRelayX(i,j);
          }  
      }
      
      //Only UP relay should be delayed, for security reasons. 
      UpRelayXDelayed(i,j);
        
      if(Reading[i][j] == LOW)
      {
        //read state button
        StatesPinSwitch[i][j] = false;
      }
      
    }
  }
  
  // publish sensors results every 30 seconds
  if(MQTTclient.connected())
  {
      if(millis() > (TimeSendSensors + 60000)) {
        TimeSendSensors = millis();
        
        //Send to WATTS
        //sprintf(BuffWatts, "%02d", float(watts));
        dtostrf(watts,4,2,BuffWatts);
        MQTTPublishString(String(BuffWatts),"WattsD4");
        
        //Send to IRMS
        //sprintf(BuffIrms, "%02d", float(Irms));
        dtostrf(Irms,4,2,BuffIrms);
        MQTTPublishString(String(BuffIrms),"IrmsD4");
        
        //Serial.print("irms: "); Serial.print(Irms); Serial.print(" Irms after: "); Serial.println(BuffIrms);
        //Serial.print("watts: "); Serial.print(watts); Serial.print(" Watts after: "); Serial.println(BuffWatts);
      }
  }
  
  // MQTT client loop processing
  MQTTclient.loop();
}


//################################### FUNCTIONS ###################################


void MQTTPublishString (String Status, String Type) {
  char BuffStatus[100];
  char BuffType[100];
  String StringStatus = Status; StringStatus.toCharArray(BuffStatus, StringStatus.length()+1);
  Type.toCharArray(BuffType, Type.length()+1);
  MQTTclient.publish(BuffType, BuffStatus);
}

void UpRelayX(int i, int j)
{ 
    String LocalState;
    digitalWrite(PinMatrixShutter[i][j], LOW); 
    TimeUpDown[i][j] = now.unixtime();
    StatesPinRelayX[i][j] = true;
}

void UpRelayXDelayed(int i, int j)
{ 
  if (StatesUpRelayX[i][j] == true && now.unixtime() - TimeUpRelayX[i][j] >= 1)
  {
     UpRelayX(i,j);
     StatesUpRelayX[i][j] = false;
  }
}

void DownRelayX(int i, int j)
{
   digitalWrite(PinMatrixShutter[i][j], HIGH); 
   StatesPinRelayX[i][j] = false; 
}

void ShuttersCommand(int i, int j)
{
  if(j == 0 ){ ShutterAfter = 1; } else { ShutterAfter = 0; }
  
  if(StatesPinRelayX[i][j] == false)
  {
    DownRelayX(i,ShutterAfter);
    StatesUpRelayX[i][j] = true;
    TimeUpRelayX[i][j] = now.unixtime();
  }
  else
  {
    DownRelayX(i,j);
  }
}

void ShuttersCommandAll(int j)
{
  
  if(j == 0 ){ ShutterAfter = 1; } else { ShutterAfter = 0; }
  
  for(int i = 0; i < totalnumber; i++)
  {
    if(StatesPinRelayX[i][j] == false)
    {
      DownRelayX(i,ShutterAfter);
      StatesUpRelayX[i][j] = true;
      TimeUpRelayX[i][j] = now.unixtime();
    }
    else
    {
      DownRelayX(i,j);
    }
  } 
}

void RESETTIMER ()
{
    long subtimer = millis();
    
    if(subtimer < 1000)
    {
      TimeAntiflapingLTerrace = 0;
      TimeSendSensors = 0;
      
      for(int i = 0; i < totalnumber; i++){
        for(int j=0; j<2 ; j++){
          TimeAntiflaping[i][j] = 0;
        }
      }
      
    }      
}

void DownRelayLiTerrace()
{
  if(StateRelayLiTerrace == true) { digitalWrite(46,HIGH); StateRelayLiTerrace = false; }
}

void CommandLightTerrace()
{
  digitalWrite(46,LOW);
  StateRelayLiTerrace = true;
  TimeUpRelayLiTerrace = now.unixtime();
  if(StateLightTerrace == false) { StateLightTerrace = true; } else { StateLightTerrace = false; }
}

void SyncRTC()
{
  //Uninitialized
  String ResponseUnixTime; //= RESTGET("I_Date2");
  if(ResponseUnixTime != "Uninitialized") { rtc.adjust(DateTime(ResponseUnixTime.toInt()+3600)); LastSync = now.unixtime(); }
  TimerSync = now.unixtime();
} 


// handles message arrived on subscribed topic(s)
void callback(char* topic, byte* payload, unsigned int length) {

  int i = 0;
  
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  
  String msgString = String(message_buff);
  
    if(msgString.equals("veranda1=UP")) { ShuttersCommand(0,0); }
    else 
    if(msgString.equals("veranda1=DOWN")) { ShuttersCommand(0,1); }
    else 
    if(msgString.equals("veranda2=UP")) { ShuttersCommand(1,0); }
    else 
    if(msgString.equals("veranda2=DOWN")) { ShuttersCommand(1,1); }
    else
    if(msgString.equals("kitchen=UP")) { ShuttersCommand(2,0); }
    else
    if(msgString.equals("kitchen=DOWN")) { ShuttersCommand(2,1); }
    else
    if(msgString.equals("lounge=UP")) { ShuttersCommand(3,0); }
    else
    if(msgString.equals("lounge=DOWN")) { ShuttersCommand(3,1); }
    else
    if(msgString.equals("room=UP")) { ShuttersCommand(4,0); }
    else
    if(msgString.equals("room=DOWN")) { ShuttersCommand(4,1); }
    else
    if(msgString.equals("office=UP")) { ShuttersCommand(5,0); }
    else
    if(msgString.equals("office=DOWN")) { ShuttersCommand(5,1); }
    else
    if(msgString.equals("all=UP")) { ShuttersCommandAll(0); }
    else
    if(msgString.equals("all=DOWN")) { ShuttersCommandAll(1); }
    else
    if(msgString.equals("lightterrace=ON")) { CommandLightTerrace(); }
    else
    { Serial.println("no message in callback MQTT"); }
  
}
