#include <Arduino.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <Config.h>

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

// stores the last time a cycle was initiated
unsigned long previousMillis = 0;

// calculated interval (ms) of spin cycle
long interval = 0;


void startSpinning(double newPosition){
  if (newPosition != desiredPosition || newPosition != currentPosition) {
    desiredPosition = newPosition;
    Serial.print("current position: ");
    Serial.println(currentPosition);
    Serial.print("desired position: ");
    Serial.println(desiredPosition);
    unsigned long currentMillis = millis();

    // calculate spin cycle length (ms)
    unsigned int millisPerPercent = desiredPosition > currentPosition ? millisPerPercentOpen : millisPerPercentClose;
    interval = (long)(abs(desiredPosition - currentPosition) * millisPerPercent);
    Serial.print("calculated interval: ");
    Serial.println(interval);

    // set state vars
    direction = desiredPosition > currentPosition ? 1 : -1;
    previousMillis = currentMillis;
    spinning = true;
    startingPosition = currentPosition;
    Serial.print("calculated direction: ");
    Serial.println(direction);

    digitalWrite(LED_BUILTIN, HIGH);

    // closing
    if (direction > 0) {
      Serial.println("closing");
      digitalWrite(RELAY_PIN_DOWN, HIGH);
      digitalWrite(RELAY_PIN_UP, LOW);
    }
    // opening
    if (direction < 0) {
      Serial.println("opening");
      digitalWrite(RELAY_PIN_UP, HIGH);
      digitalWrite(RELAY_PIN_DOWN, LOW);
    }
  }
}

void stopSpinning(){
  digitalWrite(RELAY_PIN_UP, HIGH);
  digitalWrite(RELAY_PIN_DOWN, HIGH);
  currentPosition = desiredPosition;
  EEPROM.put(eepromAddresses.currentPosition, currentPosition);
  EEPROM.commit();
  spinning = false;
  Serial.println("finished spinning!");
  digitalWrite(LED_BUILTIN, LOW);
}

bool flag = false;
bool dTap = false;

void doubleTap(double deltaT, int direction){
  if(deltaT > FIRST_TAP && deltaT < SECOND_TAP){
    Serial.println("flag TRUE");
    flag = true;
    dTap = false;
  } else if(deltaT > SECOND_TAP && flag){
    double newPosition = direction > 0 ? 0 : 100;
    Serial.print("newPosition ");
    Serial.println(newPosition);
    flag = false;
    dTap = true;
    delay(1000);
    startSpinning(newPosition);
  } else {
    Serial.println("flag FALSE");
    flag = false;
    dTap = false;
  }
}

void inputUp(unsigned long startInput, double curPosInput){
  spinning = true;
  unsigned long endInput;
  double deltaT;
  Serial.println("direction: UP");
  direction = 1;
  do{
    delay(50);
    //endInput = millis();
  }while (digitalRead(INPUT_PIN_UP) == 0);
  endInput = millis();
  Serial.print("endInput: ");
  Serial.println(endInput);
  Serial.print("startInput: ");
  Serial.println(startInput);
  deltaT = endInput - startInput;
  if(deltaT <= 50){
    stopSpinning();
    return;
  }
  curPosInput= curPosInput +((deltaT)/(secondsToOpen*1000))*100;
  curPosInput = curPosInput > 100 ? 100 : curPosInput;
  desiredPosition = curPosInput;
  currentPosition = curPosInput;
  doubleTap(deltaT, direction);
  Serial.print("UP - deltaT: ");
  Serial.println(deltaT);
  Serial.print("UP - current position input: ");
  Serial.println(curPosInput);
  if (dTap) {
    dTap = false;
    return;
  }else{
    stopSpinning();
  }
}

void inputDown(unsigned long startInput, double curPosInput){
  spinning = true;
  unsigned long endInput;
  double deltaT;
  Serial.println("direction: DOWN");
  direction = -1;
  do{
    delay(50);
  }while (digitalRead(INPUT_PIN_DOWN) == 0);
  endInput = millis();
  Serial.print("endInput: ");
  Serial.println(endInput);
  Serial.print("startInput: ");
  Serial.println(startInput);
  deltaT = endInput - startInput;
  if(deltaT <= 50){
    stopSpinning();
    return;
  }
  curPosInput= curPosInput -((deltaT)/(secondsToOpen*1000))*100;
  curPosInput = curPosInput < 0 ? 0 : curPosInput;
  desiredPosition = curPosInput;
  currentPosition = curPosInput;
  doubleTap(deltaT, direction);
  Serial.print("DOWN - deltaT: ");
  Serial.println(deltaT);
  Serial.print("DOWN - current position input: ");
  Serial.println(curPosInput);
  if (dTap) {
    dTap = false;
    return;
  }else{
    stopSpinning();
  }
}

void runServer(){
  // get position
  server.on("/position", HTTP_GET, [] (AsyncWebServerRequest *request) {
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("Request /position");
    request->send(200, "text/plain", String(currentPosition));
    digitalWrite(LED_BUILTIN, HIGH);
  });

  // set position with ?position=N (0 to 100)
  server.on("/set", HTTP_GET, [] (AsyncWebServerRequest *request) {
    digitalWrite(LED_BUILTIN, LOW);
    Serial.print("REQUEST /SET");
    if (request->hasParam("position")) {
      Serial.println(request->getParam("position")->value());
      double newPosition = request->getParam("position")->value().toDouble();
      startSpinning(newPosition);
    }
    request->send(204);
    digitalWrite(LED_BUILTIN, HIGH);
  });

  // get state
  server.on("/state", HTTP_GET, [] (AsyncWebServerRequest *request) {
    digitalWrite(LED_BUILTIN, LOW);
    Serial.print("Request /state");
    int state = spinning ? (direction == -1 ? 0 : 1) : 2;
    Serial.println(String(state));
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

  // get timing settings
  server.on("/settings", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String response = "Seconds to close: " + String(secondsToClose) + "\nSeconds to open: " + String(secondsToOpen);
    request->send(200, "text/plain", response);
  });

  // 404
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404);
  });

  server.begin();
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
  Serial.println("");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

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

  Serial.print("Initial position: ");
  Serial.println(currentPosition);
  Serial.print("Seconds to close: ");
  Serial.println(secondsToClose);
  Serial.print("Seconds to open: ");
  Serial.println(secondsToOpen);

  runServer();
}

void loop() {
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
        Serial.println("Input DOWN from spinning");
        inputDown(currentMillis, currentPosition);
      }
    }else if(direction < 0){
      if(digitalRead(INPUT_PIN_UP) == 0){
        digitalWrite(RELAY_PIN_UP, HIGH);
        digitalWrite(RELAY_PIN_DOWN, HIGH);
        Serial.println("Input UP from spinning");
        inputUp(currentMillis, currentPosition);
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
      Serial.println("Input UP form loop");
      inputUp(currentMillis, currentPosition);
    }
    if(digitalRead(INPUT_PIN_DOWN) == 0){
      digitalWrite(RELAY_PIN_UP, HIGH);
      digitalWrite(RELAY_PIN_DOWN, HIGH);
      Serial.println("Input DOWN from loop");
      inputDown(currentMillis, currentPosition);
    }
  }
}
