#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EasyButton.h>
#include "RTClib.h"

#define TEMP 23
#define LEDS 32
#define BUT_RIGHT 18 //PULLDOWN
#define BUT_LEFT 19

//Button Config
EasyButton leftButton = EasyButton(BUT_LEFT);
EasyButton rightButton = EasyButton(BUT_RIGHT);


//Temp Config
OneWire oneWire(TEMP);
DallasTemperature sensors(&oneWire);

//WiFi Config
const char* hostname = "7Clock";
IPAddress AP_IP(192,168,100,1);
IPAddress gateway(192,168,100,1);
IPAddress subnet(255,255,255,0);
const char* ssidSTA = "wireless";
const char* passwordSTA = "getoutofmyheadgetoutofmyhead";
const char* ssidAP = "7Clock";
const char* passwordAP = "thisisjustaclocksogetout";
AsyncWebServer server(80);

//Led Config
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(30, LEDS, NEO_GRB + NEO_KHZ800);

//RTC
RTC_DS3231 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

class Timer {

    
    RTC_DS3231* clock;

    bool running;
    unsigned long runningTime;
    unsigned long lastCall;

    void addTime() {
        unsigned long currentTime = clock -> now().unixtime();
        if(currentTime > lastCall)
            runningTime += currentTime - lastCall;
        lastCall = currentTime;
    }

public:

    Timer(RTC_DS3231* rtc) {
        clock = rtc;
        reset();
    }

    void start() {
        if(running)
            return;
        running = true;
        lastCall = clock -> now().unixtime();
    }

    void stop() {
        if(!running)
            return;
        running = false;
        addTime();
    }

    void toggle() {
        if(running)
            stop();
        else
            start();
    }

    void reset() {
        running = false;
        runningTime = 0;
        lastCall = 0;
    }

    bool isRunning() {
        return running;
    }

    unsigned long getRunningTime() {
        if(!running)
            return runningTime;
        addTime();
        return runningTime;
    }

};

Timer timer = Timer(&rtc);
int mode = 0;

void WiFiReconnect(WiFiEvent_t event, WiFiEventInfo_t info) {
    WiFi.reconnect();
}

void WiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  if(millis() < 60000) {
      delay(5000);
    timeClient.begin();
    timeClient.setTimeOffset(3600 * 2);
    delay(5000);
    if(timeClient.forceUpdate()) {
      DateTime currentTime = DateTime(timeClient.getEpochTime());
      rtc.adjust(currentTime);
    }
  }
}

float getTemp() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

const bool NUMBERS[15][7] = {{1,1,0,1,1,1,1},{0,0,0,0,0,1,1},{1,1,1,0,1,1,0},{1,0,1,0,1,1,1},{0,0,1,1,0,1,1},{1,0,1,1,1,0,1},{1,1,1,1,1,0,1},{0,0,0,0,1,1,1},{1,1,1,1,1,1,1},{0,0,1,1,1,1,1},{0,0,1,1,1,1,0},{0,1,1,0,0,0,0},{1,0,1,1,1,0,1},{1,1,1,1,0,0,0},{0,0,0,0,0,0,0}};
const int OFFSETS[4] = {0,7,16,23};
void displayTime() {
  DateTime currentTime = rtc.now();
  int hour = currentTime.hour();
  int minute = currentTime.minute();
  int digits[4] = {minute % 10, minute / 10, hour % 10, hour / 10};
  uint32_t color = pixels.Color(20,200,20);
  for(int i = 0; i < 4; i++) {
    for(int j = 0; j < 7; j++) {
      pixels.setPixelColor(OFFSETS[i] + j, color * NUMBERS[digits[i]][j]);
    }
  }
  pixels.setPixelColor(14, color);
  pixels.setPixelColor(15, color);
  pixels.show();
}

void displayTemp() {
  float temp = getTemp();
  int digits[4] = {10, int(temp * 10) % 10, int(temp) % 10, (int(temp) / 10) % 10};
  uint32_t color = pixels.Color(255,60,20);
  for(int i = 0; i < 4; i++) {
    for(int j = 0; j < 7; j++) {
      pixels.setPixelColor(OFFSETS[i] + j, color * NUMBERS[digits[i]][j]);
    }
  }
  pixels.setPixelColor(14, color);
  pixels.setPixelColor(15, 0);
  pixels.show();
}

void displayChrono() {
  unsigned long accuTime = timer.getRunningTime();
  int hour = accuTime / 3600;
  int minute = (accuTime / 60) % 60;
  int digits[4] = {minute % 10, minute / 10, hour % 10, hour / 10};
  uint32_t color = pixels.Color(20,20,200);
  if(timer.isRunning())
    color = pixels.Color(160,160,20);
  for(int i = 0; i < 4; i++) {
    for(int j = 0; j < 7; j++) {
      pixels.setPixelColor(OFFSETS[i] + j, color * NUMBERS[digits[i]][j]);
    }
  }
  if(timer.isRunning() && ((millis() % 1000) > 500)) {
    pixels.setPixelColor(14, 0);
    pixels.setPixelColor(15, 0);
  } else {
    pixels.setPixelColor(14, color);
    pixels.setPixelColor(15, color);
  }  
  pixels.show();
}

void displayReset() {
  int digits[4] = {14,13,12,11};
  uint32_t color = pixels.Color(255,0,0);
  for(int i = 0; i < 4; i++) {
    for(int j = 0; j < 7; j++) {
      pixels.setPixelColor(OFFSETS[i] + j, color * NUMBERS[digits[i]][j]);
    }
  }
  pixels.setPixelColor(14, 0);
  pixels.setPixelColor(15, 0);
  pixels.show();
}

void handleLeftButton() {
  if(timer.getRunningTime() > 0)
    mode = mode < 3 ? mode + 1 : 0;
  else
    mode = mode < 2 ? mode + 1 : 0;
}

void handleRightButton() {
  if(mode == 2) {
    timer.toggle();
  }
  if(mode == 3) {
    timer.reset();
    mode = 2;
  }
  else {
    mode = 2;
  }
}

void setup() {
  rightButton.onPressed(handleRightButton);
  leftButton.onPressed(handleLeftButton);
  rightButton.begin();
  leftButton.begin();

  //WiFi config
  WiFi.onEvent(WiFiReconnect, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(WiFiConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.softAPConfig(AP_IP, gateway, subnet);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname);

  //OTA config
  WiFi.softAP(ssidAP, passwordAP);
  WiFi.begin(ssidSTA, passwordSTA);
  AsyncElegantOTA.begin(&server);
  server.begin();

  //Serial config
  Serial.begin(115200);

  //Temp probe setup
  sensors.begin();

  //Display setup
  pixels.begin();

  //RTC setup
  rtc.begin();
}

int loops = 0;
void loop() {
  leftButton.read();
  rightButton.read();
  if(mode == 0 && loops > 20) {
    displayTime();
    loops = 0;
  }
  if(mode == 1 && loops > 20) {
    displayTemp();
    loops = 0;
  }
  if(mode == 2 && loops > 20) {
    displayChrono();
    loops = 0;
  }
  if(mode == 3 && loops > 20) {
    displayReset();
    loops = 0;
  }
  loops++;
  delay(10);
}