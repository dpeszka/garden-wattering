#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PCF8574.h>
#include <RTClib.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // wyswietlacz lcd
PCF8574 relays(0x20); // ekstender io
RTC_DS1307 RTC; //zegar

#define RESET_EMERGENCY 4
#define PUMP_PRESSURE 5
#define ALERT_OUT 14

#define PUMP_RELAY 7
#define SECTIONS 7

#define TIMEPERSECTION 5

//AP configuration
char * ssid_ap = "NODEMCU";
char * password_ap = "qwerty1234";
IPAddress ip(192, 168, 6, 10);
IPAddress gateway(192,168, 6, 1);
IPAddress subnet(255, 255, 255, 0);

//server
ESP8266WebServer server;

int humidityValue = -1;

int resetLastPress = 0;
int lastPressureStatus = -1;
int lastPressureStatusChange = -1;

bool emergency = false;
bool systemRunning = false;

int currentSection = 1;
bool pumpRunning = false;
int pumpRunningFrom = -1;

int sectionTime = -1;

void setup(){
  Serial.begin(57600);
  while(!Serial);
  Serial.println("Serial working");
  Wire.begin(2, 0);
  lcd.init();   // Inicjalizacja LCD 2x16
  lcd.backlight(); // zalaczenie podwietlenia 
  
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ip, gateway, subnet);
  WiFi.softAP(ssid_ap, password_ap);
  
  server.on("/", handleIndex);
  server.on("/update", handleUpdate);
  server.begin();
  
  for(int i = 0; i < SECTIONS; i++){
    relays.pinMode(i, OUTPUT);
    relays.digitalWrite(i, HIGH);
  }

  relays.pinMode(PUMP_RELAY, OUTPUT);
  relays.digitalWrite(PUMP_RELAY, HIGH);

  for(int i = 0; i < SECTIONS; i++){
    relays.digitalWrite(i, LOW);
    delay(100);
    relays.digitalWrite(i, HIGH);
  }
  
  RTC.begin();
  if (!RTC.isrunning()) {
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }

  pinMode(RESET_EMERGENCY, INPUT);
  attachInterrupt(digitalPinToInterrupt(RESET_EMERGENCY), resetEmergency, RISING);

  pinMode(PUMP_PRESSURE, INPUT);
  attachInterrupt(digitalPinToInterrupt(PUMP_PRESSURE), pressureChange, CHANGE);

  pinMode(ALERT_OUT, OUTPUT);
}


ICACHE_RAM_ATTR void resetEmergency() {
    if(resetLastPress + 200 > millis()){
      return;
    }
    resetLastPress = millis();
    
    Serial.println("Button pressed");

    emergency = false;
    digitalWrite(ALERT_OUT, LOW);
    lastPressureStatus = -1;
    lastPressureStatusChange = -1;
    systemRunning = false;
    currentSection = 1;
    shutDownAllSections();
    stopPump();
}

int currentPressureStatus;

ICACHE_RAM_ATTR void pressureChange() {
    currentPressureStatus = digitalRead(PUMP_PRESSURE);
    if(lastPressureStatus == currentPressureStatus){
      return;
    }
    lastPressureStatus = currentPressureStatus;
    lastPressureStatusChange = millis();
    Serial.print("Pressure ");
    Serial.print(currentPressureStatus);
    Serial.println("");
}

void startPump(){
  if(pumpRunning) return;
  relays.digitalWrite(PUMP_RELAY, LOW);
  pumpRunning = true;
  pumpRunningFrom = millis();
  lastPressureStatusChange = millis();
  lastPressureStatus = 0;
}

void stopPump(){
  relays.digitalWrite(PUMP_RELAY, HIGH);
  pumpRunning = false;
  pumpRunningFrom = 0;
}

void shutDownAllSections(){
  for(int i = 0; i < SECTIONS; i++){
    relays.digitalWrite(i, HIGH);
  }
}

void enableSection(int sectionNum){
    relays.digitalWrite(sectionNum - 1, LOW);
    sectionTime = millis();
}

void setEmergency(){
  emergency = true;
  digitalWrite(ALERT_OUT, HIGH);
  stopPump();
  delay(100);
  shutDownAllSections();
  systemRunning = false;
}

int tmpNoPressure;
int tmpSectionTime;

void loop(){
    server.handleClient();
    printDateTime();

    lcd.setCursor(0, 1);
    lcd.print("H:     ");
    lcd.setCursor(3, 1);
    if(humidityValue > 0){
      lcd.print(String(humidityValue));
      lcd.print("%");
    }else{
      lcd.print("n/d");
    }

    tmpNoPressure = (millis() - lastPressureStatusChange) / 1000;
    
    if(emergency){
      lcd.setCursor(0, 0);
      lcd.print("EMERGENCY       ");
    }else if(lastPressureStatus == 0 && tmpNoPressure > 5 && pumpRunning){
      lcd.setCursor(0, 0);
      lcd.print("NO PRESSURE ");
      lcd.setCursor(12, 0);
      print2dig(tmpNoPressure);
      if(tmpNoPressure >= 30){
        setEmergency();
      }
      lcd.print("  ");
    }else if(humidityValue < 0){
      lcd.setCursor(0, 0);
      lcd.print("NO HUMIDITY DATA");
    } else if(humidityValue > 50 && !systemRunning) {
      lcd.setCursor(0, 0);
      lcd.print("HUMIDITY TO HIGH");
    }else if(!systemRunning){
      lcd.setCursor(0,0);
      lcd.print("SYSTEM READY    ");
      DateTime now = RTC.now(); 

      if(now.second() == 0){
        systemRunning = true;
      }
    } else if(systemRunning) {

      startPump();
      
      lcd.setCursor(3, 0);
      lcd.print(" P:");
      if(pumpRunning){
        lcd.print("ON ");
      }else{
        lcd.print("OFF");
      }

      lcd.setCursor(9,0);
      lcd.print(" T:");
      if(sectionTime > 0){
        tmpSectionTime = (millis() - sectionTime) / 1000;
      }else{ 
        tmpSectionTime = 0;
        shutDownAllSections();
        enableSection(currentSection);
      }
      lcd.print(tmpSectionTime);
      lcd.print("   ");

      if(tmpSectionTime >= TIMEPERSECTION){
        currentSection++;
        if(currentSection > SECTIONS){
          shutDownAllSections();
          stopPump();
          systemRunning = false;
          currentSection = 1;
          sectionTime = -1;
        }else{
          shutDownAllSections();
          enableSection(currentSection);
        }
      }
      lcd.setCursor(0,0);
      lcd.print("S:");
      lcd.print(currentSection);
    }
    delay(1000);
}

void handleIndex() {
  server.send(200, "text/plain", String(humidityValue));
}

void handleUpdate() {
  humidityValue = server.arg("value").toInt(); 
  server.send(200, "text/plain", "Updated");
}

void printDateTime(){
  DateTime now = RTC.now(); 
  lcd.setCursor(8,1);
  print2dig(now.hour());
  lcd.print(":");
  print2dig(now.minute());
  lcd.print(":");
  print2dig(now.second());
}

void print2dig(int num){
  if(num < 10){
    lcd.print('0');
  }
  lcd.print(num);
}

void lcdClearLine(int lineNumber){
  lcd.setCursor(0, lineNumber);
  lcd.print("                ");
}
