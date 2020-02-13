#include <Arduino.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <Config.h>

//#define LED_BUILTIN 2
#define INPUT_PIN_UP 25
#define INPUT_PIN_DOWN 26
#define RELE_PIN_UP 12
#define RELE_PIN_DOWN 13

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
unsigned int milliesPerPercentClose = (secondsToClose * 1000) / 100;
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
    unsigned long currentMillis = millis();

    // calculate spin cycle length (ms)
    unsigned int millisPerPercent = desiredPosition > currentPosition ? millisPerPercentOpen : milliesPerPercentClose;
    interval = (long)(abs(desiredPosition - currentPosition) * millisPerPercent);

    // set state vars
    direction = desiredPosition > currentPosition ? 1 : -1;
    previousMillis = currentMillis;
    spinning = true;
    startingPosition = currentPosition;

    digitalWrite(LED_BUILTIN, HIGH);

    // closing
    if (direction > 0) {
      digitalWrite(RELE_PIN_DOWN, HIGH);
      digitalWrite(RELE_PIN_UP, LOW);
    }
    // opening
    if (direction < 0) {
      digitalWrite(RELE_PIN_UP, HIGH);
      digitalWrite(RELE_PIN_DOWN, LOW);
    }
  }
}

void stopSpinning(){
  digitalWrite(RELE_PIN_UP, HIGH);
  digitalWrite(RELE_PIN_DOWN, HIGH);
  spinning = false;
  currentPosition = desiredPosition;
  EEPROM.put(eepromAddresses.currentPosition, currentPosition);
  EEPROM.commit();
  digitalWrite(LED_BUILTIN, LOW);
}

void inputButton(unsigned long inizio, double cP){
  spinning = true;
  unsigned long trascorso;
  double deltaT;
  if (digitalRead(INPUT_PIN_UP)) { // Opening
    direction = -1;
    while (digitalRead(INPUT_PIN_UP)) {
       trascorso= millis();
    }
    deltaT = trascorso - inizio;
    cP= cP +((deltaT)/(secondsToOpen*1000))*100;
    cP = cP > 100 ? 100 : cP;
    desiredPosition = cP;
    currentPosition = cP;
  }else{ // Closing
    direction = 1;
    while (digitalRead(INPUT_PIN_DOWN)) {
       trascorso= millis();
    }
    deltaT = trascorso - inizio;
    cP= cP -((deltaT)/(secondsToOpen*1000))*100;
    cP = cP < 0 ? 0 : cP;
    desiredPosition = cP;
    currentPosition = cP;
  }

  stopSpinning();
}

void runServer(){
  // get position
  server.on("/position", HTTP_GET, [] (AsyncWebServerRequest *request) {
    digitalWrite(LED_BUILTIN, LOW);
    request->send(200, "text/plain", String(currentPosition));
    digitalWrite(LED_BUILTIN, HIGH);
  });

  // set position with ?position=N (0 to 100)
  server.on("/set", HTTP_GET, [] (AsyncWebServerRequest *request) {
    digitalWrite(LED_BUILTIN, LOW);
    if (request->hasParam("position")) {
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
      milliesPerPercentClose = (secondsToClose * 1000) / 100;
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
  pinMode(RELE_PIN_UP, OUTPUT);
  pinMode(RELE_PIN_DOWN, OUTPUT);
  digitalWrite(RELE_PIN_UP,HIGH);
  digitalWrite(RELE_PIN_DOWN, HIGH);

  //initialize INPUT pin
  pinMode(INPUT_PIN_UP, INPUT_PULLDOWN);
  pinMode(INPUT_PIN_DOWN, INPUT_PULLDOWN);

  // wifi connect
  WiFi.mode(WIFI_STA);
  //WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    while(WiFi.waitForConnectResult() != WL_CONNECTED){
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      delay(100);
    }
  }

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
  milliesPerPercentClose = (secondsToClose * 1000) / 100;
  millisPerPercentOpen = (secondsToOpen * 1000) / 100;

  runServer();
}

void loop() {
  unsigned long currentMillis = millis();
  if(digitalRead(INPUT_PIN_UP) || digitalRead(INPUT_PIN_DOWN)){
    digitalWrite(RELE_PIN_UP, HIGH);
    digitalWrite(RELE_PIN_DOWN, HIGH);
    inputButton(currentMillis, currentPosition);
  }

  if (spinning){
    // calculate current position
    double percentCompleted = ((double)(currentMillis - previousMillis))/((double)interval);
    //percentCompleted = (currentMillis - previousMillis) < 0 ? 0 : percentCompleted;
    if((int)(currentMillis - previousMillis) < 0){
      percentCompleted = 0;
    }
    currentPosition = (percentCompleted * (desiredPosition-startingPosition)) + startingPosition;
    if(digitalRead(INPUT_PIN_UP) || digitalRead(INPUT_PIN_DOWN)){
      digitalWrite(RELE_PIN_UP, HIGH);
      digitalWrite(RELE_PIN_DOWN, HIGH);
      inputButton(currentMillis, currentPosition);
    }
    // if position has been reached or is out of range, stop spinning
    if (currentPosition == desiredPosition || percentCompleted > 1 || currentPosition < 0 || currentPosition > 100){
      stopSpinning();
    }
  }
}
