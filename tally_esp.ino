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
const int linesToListenTo[] = { 0, 2 }; //0: Cam 1 PGM, 2: Cam 2 PGM, 4: Cam 3 PGM
const int noLines = 2; //Number of elements in linesToListenTo array;
boolean linesState[noLines]; //Store current state of each line here

//Keep the current status of the tally light here
bool lightStatus = false;

//All lines off at init time
void initLinesArray() {
  for (int i = 0; i < noLines; i++) {
    linesState[i] = false;
  }
}

//Return true if any of the auditioned tally lines are active
bool getLightStatus() {
  for (int i = 0; i < noLines; i++) {
    if (linesState[i] == true) {
      return true;
    }
  }
  return false;
}

//Return the index of the lineNo provided, or -1
int getLineIndex(int lineNo) {
  //Traverse the array to find the index of lineNo, if present  
  for (int i = 0; i < noLines; i++) {
    if (linesToListenTo[i] == lineNo) {
      return i;
    }
  }
  return -1;
}

//Print status of auditioned lines
void printLinesState() {
  Serial.println("STATE OF LINES");
  for (int i = 0; i < noLines; i++) {
    Serial.print(linesToListenTo[i]);
    Serial.print(":");
    Serial.println(linesState[i]);
  }
}

//Return true if lineNo was valid, and its value changed
bool updateLineState(int lineNo, bool state) {
  //Check if we're listening to this line (find index of element in linesToListenTo by value)
  int lineIndex = getLineIndex(lineNo);
  if (lineIndex > -1) {    
    //Line was found
    if (linesState[lineIndex] != state) {
      //State has changed
      linesState[lineIndex] = state;
      return true;
    }
  }  
  return false;
}


WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;


// LED Pin
const int ledPin = 2;

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

  if (topicStr.substring(0,17) == "mpct/update/Tally") {
    //Extract tally line no from topic string
    String tallyLineStr = topicStr.substring(17, topicStr.indexOf("."));    
    int lineNo = tallyLineStr.toInt();



    //Parse message JSON into data object
    DynamicJsonDocument data(1024);
    deserializeJson(data, messageStr);

    //Extract the value/state for the tally line
    bool state = !data["data"]["status"]["value"];

    //Update the line (returns false if line is not auditioned or value didn't change)
    if (updateLineState(lineNo, state)) {
      const bool newLightStatus = getLightStatus();
      if (lightStatus != newLightStatus) {
        //Light status changed
        lightStatus = newLightStatus;
        Serial.print("Light: ");
        Serial.println(lightStatus);
        //Drive light:
        digitalWrite(ledPin, lightStatus);
      }
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
  pinMode(ledPin, OUTPUT);
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

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;    
  }
}