/*

On Air Sign

Simple Web Server that creates an endpoint to toggle an LED attached to pin 8 on and off.


Based on Sketch:
 WiFi Web Server LED Blink
 created 25 Nov 2012
 by Tom Igoe
 */

#include <SPI.h>
#include <WiFiNINA.h>

#include "arduino_secrets.h" 
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key index number (needed only for WEP)

const char* ON_AIR = "GET /ON_AIR";
const char* OFF_AIR = "GET /OFF_AIR";
const char* STATE = "GET /STATE";
const char* STATE_ON = "ON";
const char* STATE_OFF = "OFF";

String state = STATE_OFF;

int status = WL_IDLE_STATUS;
WiFiServer server(80);

void setup() {
  Serial.begin(9600);      // initialize serial communication
  pinMode(9, OUTPUT);      // set the LED pin mode
  pinMode(LED_BUILTIN, OUTPUT);

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(ssid);                   // print the network name (SSID);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  server.begin();                           // start the web server on port 80
  printWifiStatus();                        // you're connected now, so print out the status
}

void loop() {
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("new client");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Check to see if the client request was "GET /H" or "GET /L":
        if (currentLine.endsWith(ON_AIR)) {
          digitalWrite(9, HIGH);               // GET /H turns the LED on
          digitalWrite(LED_BUILTIN, HIGH);
          state = STATE_ON;
          writeNoContentResponse(client);
        }
        else if (currentLine.endsWith(OFF_AIR)) {
          digitalWrite(9, LOW);                // GET /L turns the LED off
          digitalWrite(LED_BUILTIN, LOW);
          state = STATE_OFF;
          writeNoContentResponse(client);
        }
        else if(currentLine.endsWith(STATE)) {
          writeStateResponse(client, state);
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}

void writeNoContentResponse(WiFiClient client) {
  client.println("HTTP/1.1 204 No Content");
  client.println();
}

void writeStateResponse(WiFiClient client, String state) {
  client.println("HTTP/1.1 200 OK");
  client.println("content-type: text/plain; charset=UTF-8");
  client.println("server: wagner/on-air-sign");
  client.println("x-on-air: v1.0.0");
  client.print("content-length: ");
  client.println(state.length());
  client.println();
  client.println(state);
  client.println();
  client.println();
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}
