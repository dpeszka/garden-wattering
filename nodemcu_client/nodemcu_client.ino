#include <ESP8266WiFi.h>

int humidity = 0;


const char* ssid = "NODEMCU";
const char* password = "qwerty1234";
const char* host = "192.168.6.10";

WiFiClient client;

const int analogPin = A0;

int sensorValue = 0;
int outputValue = 0;

//deepsleep
const int sleepTime = 5;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(A0, INPUT);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
  }

  Serial.print("IP Address(AP): ");
  Serial.println(WiFi.localIP());
  
  sensorValue = analogRead(analogPin);

  outputValue = map(sensorValue, 0, 1024, 0, 100);
  Serial.print("humidity = ");
  Serial.println(outputValue);
  if(client.connect(host, 80)){
    String url = "/update?value=";
    url += String(outputValue);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" +
                  "Connection: keep-alive\r\n\r\n");
    delay(10);

    Serial.print("Response: ");
    while(client.available()){
      String line = client.readStringUntil('\r');
      Serial.println(line);
    }
  }
  Serial.print("going to sleep for ");
  Serial.print(sleepTime);
  Serial.println(" sec");
  ESP.deepSleep(sleepTime * 1e6);
}

void loop() {
  // put your main code here, to run repeatedly:
  

  Serial.print("sensor = ");
  Serial.print(sensorValue);
  Serial.print("\t output = ");
  Serial.println(outputValue);

  delay(1000);
}
