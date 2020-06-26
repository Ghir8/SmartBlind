#include <Arduino.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <Config.h>
#include <ArduinoOTA.h>

#define LED_BUILTIN 2
#define INPUT_PIN_UP 32
#define INPUT_PIN_DOWN 33
#define RELAY_PIN_UP 27
#define RELAY_PIN_DOWN 26

#define FIRST_TAP 500
#define SECOND_TAP 1000

AsyncWebServer server(80);

// addresses of data stored in EEPROM
struct {
  int initialized = 0;
  int secondsToClose = sizeof(bool);
  int secondsToOpen = sizeof(bool) + sizeof(float);
  int currentPosition = sizeof(bool) + (sizeof(float) * 2);
} eepromAddresses;

// default timing settings
float secondsToClose = DEFAULT_SECONDS_TO_CLOSE;
float secondsToOpen = DEFAULT_SECONDS_TO_OPEN;
unsigned int millisPerPercentClose = (secondsToClose * 1000) / 100;
unsigned int millisPerPercentOpen = (secondsToOpen * 1000) / 100;

// blind position / state vars
double startingPosition = 0.0;
double currentPosition = 0.0;
double desiredPosition = 0.0;
bool spinning = false;
int direction; // 1 is closing (DOWN), -1 is opening (UP)
bool flag = false;
bool dTap = false;

// stores the last time a cycle was initiated
unsigned long previousMillis = 0;

// calculated interval (ms) of spin cycle
long interval = 0;

// functions
void startSpinning(double newPosition);
void stopSpinning();
void doubleTap(double deltaT, int direction);
void userInput(unsigned long startInput, double curPosInput, int direction);
void runServer();
void OTAStart();
void debug(String s){
  if (DEBUG_MODE) {
    Serial.println(s);
  }
}

void setup(){
  Serial.begin(115200);
  EEPROM.begin(512);

  // initialize LED and pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY_PIN_UP, OUTPUT);
  pinMode(RELAY_PIN_DOWN, OUTPUT);
  digitalWrite(RELAY_PIN_UP,HIGH);
  digitalWrite(RELAY_PIN_DOWN, HIGH);

  //initialize INPUT pin
  pinMode(INPUT_PIN_UP, INPUT_PULLUP);
  pinMode(INPUT_PIN_DOWN, INPUT_PULLUP);

  // wifi connect
  WiFi.mode(WIFI_STA);
  //WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    while(WiFi.waitForConnectResult() != WL_CONNECTED){
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      Serial.print(".");
      delay(100);
    }
  }
  debug("IP Address: "+WiFi.localIP());

  // get stored settings or use defaults
  bool initialized = false;
  EEPROM.get(eepromAddresses.initialized, initialized);
  if (initialized) {
    EEPROM.get(eepromAddresses.secondsToClose, secondsToClose);
    EEPROM.get(eepromAddresses.secondsToOpen, secondsToOpen);
    EEPROM.get(eepromAddresses.currentPosition, currentPosition);
  } else {
    EEPROM.put(eepromAddresses.initialized, true);
    EEPROM.put(eepromAddresses.secondsToClose, secondsToClose);
    EEPROM.put(eepromAddresses.secondsToOpen, secondsToOpen);
    EEPROM.put(eepromAddresses.currentPosition, currentPosition);
    EEPROM.commit();
  }

  // milliseconds of rotation per 1% change in blinds
  millisPerPercentClose = (secondsToClose * 1000) / 100;
  millisPerPercentOpen = (secondsToOpen * 1000) / 100;

  debug("Initial position: "+String(currentPosition));
  debug("Seconds to close: "+String(secondsToClose));
  debug("Seconds to open: "+String(secondsToOpen));

  OTAStart();
  runServer();
}

void loop() {
  ArduinoOTA.handle();
  unsigned long currentMillis = millis();

  if (spinning){
    // calculate current position
    double percentCompleted = ((double)(currentMillis - previousMillis))/((double)interval);

    // solution for data inconsistency before currentPosition's calculus
    if((int)(currentMillis - previousMillis) < 0){
      percentCompleted = 0;
    }
    currentPosition = (percentCompleted * (desiredPosition-startingPosition)) + startingPosition;

    // check user input
    if (direction > 0){
      if(digitalRead(INPUT_PIN_DOWN) == 0){
        digitalWrite(RELAY_PIN_UP, HIGH);
        digitalWrite(RELAY_PIN_DOWN, HIGH);
        debug("Input DOWN from spinning");
        //inputDown(currentMillis, currentPosition);
        userInput(currentMillis, currentPosition, 1);
      }
    }else if(direction < 0){
      if(digitalRead(INPUT_PIN_UP) == 0){
        digitalWrite(RELAY_PIN_UP, HIGH);
        digitalWrite(RELAY_PIN_DOWN, HIGH);
        debug("Input UP from spinning");
        //inputUp(currentMillis, currentPosition);
        userInput(currentMillis, currentPosition, -1);
      }
    }
    // if position has been reached or is out of range, stop spinning
    if (currentPosition == desiredPosition || percentCompleted > 1 || currentPosition < 0 || currentPosition > 100){
      stopSpinning();
    }
  }else{
    // check user input
    if(digitalRead(INPUT_PIN_UP) == 0){
      digitalWrite(RELAY_PIN_UP, HIGH);
      digitalWrite(RELAY_PIN_DOWN, HIGH);
      debug("Input UP form loop");
      //inputUp(currentMillis, currentPosition);
      userInput(currentMillis, currentPosition, -1);
    }
    if(digitalRead(INPUT_PIN_DOWN) == 0){
      digitalWrite(RELAY_PIN_UP, HIGH);
      digitalWrite(RELAY_PIN_DOWN, HIGH);
      debug("Input DOWN from loop");
      //inputDown(currentMillis, currentPosition);
      userInput(currentMillis, currentPosition, 1);
    }
  }
}

// START SPINNING

void startSpinning(double newPosition){
  if (newPosition != desiredPosition || newPosition != currentPosition) {
    desiredPosition = newPosition;
    debug("current position: "+String(currentPosition));
    debug("desired position: "+String(desiredPosition));

    unsigned long currentMillis = millis();

    // calculate spin cycle length (ms)
    unsigned int millisPerPercent = desiredPosition > currentPosition ? millisPerPercentOpen : millisPerPercentClose;
    interval = (long)(abs(desiredPosition - currentPosition) * millisPerPercent);
    debug("calculated interval: "+interval);

    // set state vars
    direction = desiredPosition > currentPosition ? 1 : -1;
    previousMillis = currentMillis;
    spinning = true;
    startingPosition = currentPosition;
    debug("calculated direction: "+direction);

    digitalWrite(LED_BUILTIN, HIGH);

    // closing
    if (direction > 0) {
      debug("closing");
      digitalWrite(RELAY_PIN_DOWN, HIGH);
      digitalWrite(RELAY_PIN_UP, LOW);
    }
    // opening
    if (direction < 0) {
      debug("opening");
      digitalWrite(RELAY_PIN_UP, HIGH);
      digitalWrite(RELAY_PIN_DOWN, LOW);
    }
  }
}

// STOP SPINNING

void stopSpinning(){
  digitalWrite(RELAY_PIN_UP, HIGH);
  digitalWrite(RELAY_PIN_DOWN, HIGH);
  currentPosition = desiredPosition;
  EEPROM.put(eepromAddresses.currentPosition, currentPosition);
  EEPROM.commit();
  spinning = false;
  debug("finished spinning!");
  digitalWrite(LED_BUILTIN, LOW);
}

// DOUBLE TAP

void doubleTap(double deltaT, int direction){
  if(deltaT > FIRST_TAP && deltaT < SECOND_TAP){
    debug("flag TRUE");
    flag = true;
    dTap = false;
  } else if(deltaT > SECOND_TAP && flag){
    double newPosition = direction > 0 ? 0 : 100;
    debug("newPosition "+String(newPosition));
    flag = false;
    dTap = true;
    delay(1000);
    startSpinning(newPosition);
  } else {
    debug("flag FALSE");
    flag = false;
    dTap = false;
  }
}

// USER INPUT

void userInput(unsigned long startInput, double curPosInput, int direction){
  spinning = true;
  unsigned long endInput;
  double deltaT;
  direction > 0 ? debug("Direction DOWN"): debug("Direction UP"); // 1 down, -1 up
  if (direction > 0) {
    do{
      delay(50);
    }while (digitalRead(INPUT_PIN_DOWN) == 0);
  } else {
    do{
      delay(50);
    }while (digitalRead(INPUT_PIN_UP) == 0);
  }
  endInput = millis();
  deltaT = endInput - startInput;
  if(deltaT <= 50){
    stopSpinning();
    return;
  }
  curPosInput = direction > 0 ? curPosInput -((deltaT)/(secondsToOpen*1000))*100 : curPosInput +((deltaT)/(secondsToOpen*1000))*100;
  curPosInput = direction > 0 ? (curPosInput < 0 ? 0 : curPosInput) : (curPosInput > 100 ? 100 : curPosInput);
  desiredPosition = curPosInput;
  currentPosition = curPosInput;
  doubleTap(deltaT, direction);
  debug("Current position: "+String(currentPosition));
  if (dTap) {
    dTap = false;
    return;
  }else{
    stopSpinning();
  }
}

// SERVER

void runServer(){
  // get position
  server.on("/position", HTTP_GET, [] (AsyncWebServerRequest *request) {
    digitalWrite(LED_BUILTIN, LOW);
    debug("Request /position");
    request->send(200, "text/plain", String(currentPosition));
    digitalWrite(LED_BUILTIN, HIGH);
  });

  // set position with ?position=N (0 to 100)
  server.on("/set", HTTP_GET, [] (AsyncWebServerRequest *request) {
    digitalWrite(LED_BUILTIN, LOW);
    debug("REQUEST /SET");
    if (request->hasParam("position")) {
      debug(request->getParam("position")->value());
      double newPosition = request->getParam("position")->value().toDouble();
      startSpinning(newPosition);
    }
    request->send(204);
    digitalWrite(LED_BUILTIN, HIGH);
  });

  // get state
  server.on("/state", HTTP_GET, [] (AsyncWebServerRequest *request) {
    digitalWrite(LED_BUILTIN, LOW);
    int state = spinning ? (direction == -1 ? 0 : 1) : 2;
    debug("Request /state"+String(state));
    request->send(200, "text/plain", String(state));
    digitalWrite(LED_BUILTIN, HIGH);
  });

  // update timing settings with ?secondsToClose=X&secondsToOpen=Y (positive float)
  server.on("/timing", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (currentPosition > 0 && currentPosition < 100) {
      request->send(409, "text/plain", "Cannot update timing now. Move blinds to fully closed or open position and try again.");
      return;
    }
    if (request->hasParam("secondsToClose")) {
      secondsToClose = request->getParam("secondsToClose")->value().toFloat();
      EEPROM.put(eepromAddresses.secondsToClose, secondsToClose);
      millisPerPercentClose = (secondsToClose * 1000) / 100;
    }
    if (request->hasParam("secondsToOpen")) {
      secondsToOpen = request->getParam("secondsToOpen")->value().toFloat();
      EEPROM.put(eepromAddresses.secondsToOpen, secondsToOpen);
      millisPerPercentOpen = (secondsToOpen * 1000) / 100;
    }
    EEPROM.commit();
    request->send(204);
  });

  // 404
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404);
  });

  server.begin();
}


// OTA

void OTAStart() {

  ArduinoOTA.setHostname("esp-blids");
  //ArduinoOTA.setPassword(""); // SET OTA PASSWORD

  ArduinoOTA
    .onStart([]() {
     String type;
     if (ArduinoOTA.getCommand() == U_FLASH)
       type = "sketch";
     else // U_SPIFFS
       type = "filesystem";

     // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
     Serial.println("Start updating " + type);
   })
   .onEnd([]() {
     Serial.println("\nEnd");
   })
   .onProgress([](unsigned int progress, unsigned int total) {
     Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
   })
   .onError([](ota_error_t error) {
     Serial.printf("Error[%u]: ", error);
     if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
     else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
     else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
     else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
     else if (error == OTA_END_ERROR) Serial.println("End Failed");
   });

  ArduinoOTA.begin();
}
