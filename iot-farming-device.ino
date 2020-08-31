#include <WiFi101.h>
#include <ThingSpeak.h>
#include "secrets.h"

int status = WL_IDLE_STATUS;
int interval = 15 * 1000; // Convert s to milliseconds
int plant_height, moisture_1hr, moisture_4hr, moisture_24hr, moistureLevel;
int soilThreshold = 300; // Wet soil 700, Dry soil 300, see https://wiki.seeedstudio.com/Grove-Moisture_Sensor/
int moisturePin = A0;
long previousMillis = 0;

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

unsigned long channel_number = SECRET_CH_ID;
const char *write_apikey = SECRET_WRITE_APIKEY;
const char *host = "maker.ifttt.com";

String iftttapikey = SECRET_IFTTT_APIKEY; // API key can be found at the applet dashboard
String pumpStatus = "auto";

// Smoothing algorithm init
const int numReadings = 10;
int readings[numReadings]; // the readings from the analog input
int readIndex = 0;		   // the index of the current reading
int total = 0;			   // the running total
int average = 0;		   // the average

WiFiServer server(80);
WiFiClient client(80);
IPAddress ip(192, 168, 1, 177);

void setup()
{
	Serial.begin(9600);
	pinMode(4, OUTPUT); // Controlling relay requires output at D4
	WiFi.config(preferredIp);
	while (status != WL_CONNECTED)
	{
		Serial.print("Attempting to connect to Network named: ");
		Serial.println(ssid); // print the network name (SSID)
		status = WiFi.begin(ssid, pass);
		// Wait 5 seconds for connection:
		delay(5 * 1000);
	}
	server.begin(); // Start the web server
}

void loop()
{
	// Anything here will run constantly
	WiFiClient client = server.available();
	runServer();
	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis > interval)
	{ // millis() based code to run code without delay(), will run every interval seconds
		previousMillis = currentMillis;

		IPAddress ip = WiFi.localIP();
		Serial.print("IP Address: ");
		Serial.println(ip);
		thingPost();
		if (average < soilThreshold && pumpStatus == "auto") // If water level is low, and pump is set to run automatically
		{
			sendMessage();
		}
		else
		{
		}
	}
}

void waterPlant()
{
	Serial.println("Watering the plant now");
	digitalWrite(4, HIGH);
	delay(3 * 1000);
	digitalWrite(4, LOW);
}

void sendMessage()
{
	if (client.connect(host, 80))
	{																												  // client.connect returns true for a successful connection with the domain
		Serial.println("IFTTT request in Progress");																  // Formatting for HTTP GET requests, IFTTT also allows POST requests
		client.println("GET https://maker.ifttt.com/trigger/moistureStatus/with/key/CHNuP7Ow5TQEzfpLHOxd6 HTTP/1.1"); // Replace iftttapikey with your own API key
		client.println("Host: www.maker.ifttt.com");
		client.println("Connection: close"); // Close connection once data has been delivered
		client.println();
		Serial.println("IFTTT request is successful!");
	}
	else
	{
		Serial.println("IFTTT request has failed.");
	}
}

void thingPost()
{
	if (average > 200) // dont send data if errored
	{
		Serial.println("Data that we are uploading: " + String(average));
		ThingSpeak.setField(1, average); // Set field corresponding to ThingSpeak data bucket
		ThingSpeak.setField(2, average);
		ThingSpeak.setField(3, average);
		int x = ThingSpeak.writeFields(channel_number, write_apikey); // Write the fields to ThingSpeak service
		if (x == 200)
		{
			Serial.println("DATA: Channel update successful."); // HTTP Request has returned 200 (OK)
		}
		else
		{
			Serial.println("DATA: Problem updating channel. HTTP error code " + String(x));
		}
	}
	else
	{
		Serial.println("Water pump is malfunctioning OR Initial setup is running!");
	}
}

void getAverage()
{
	// subtract the last reading
	total = total - readings[readIndex];
	// read from the sensor
	readings[readIndex] = analogRead(moisturePin);
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

void runServer()
{
	if (client)						  // server code
	{								  // if you get a client,
		Serial.println("new client"); // print a message out the serial port
		String currentLine = "";	  // make a String to hold incoming data from the client
		while (client.connected())
		{
			if (client.available())
			{							// if there's bytes to read from the client,
				char c = client.read(); // read a byte, then
				Serial.write(c);		// print it out the serial monitor
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
						// the content of the HTTP response follows the header:
						client.print("Click <a href=\"/A\">here</a> to toggle the pump between auto and manual<br>");
						client.print("The pump is currently set to: " + pumpStatus + "<br>");
						client.print("Click <a href=\"/B\">here</a> to water the plant<br>");
						client.print("Soil threshold is currenly: " + soilThreshold + ". Take note that 700 is humid, and 300 is dry.");
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
				{ // If 'water plant' command is triggered
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