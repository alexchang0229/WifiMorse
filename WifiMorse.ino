#include <WiFiManager.h>
#include <PubSubClient.h>

WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient client(espClient);

#define mqtt_server "io.adafruit.com" 
#define port        1883
#define MQTT_id    "YOUR ID HERE" 
#define MQTT_password  "YOUR PASSWORD HERE"
#define keyPin 4
#define LedPin 16
#define channelPin 0

char* station_id = "A"; //Chage this so it's unique for each telegraph module
char* ChannelA = "YOUR ID HERE/feeds/A" ;
char* ChannelB = "YOUR ID HERE/feeds/B" ;
char* ChannelC = "YOUR ID HERE/feeds/C" ;

const int messageArraySize = 400;

int keyState = 0;
int channelCounter = 1;
int channelkeyState;
int channelButtonPrevState = 1;
int lastkeyState = 0; // previous state of the button
int startPressed = 0;    // the moment the button was pressed
int endPressed = 0;      // the moment the button was released
int holdTime = 0;        // how long the button was hold
int idleTime = 0;        // how long the button was idle
int idleArray[messageArraySize];     // array to record button idle times
int holdArray[messageArraySize];   // array to record button hold times
int i;
int j;
int k;
char space[] = " ";
int sendArray[messageArraySize*2];   // array to store idle/hold times to be sent
char buf[sizeof(int)];
int pressed = 0;
char textToSend[sizeof(sendArray) * 5] = {"\0"};

char* channelNow = ChannelA;
long lastReconnectAttempt = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  //This function handles the message receiving 
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char* token;
  bool self = 0;
  if(payload[0] == *station_id || payload[0]=='!'){
    //Don't process messages originating from this station.
    //Also don't process the connected messages.
    self = 1; 
  }
  token = strtok((char*)payload, " ");
  i = 0;
  while (token && self == 0){
    //loop through recieved message
    Serial.print(token);
    Serial.print(" ");
    if ( i % 2 == 0 && atoi(token)!= 0) {
      //Split even and odd entries into tone and no tones
        tone(12, 523);
        delay(atoi(token));
        noTone(12);
    }
    else {
      noTone(12);
      delay(atoi(token));
    }
    token = strtok(NULL, " ");
    i++;
  }
  i=0;
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  if(client.setBufferSize(4096)){
    Serial.println("Set buffer size sucess");
    }
  else {
    Serial.println("Set buffer size failed");
    }
  pinMode(channelPin, INPUT_PULLUP);
  pinMode(keyPin, INPUT);
  pinMode(LedPin, OUTPUT);
  wifiManager.autoConnect("Telegraph");
  client.setServer(mqtt_server, port);
  client.setCallback(callback);
}

boolean reconnect() {
  if (client.connect(station_id, MQTT_id, MQTT_password)) {
    char connectedMessage[50] = "!Connected: ";
    // Once connected, publish an announcement...
    client.publish(channelNow, strcat(connectedMessage,station_id));
    // ... and resubscribe
    client.subscribe(channelNow);
    memset(connectedMessage, 0, sizeof(connectedMessage));
  }
  return client.connected();
}

void loop() {
  //----------- KEEP MQTT CONNECTED -----------//
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop();
  }
  delay(3);


  //----------- CHANGE BETWEEN CHANNELS -----------//
  channelkeyState = digitalRead(channelPin); 
  if(channelkeyState == 0 && channelkeyState!=channelButtonPrevState){
    changeChannels();
  }
  channelButtonPrevState = channelkeyState;


  //----------- PROCESS TELEGRAPH KEY PRESSES -----------//
  keyState = digitalRead(keyPin);
  updateState(); 
  if (lastkeyState != keyState && keyState == 1) {
    idleArray[i] = idleTime; //Register up time in array
    i++;
  }
  if (lastkeyState != keyState && keyState == 0) {
    holdArray[j] = holdTime; //Register down time in array
    j++;
  }
  lastkeyState = keyState;

  //----------- TRANSMIT -----------//
  if (idleTime > 2500 && pressed == 1) { //Only transmit if it's been 2.5 seconds since last button press
    pressed = 0; //reset button press
    k = 0;
    idleArray[0] = 0; //set first delay to zero so the message doesn't start with delay
    for (i = 0; i < messageArraySize;) { //This loop combines the timing of the idle and hold arrays into sendArray
      sendArray[i] = idleArray[k]; //Put idle times in even index of sendArray
      i = i + 1; //indexing sendArray
      sendArray[i] = holdArray[k]; //Put hold times in odd index of sendArray
      i = i + 1;
      k = k + 1; //indexing idle and hold arrays
    }
    strcat(textToSend,station_id); //Start message with ID of sending station
    strcat(textToSend, space); 
    for (i = 0; i < messageArraySize*2; i++)
    {
      itoa(sendArray[i], buf, 10);
      strcat(textToSend, buf);
      strcat(textToSend, space);
    }
    client.publish(channelNow, (char*)textToSend); 
    tone(12, 523 * 2); //Confirmation beep
    delay(100);
    noTone(12);
    memset(idleArray, 0, sizeof(idleArray)); //clear arrays
    memset(holdArray, 0, sizeof(holdArray));
    memset(textToSend, 0, sizeof(textToSend));
    i = 0;
    j = 0;
    
  }
}

void updateState() {
  //This function records the timing of the Morse key presses.
  if (keyState == HIGH) { //Key is down
    startPressed = millis();  
    holdTime = startPressed - endPressed; //save time button was held down
    digitalWrite(LedPin, LOW);  //Flash LED
    tone(12, 523); //Play beep
    if (idleTime > 2500) {
      idleTime = 0; //if button was pressed after more than 2.5 seconds idle, restart 2.5 second timer
    }
    pressed = 1; //button was pressed
  }
  if (keyState == LOW) { //Key is up
    endPressed = millis(); 
    digitalWrite(LedPin, HIGH); //Extinguish LED
    idleTime = endPressed - startPressed; //save time button was up
    noTone(12); //Stop beep
  }
}

void changeChannels() {
    client.unsubscribe(channelNow);
    channelCounter++;
    switch(channelCounter){
    case 1:
      channelNow = ChannelA;
      tone(12,523);delay(100);noTone(12);
      break;
    case 2:
      channelNow = ChannelB;
      tone(12,523);delay(100);noTone(12);tone(12,523);delay(100);noTone(12);
      break;
    case 3:
      channelNow = ChannelC;
      channelCounter = 0;
      tone(12,523);delay(100);noTone(12);tone(12,523);delay(100);noTone(12);tone(12,523);delay(100);noTone(12);
      break;  
    }
    Serial.print(channelNow);
    reconnect();
}
