/*
 * Valparaiso University
 * ECE212A Auto Garden Project

TODO:
  1. Make function to check if that interval has happened yet once per loop() cycle. (Almost done currently handled in loop but not function)
  2. Change togglePump() to run until a certain amount of water has drained from a tank
  3. Define constants for all time zones
      - Instead of hours and minutes, make the constants in seconds
      - (UTC+5:30) would be 19800 seconds (18000s + 1800s)
      - (UTC-3:30) would be -12600 seconds
  4. Create catch that looks for UNIXtime value duplicates
      - If duplicate happens once, try again. If the same duplicate happens again, reset the system.
      - If two seperate duplicates happen, reset the system. 
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>

static const uint8_t D0   = 16; //Keep Travis happy comment before verifying

#define CENTRAL_STANDARD_TIME 5  // The 5 hours needed to be subtracted onto the getHours(actualTime)
#define PUMP_PIN              D0

// Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
const uint32_t seventyYears = 2208988800UL;

// Stores the watering interval in seconds thats accepted on the webpage as hours and minutes
uint32_t waterInterval;

// Network credentials
const char* ssid     = ""; // Enter SSID
const char* password = ""; // Enter Password

// Set web server port number to 80
ESP8266WebServer server(80);

/*-----UDP Declarations-----*/
WiFiUDP UDP; // Instance of WiFiUDP

IPAddress timeServerIP; // IPAddress instance that will hold the time.nist.gov NTP server address
const char* NTPServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48; // NTP package size

byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
/*---End UDP Declarations----*/

char htmlResponse[3000];

void setup() {
  pinMode(PUMP_PIN, OUTPUT); // LED
  delay(2000);

  Serial.begin(115200);
  if(startWiFi(ssid, password)) {
    // Print local IP address and start web server
    Serial.println();
    Serial.println(F("WiFi connected."));
    Serial.print(F("Local IP address: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("MAC Address: "));
    Serial.println(WiFi.macAddress());
    Serial.println(F("Public IP: "));
    GetExternalIP();
    Serial.println();
  } else {
    Serial.println(F("Connection Failed."));
    Serial.println(F("Please check the SSID and Password."));
    Serial.println(F("Ending program..."));
    exit(0);
  }

  startUDP();

  if(!WiFi.hostByName(NTPServerName, timeServerIP)) { // Get the IP address of the NTP server
    Serial.println(F("DNS lookup failed. Rebooting."));
    Serial.flush();
    ESP.reset();
  }
  Serial.print(F("Time server IP:\t"));
  Serial.println(timeServerIP);

  sendNTPpacket(timeServerIP); // Things break when I take this out so keep it

  server.on("/", handleRoot);
  server.on("/setTime", handleSetTime);
  
  server.begin();
  digitalWrite(D0, LOW);
}

unsigned long intervalNTP = 10000; // Request NTP time every 10 seconds
unsigned long lastNTPRequestSent;  // Last time an NTP request was sent
unsigned long lastNTPResponse = millis();
uint32_t timeUNIX     = 0; // Current UNIX time
uint32_t prevTimeUNIX = 0; // Previous UNIX time

unsigned long prevActualTime = 0;

void loop(){
  /*----Send and receive packet----*/
  unsigned long currentMillis = millis();
  if((currentMillis - lastNTPRequestSent)>=intervalNTP){ //Send a request every 10 seconds
    Serial.println(F("\r\nSending NTP request ..."));
    sendNTPpacket(timeServerIP); // Send an NTP request
    //delay(3000);
    byte maxRequestAttempts = 30;  // Maximum number of requests sent before error is thrown
    byte requestAttempts = 0; // Number of request sent
    uint32_t time = getTime(); // Check if an NTP response has arrived and get the (UNIX) time
    while(!time && (requestAttempts <= maxRequestAttempts)) { // 30 attempts to recieve a time
      //sendNTPpacket(timeServerIP); // Send an NTP request
      delay(100); //Slight delay
      time = getTime();
      requestAttempts++;
      Serial.print("Request #");
      Serial.println(requestAttempts);
      if(requestAttempts >= maxRequestAttempts) { // If no response is recieved within maxRequestSent times
        Serial.println(F("Error! No response."));
      }
    }
    
    if(time) { // If a new timestamp has been received
      timeUNIX = time;
      Serial.print(F("NTP response: "));
      Serial.println(timeUNIX); // Print the raw response
      Serial.print(F("Actual Time: "));
      Serial.print(getHours(timeUNIX));
      Serial.print(F(":"));
      Serial.print(getMinutes(timeUNIX));
      Serial.print(F(":"));
      Serial.println(getSeconds(timeUNIX));
    } else if ((currentMillis - lastNTPResponse) > 3600000) {
      Serial.println(F("More than 1 hour since last NTP response. Rebooting."));
      Serial.flush();
      ESP.reset();
    }
    
    lastNTPRequestSent = millis();
  }
  /*----End Send packet----*/

  /*----Check water interval----*/
  if(((timeUNIX - prevTimeUNIX) >= waterInterval) && (waterInterval != 0)) { // If the time interval has passed then run the pump
    Serial.println(F("Pump Toggled"));
    Serial.print(F("Delta NTP: "));
    Serial.println(timeUNIX - prevTimeUNIX);
    togglePump(); // Turn the pump on
    prevTimeUNIX = timeUNIX; // Set the previous NTP time as the current
  }
  /*----End Check water interval----*/
  
  /*----Main server handler----*/
  server.handleClient();
  /*--End Main server handler--*/
}

/*
 * <Name>handleRoot</Name>
 * <Desc>Displays the root HTML page</Desc>
 * <Params>None</Params>
 * <Return>void</Return>
*/
void handleRoot() {
  snprintf(htmlResponse, 3000,
"<!DOCTYPE html>"
"<html>"

"<head>"
  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
  "<meta http-equiv=\"refresh\" content=\"30\"/>"
  "<link rel=\"icon\" href=\"data:,\">"
  "<style>"
    "p.groove {border-style: groove;}"
    "p.none {border-style: none;}"
    "p.mix {border-style: groove none none none;}"
  "</style>"
"</head>"

"<body style=\"background-color:rgb(42, 255, 62);text-align:center; font-family: Helvetica;\">"
  "<br>"
  "<h1>Auto Garden ESP8266 Web Server</h1>"
  "<div>"
    "<div>"
      "<br>"
      "<p>Please enter the number of hours between each watering:</p>"
      "<input type=\"text\" name=\"hours\" id=\"hours\" size=\"3\" value=\"0\" autofocus> Hours"
      "<br><input type=\"text\" name=\"mins\" id=\"mins\" size=\"3\" value=\"0\" autofocus> Minutes"
      "<div>"
        "<br><button id=\"set_btn\">Set Time</button>"
      "</div>"
    "</div>"
    
    "<br>"
    "<br>"
    "<br>"
    "<br>"

    "<div>"
      "<p class=\"mix\"> </p>"
      "<br>"
      "<h1> About This Project </h1>"
      "<p>This project takes the hastle out of remembering to water your plants every day. Input any given amount of time and when that specific time elapses, a pre-determined amount of water will be released onto your plant. </p>"
      "<br>"
    "</div>"
    
    "<div>"
      "<p class=\"mix\"> </p>"
      "<br>"
      "<h1> About This Project </h1>"
      "<p>This project was created by Chris Gutschlag, Allen Poe, and Gina Bernico. </p>"
    "</div>"
  "</div>"
  "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.11.3/jquery.min.js\"></script>"
  "<script>"
    "$(document).ready(function(){"
      "$(\"#set_btn\").click(function(event){"
        "event.preventDefault();"
        "var hours;"
        "var mins;"
        "hours = $(\"#hours\").val();"
        "mins = $(\"#mins\").val();"
        "$.get(\"/setTime?hours=\"+hours+\"&mins=\"+mins, function(data){console.log(data);});"
      "});"
    "});"
  "</script>"
"</body>"

"</html>");
  
  server.send(200, "text/html", htmlResponse);
}

/*
 * <Name>handleSetTime</Name>
 * <Desc>Handles the GET request from the user when the "Set Time" button is pressed on the root page</Desc>
 * <Params>None</Params>
 * <Return>void</Return>
*/
void handleSetTime() {
  int hours   = server.arg("hours").toInt(); // Grabs the string from the hours input box on the webpage and converts it to an integer
  int minutes = server.arg("mins").toInt();  // Grabs the string from the minutes input box on the webpage and converts it to an integer
  
  if (!hours && !minutes){ // If both hours and minutes are equal to 0
    Serial.println(F("Watering time cannot be 0!"));
  } else if((hours == 0) && (minutes < 1)) { // The watering interval must be greater than or equal to 30 minutes. (Currently 1 minute for debugging.)
    Serial.println(F("Watering time must be greater than or equal to 30 minutes!"));
  } else { // Set the watering interval.
    setWaterInterval(hours, minutes);
    Serial.print(F("Hours: "));
    Serial.println(hours);
    Serial.print(F("Minutes: "));
    Serial.println(minutes);
    Serial.print(F("waterInterval: "));
    Serial.println(waterInterval);
  }
}

/*
 * <Name>startWiFi</Name>
 * <Desc>Starts the wifi connection</Desc>
 * <Params>ssid : SSID of the network, passwird : Password of the network</Params>
 * <Return>0 : If failed connection, 1 : If successful connection</Return>
*/
bool startWiFi(const char* ssid, const char* password) {
  int startStamp    = millis(); // Reconds the number of milliseconds at the begining of the connection attempt
  int currentStamp; 
  int cutOffTime    = 30000; // 30 second cut off time
  // Connect to Wi-Fi network with SSID and password
  Serial.print(F("Connecting to "));
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    currentStamp = millis();
    if((currentStamp - startStamp) >= cutOffTime) {
      return 0; // Took too long to connect
    } else {
      delay(500);
      Serial.print(".");
    }
  }
  return 1; // Successful connection
}

/*
 * <Name>startUDP</Name>
 * <Desc>Starts the UDP connection</Desc>
 * <Params>None</Params>
 * <Return>void</Return>
*/
void startUDP() {
  Serial.println(F("Starting UDP"));
  UDP.begin(123);                          // Start listening for UDP messages on port 123
  Serial.print(F("Local port:\t"));
  Serial.println(UDP.localPort());
  Serial.println();
}

/*
 * <Name>getTime</Name>
 * <Desc>Reads the returned packet and returns the time in UNIX time (seconds)</Desc>
 * <Params>None</Params>
 * <Return>UNIXTime : The NTPTime subtracted by 70 years in seconds</Return>
*/
uint32_t getTime() {
  if (UDP.parsePacket() == 0) { // If there's no response (yet)
    return 0;
  } else {
    UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  }

  // The returned time is found from byte 40 through 43 of the returned 48 byte packet
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
  
  // Convert NTP time to a UNIX timestamp:
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  return UNIXTime;
}

/*
 * <Name>sendNTPpacket</Name>
 * <Desc>Initializes and sends the NTP packet to the provided server IP address</Desc>
 * <Params>address : The IP address of the server that the packet is being sent to</Params>
 * <Return>void</Return>
*/
void sendNTPpacket(IPAddress& address) {
  memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  
  // Initialize values needed to form NTP request
  NTPBuffer[0] = 0b11100011;   // LI, Version, Mode
  
  // send a packet requesting a timestamp:
  UDP.beginPacket(address, 123); // NTP requests are to port 123
  UDP.write(NTPBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}

// Thank you martinayotte https://www.esp8266.com/viewtopic.php?f=6&t=12782
/*
 * <Name>GetExternalIP</Name>
 * <Desc>Gets the external IP of the system</Desc>
 * <Params>None</Params>
 * <Return>void</Return>
*/
void GetExternalIP()
{
  WiFiClient client;
  if (!client.connect("api.ipify.org", 80)) {
    Serial.println(F("Failed to connect with 'api.ipify.org' !"));
  }
  else {
    int timeout = millis() + 5000;
    client.print("GET /?format=json HTTP/1.1\r\nHost: api.ipify.org\r\n\r\n");
    /*while (client.available() == 0) {
      if (timeout - millis() < 0) {
        Serial.println(F(">>> Client Timeout !"));
        client.stop();
        return;
      }
    }*/
    int size;
    while ((size = client.available()) > 0) {
      uint8_t* msg = (uint8_t*)malloc(size);
      size = client.read(msg, size);
      Serial.write(msg, size);
      free(msg);
    }
  }
}

/*
 * <Name>getSeconds</Name>
 * <Desc>Converts the UNIX time into 0-60 seconds</Desc>
 * <Params>UNIXTime : The UNIX time stamp</Params>
 * <Return>UNIXTime % 60 : 0-60 seconds</Return>
*/
inline int getSeconds(uint32_t UNIXTime) {
  return UNIXTime % 60;
}

/*
 * <Name>getMinutes</Name>
 * <Desc>Converts the UNIX time into 0-60 minutes</Desc>
 * <Params>UNIXTime : The UNIX time stamp</Params>
 * <Return>UNIXTime/60 % 60 : 0-60 minutes</Return>
*/
inline int getMinutes(uint32_t UNIXTime) {
  return UNIXTime / 60 % 60;
}

/*
 * <Name>getHours</Name>
 * <Desc>Converts the UNIX time into 0-23 hours</Desc>
 * <Params>UNIXTime : The UNIX time stamp</Params>
 * <Return>hours : 0-23 hours</Return>
*/
inline int getHours(uint32_t UNIXTime) {
  int hours = (UNIXTime / 3600 % 24)-CENTRAL_STANDARD_TIME; // Convert the UNIX time stamp to hours and then subtract the central standard time zone from it

  if(hours < 0) { // If the hours are negative, add it to 24 (since it is negative) to get the correct hours
    //Serial.println("Hours converted");
    return 24 + hours;
  } else {
    return hours;
  }
}

/*
 * <Name>setWaterInterval</Name>
 * <Desc>Times in the hours and minutes, converts them to seconds, and adds them together to set the water interval</Desc>
 * <Params>inHours : Input hours, inMinutes : Input minutes</Params>
 * <Return>void</Return>
*/
void setWaterInterval(int inHours, int inMinutes) {
  waterInterval = (inHours*3600)+(inMinutes*60);
}

void togglePump() {
  /*
  static bool pumpState = 0;
  Serial.print("Pump State: ");
  Serial.println(pumpState);
  digitalWrite(D1, !pumpState);
  */
  digitalWrite(PUMP_PIN, HIGH);
  delay(5000);
  digitalWrite(PUMP_PIN, LOW);
}
