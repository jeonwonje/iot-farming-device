#include <WiFi101.h>
#include "secrets.h"

char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
char host[] = "maker.ifttt.com";
char thingSpeakAddress[] = "api.thingspeak.com";

int status = WL_IDLE_STATUS;
int interval = 15 * 1000; // Convert s to milliseconds
int soilThreshold = 300;
const int numReadings = 10;
int readings[numReadings]; // the readings from the analog input
int readIndex = 0;       // the index of the current reading
int total = 0;         // the running total
int average = 300;

String pumpStatus = "auto";
String APIKey = SECRET_APIKEY;

long previousMillis = 0;

WiFiServer server(80);
WiFiClient client;

void setup() {
  Serial.begin(9600);
  pinMode(4, OUTPUT); // Controlling relay requires output at D4
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(1000);
  }
  server.begin(); // Start the web server
}

void loop() {
  while (client.available()) {
    Serial.write(client.read());
  }
  runServer();
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > interval)
  { // millis() based code to run code without delay(), will run every interval seconds
    previousMillis = currentMillis;

    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);
    getAverage();
    updateThingSpeak("field1=" + String(average));
    if (average < soilThreshold && pumpStatus == "auto") // If water level is low, and pump is set to run automatically
    {
      sendIFTTT();
      waterPlant();
    }
    else
    {
      Serial.println("Plant is already watered!");
    }
  }
}

void sendIFTTT() {
  client.stop();
  if (client.connect(host, 80)) {
    Serial.println("connecting...");
    client.println("GET https://maker.ifttt.com/trigger/moistureStatus/with/key/" + SECRET_IFTTT_APIKEY +"HTTP/1.1");
    client.println("Host: www.maker.ifttt.com");
    client.println("Connection: close"); // Close connection once data has been delivered
    client.println();
    Serial.println("IFTTT request is successful!");
  }
  else {
    Serial.println("Connection failed");
  }
}

void waterPlant()
{
  Serial.println("Watering the plant now");
  digitalWrite(4, HIGH);
  delay(2 * 1000);
  digitalWrite(4, LOW);
}

void runServer()
{
  WiFiClient client = server.available();
  if (client)             // server code
  { // if you get a client,
    Serial.println("new client"); // print a message out the serial port
    String currentLine = "";    // make a String to hold incoming data from the client
    while (client.connected())
    {
      if (client.available())
      { // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c);    // print it out the serial monitor
        if (c == '\n')
        { // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print("");
            client.print("<!DOCTYPE html>");
            client.print("<head><style> * {font-family: Verdana;font-size:30px;padding:15px;}</style></head>");
            client.print("");
            client.print("Click <a href=\"/A\">here</a> to toggle the pump between auto and manual<br>");
            client.print("The pump is currently set to: " + pumpStatus + "<br>");
            client.print("Click <a href=\"/B\">here</a> to water the plant<br>");
            client.print("Soil threshold is currenly: " + String(soilThreshold) + ". Take note that 700 is humid, and 300 is dry.<br>");
            client.print("<a href=\"/C\">Increase</a> moisture threshold by 25<br>");
            client.print("<a href=\"/D\">Decrease</a> moisture threshold by 25<br>");
            client.println();
            break;
          }
          else
          {
            currentLine = "";
          }
        }
        else if (c != '\r')
        {
          currentLine += c;
        }
        if (currentLine.endsWith("GET /A"))
        {
          if (pumpStatus == "auto") // GET /A turns the pump on auto / manual mode
          {
            pumpStatus = "manual";
          }
          else
          {
            pumpStatus = "auto";
          }
        }
        if (currentLine.endsWith("GET /B"))
        {
          waterPlant();
        }
        if (currentLine.endsWith("GET /C"))
        {
          soilThreshold += 25;
        }
        if (currentLine.endsWith("GET /D"))
        {
          soilThreshold -= 25;
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}

void getAverage()
{
  // subtract the last reading
  total = total - readings[readIndex];
  // read from the sensor
  readings[readIndex] = analogRead(A0);
  // add the reading to the total
  total = total + readings[readIndex];
  // advance to the next position in the array:
  readIndex = readIndex + 1;

  if (readIndex >= numReadings)
  {
    readIndex = 0;
  }
  // calculate the average:
  average = total / numReadings;
  Serial.println(average);
}

void updateThingSpeak(String tsData) {
  if (client.connect(thingSpeakAddress, 80)) {
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + APIKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(tsData.length());
    client.print("\n\n");
    client.print(tsData);
    if (client.connected()) {
      Serial.println("Connecting to ThingSpeak...");
      Serial.println();
    }
  }
  client.stop();
}
