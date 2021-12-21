#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//WiFi
const char* ssid = "wnet2305n_guest";
const char* password = "gewitter";
const char* mqtt_server = "192.168.1.166";

//MQTT
const char mqtt_topic[] = "mpct/update/#";

//Tally Lines to listen to (device# on MPCT controller Tally)
const int pgmLinesToListenTo[] = { 0, 2 }; //0: Cam 1 PGM, 2: Cam 2 PGM, 4: Cam 3 PGM
const int pvwLinesToListenTo[] = { 1, 3 }; //1: Cam 1 PVW, 3: Cam 2 PVW, 5: Cam 3 PVW
const int noPgmLines = 2; //Number of elements in pgmLinesToListenTo array;
const int noPvwLines = 2; //Number of elements in pvwLinesToListenTo array;
boolean pgmLinesState[noPgmLines]; //Store current state of program lines here
boolean pvwLinesState[noPvwLines]; //Store current state of preview lines here

//Keep the current status of the tally lights here
bool pgmLightStatus = false;
bool pvwLightStatus = false;

//Status of blinking blue indicator
bool blueStatus = true;

//All lines off at init time
void initLinesArrays() {
  for (int i = 0; i < noPgmLines; i++) {
    pgmLinesState[i] = false;
  }
  for (int i = 0; i < noPvwLines; i++) {
    pvwLinesState[i] = false;
  }
}

//Return true if any of the auditioned program lines are active
bool getPgmLightStatus() {
  for (int i = 0; i < noPgmLines; i++) {
    if (pgmLinesState[i] == true) {
      return true;
    }
  }
  return false;
}

//Return true if any of the auditioned preview lines are active
bool getPvwLightStatus() {
  for (int i = 0; i < noPvwLines; i++) {
    if (pvwLinesState[i] == true) {
      return true;
    }
  }
  return false;
}

//Return the index of the program lineNo provided, or -1
int getPgmLineIndex(int lineNo) {
  //Traverse the array to find the index of lineNo, if present  
  for (int i = 0; i < noPgmLines; i++) {
    if (pgmLinesToListenTo[i] == lineNo) {
      return i;
    }
  }
  return -1;
}

//Return the index of the preview lineNo provided, or -1
int getPvwLineIndex(int lineNo) {
  //Traverse the array to find the index of lineNo, if present  
  for (int i = 0; i < noPvwLines; i++) {
    if (pvwLinesToListenTo[i] == lineNo) {
      return i;
    }
  }
  return -1;
}

//Print status of auditioned lines
void printLinesState() {
  Serial.println("STATE OF LINES");
  for (int i = 0; i < noPgmLines; i++) {
    Serial.print(pgmLinesToListenTo[i]);
    Serial.print(":");
    Serial.println(pgmLinesState[i]);
  }
}

//Return true if lineNo was valid, and its value changed
bool updatePgmLineState(int lineNo, bool state) {
  //Check if we're listening to this line (find index of element in pgmLinesToListenTo by value)
  int lineIndex = getPgmLineIndex(lineNo);
  if (lineIndex > -1) {    
    //Line was found
    if (pgmLinesState[lineIndex] != state) {
      //State has changed
      pgmLinesState[lineIndex] = state;
      return true;
    }
  }  
  return false;
}

//Return true if lineNo was valid, and its value changed
bool updatePvwLineState(int lineNo, bool state) {
  //Check if we're listening to this line (find index of element in pvwLinesToListenTo by value)
  int lineIndex = getPvwLineIndex(lineNo);
  if (lineIndex > -1) {    
    //Line was found
    if (pvwLinesState[lineIndex] != state) {
      //State has changed
      pvwLinesState[lineIndex] = state;
      return true;
    }
  }  
  return false;
}

WiFiClient espClient;
PubSubClient client(espClient);



// LED Pin
const int pinRed = 16; //The Program LED Field
const int pinGreen = 17; //The Preview LED field
const int pinBlue = 4; //Blinking status LED

String getMsgStr(byte* message, unsigned int length) {
  String messageStr;  
  for (int i = 0; i < length; i++) {
    //Serial.print((char)message[i]);
    messageStr += (char)message[i];
  }
  return messageStr;
}

//MQTT message received
void callback(char* topic, byte* message, unsigned int length) {
  String topicStr = String(topic);
  String messageStr = getMsgStr(message, length);

  //Serial.print("Message arrived on topic: ");
  //Serial.println(topic);
  bool changeOccurred = false; //flag to determine whether lights need to be updated
  if (topicStr.substring(0,17) == "mpct/update/Tally") {
    //Extract tally line no from topic string
    String tallyLineStr = topicStr.substring(17, topicStr.indexOf("."));    
    int lineNo = tallyLineStr.toInt();

    //Parse message JSON into data object
    DynamicJsonDocument data(1024);
    deserializeJson(data, messageStr);

    //Extract the value/state for the tally line
    bool state = !data["data"]["status"]["value"];

    //Update the line (updatePgmLineState and updatePvwLineState return false if line is not auditioned or value didn't change)
    if (updatePgmLineState(lineNo, state)) {
      changeOccurred = true;
      const bool newPgmLightStatus = getPgmLightStatus();
      if (pgmLightStatus != newPgmLightStatus) {
        //Light status changed
        pgmLightStatus = newPgmLightStatus;
        Serial.print("PGM: ");
        Serial.println(pgmLightStatus);
      }
    } else if (updatePvwLineState(lineNo, state)) {
      changeOccurred = true;
      const bool newPvwLightStatus = getPvwLightStatus();
      if (pvwLightStatus != newPvwLightStatus) {
        //Light status changed
        pvwLightStatus = newPvwLightStatus;
        Serial.print("PVW: ");
        Serial.println(pvwLightStatus);
      }
    }
    if (changeOccurred) {
        //Drive program light:
        digitalWrite(pinRed, pgmLightStatus);
        //Drive preview light:
        digitalWrite(pinGreen, !pgmLightStatus && pvwLightStatus);
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  //Initialize LED pins
  pinMode(pinRed, OUTPUT);
  pinMode(pinGreen, OUTPUT);
  pinMode(pinBlue, OUTPUT);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


long lastMsg = 0; //Timestamp for loop intervals

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;    
    //Blink the indicator
    blueStatus = !blueStatus;
    digitalWrite(pinBlue, blueStatus);
  }
}