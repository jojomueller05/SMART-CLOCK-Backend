#include <Arduino.h>

//Public Librarys:
#include <Wire.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include "rgb_lcd.h"
#include <RTClib.h> 
#include <SD.h> 
#include <ArduinoJson.h>


//Own Modules:
#include "LCD.h"
#include "RTC.h"
#include "MICROSD.h"

//LCD settings:
rgb_lcd lcd;
const int colorR = 192;
const int colorG = 81;
const int colorB = 209;

//rtc settings:
RTC_DS3231 rtc;

//SD Card settigns:
const int SDCARD = 4;

//BUZZER Settings:
const int BUZZER = 1;

//ALARMBUTTON Settings:
const int ALARMBUTTON = 6;

//wifi/webserver settings:
// visit http://192.168.4.1
char ssid[] = "SMART CLOCK";        // Dein Netzwerk-SSID (Name)
char pass[] = "12345678";
int keyIndex = 0; 
int status = WL_IDLE_STATUS;
WiFiServer server(80);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  //lcd init:
  lcd.begin(16, 2); // Initialize LCD here
  lcd.setRGB(colorR, colorG, colorB);
  defaultLCDValue();

  //rts init:
  Wire.begin();
    //check if rtc is ready
    if (!rtc.begin()) {
      Serial.println("RTC konnte nicht initialisiert werden!");
      while (1);
    }

  // SD card init:
  if (!SD.begin(SDCARD)) {
    Serial.println("SD-Karte konnte nicht initialisiert werden.");
    while (1);
  }
  
  //button init:
  pinMode(ALARMBUTTON, INPUT_PULLUP);

  //BUZZER init:
  pinMode(BUZZER, OUTPUT);

  //wifi / webserver setup:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Kommunikation mit dem WiFi-Modul fehlgeschlagen!");
    // Nicht fortsetzen
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Bitte aktualisiere die Firmware");
  }

  Serial.print("Erstelle einen Access Point mit dem Namen: ");
  Serial.println(ssid);

  status = WiFi.beginAP(ssid, pass);

  if (status != WL_AP_LISTENING) {
    Serial.println("Erstellen des Access Points fehlgeschlagen");
    // Nicht fortsetzen
    while (true);
  }

    // Warte 10 Sekunden auf eine Verbindung:
  delay(10000);

  // Starte den Webserver auf Port 80
  server.begin();


}

void updateLCDScreen(){
    // put your main code here, to run repeatedly:

  //LCD Screen:
  //get current date:
  char timeChar[6];
  getCurrentTime(timeChar);

  //get current time:
  char dateChar[11];
  getCurrentDate(dateChar);
  
  //getJsonData:
  String settings[3];
  getJsonData(settings);
  
  //settings date & time
  String dateString = settings[0];
  String timeString = settings[1];
  String clockSet = settings[2];

  
  if (clockSet == "true"){
    //updateLCD Screen:
    updateLCD(timeChar, dateChar, true);    
  } else {
      //updateLCD Screen:
      updateLCD(timeChar, dateChar, false);
  }
}
void loop() {
  // put your main code here, to run repeatedly:

  //LCD Screen:
  //get current date:
  char timeChar[6];
  getCurrentTime(timeChar);

  //get current time:
  char dateChar[11];
  getCurrentDate(dateChar);
  
  //getJsonData:
  String settings[3];
  getJsonData(settings);
  
  //settings date & time
  String dateString = settings[0];
  String timeString = settings[1];
  String clockSet = settings[2];

  
  if (clockSet == "true"){
    //updateLCD Screen:
    updateLCD(timeChar, dateChar, true);    
  } else {
      //updateLCD Screen:
      updateLCD(timeChar, dateChar, false);
  }

  // if settings == now run buzzer until button clicked.
  if (String(dateChar) == dateString && String(timeChar) == timeString && clockSet == "true"){

    while (digitalRead(ALARMBUTTON) == HIGH){
      //run buzzer
      digitalWrite(BUZZER, HIGH);
      delay(1000);
      digitalWrite(BUZZER, LOW);
      delay(1000);
      Serial.println();
      Serial.println("Buzzer is runing");
      Serial.println();
      //update lcd screen
      updateLCDScreen();
 
    }

    if (digitalRead(ALARMBUTTON) == LOW){
      Serial.println();
      Serial.println("Button pressed!");
      Serial.println();
      updateJson(dateString, timeString, false);
    }        
 

    // if there is a date and time value but clock isn't set
  } else if (dateString != "" && timeString != "" && clockSet == "false"){
    
    // if current date and time doesn't match config 
      dateString = incrementDate(dateString);
      updateJson(dateString, timeString, true);
  }

  //webserver routing:
  WiFiClient client = server.available();

  if (client) {
    Serial.println("Neuer Client verbunden");
    String request = client.readStringUntil('\r');

    if (request.indexOf("GET / ") != -1 || request.indexOf("GET /?error=none ") != -1) {
  // Wenn die Anforderung ein GET auf / ist, HTML zurückgeben
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();

  getFileContent("index.htm", client);

  // Schließe die Verbindung zum Client
  client.stop();
}
else if (request.indexOf("GET /settings ") != -1 || request.indexOf("GET /settings?error=none ") != -1) {
        // Wenn die Anforderung ein GET auf /settings ist, HTML zurückgeben
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println();

        getFileContent("settings.htm", client);

        // Schließe die Verbindung zum Client
        client.stop();
    }

 else if (request.indexOf("POST /get-data ") != -1) {
    // Wenn die Anforderung ein POST auf /get-data ist, JSON zurückgeben
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println();

    getFileContent("settings.txt", client);
    
} else if (request.indexOf("POST / ") != -1) {
  // Wenn die Anforderung ein POST auf / ist
  // Hier analysieren wir den HTTP-Header, um die POST-Daten zu extrahieren
  int contentLength = 0;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("Content-Length: ")) {
      contentLength = line.substring(16).toInt();
    }
    if (line == "\r") {
      break; // Leerzeile am Ende des Headers
    }
  }

  // Hier lesen wir den HTTP-Body (Request-Daten)
String body = client.readStringUntil('\r'); // Lies die Daten bis zum Ende des Requests

// Analysiere die POST-Daten, um date, time und set zu extrahieren
String dateValue = "";
String timeValue = "";
String setString = "";
bool setValue = false;
int dataIndex = body.indexOf("date=");
int timeIndex = body.indexOf("time=");
int setIndex = body.indexOf("set=");

if (dataIndex != -1 && timeIndex != -1 && setIndex != -1) {
    dateValue = body.substring(dataIndex + 5, timeIndex - 1);
    timeValue = body.substring(timeIndex + 5, setIndex - 1);
    setString = body.substring(setIndex + 4);
    
    
    // Umwandeln von "setString" in einen boolschen Wert
    if (setString.equalsIgnoreCase("true")) {
        setValue = true;
    }
}

  // Gib die extrahierten Daten aus
  Serial.println("Date: " + dateValue);
  Serial.println("Time: " + timeValue);
  Serial.println("Set:" + setString);
  
  if (dateValue != "" && timeValue != ""){
  //Format date and time:
  String validDateString = formatDate(dateValue);
  String validTimeString = formatTime(timeValue);

  // Gib die extrahierten, neuen Daten aus
  Serial.println("Date: " + validDateString);
  Serial.println("Time: " + validTimeString);

  updateJson(validDateString, validTimeString, setValue);

  } else {
  
  updateJson(dateValue, timeValue, setValue);

  }

  // Sende eine HTTP-Weiterleitung und beende die Verbindung
  client.println("HTTP/1.1 302 Found");
  client.println("Location: /?error=none");
  client.println();
  client.stop();
}
 else if (request.indexOf("POST /settings ") != -1) {
  // Wenn die Anforderung ein POST auf / ist
  // Hier analysieren wir den HTTP-Header, um die POST-Daten zu extrahieren
  int contentLength = 0;
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line.startsWith("Content-Length: ")) {
      contentLength = line.substring(16).toInt();
    }
    if (line == "\r") {
      break; // Leerzeile am Ende des Headers
    }
  }

// Hier lesen wir den HTTP-Body (Request-Daten)
String body = client.readStringUntil('\r'); // Lies die Daten bis zum Ende des Requests

 // Analysiere die POST-Daten, um date, time und set zu extrahieren
String dateValue = "";
String timeValue = "";

int dataIndex = body.indexOf("date=");
int timeIndex = body.indexOf("time=");


if (dataIndex != -1 && timeIndex != -1) {
    dateValue = body.substring(dataIndex + 5, timeIndex - 1);
    timeValue = body.substring(timeIndex + 5);
}

  // Gib die extrahierten Daten aus
  Serial.println("Date: " + dateValue);
  Serial.println("Time: " + timeValue);

  
  //Format date and time:
  String validDateString = formatDate(dateValue);
  String validTimeString = formatTime(timeValue);

  // Gib die extrahierten, neuen Daten aus
  Serial.println("Date: " + validDateString);
  Serial.println("Time: " + validTimeString);

  setRTCDateTime(rtc, validDateString, validTimeString);
  // Sende eine HTTP-Weiterleitung und beende die Verbindung
  client.println("HTTP/1.1 302 Found");
  client.println("Location: /settings?error=none");
  client.println();
  client.stop();
}

else {
      client.println("HTTP/1.1 404 Not Found");
      client.println("Content-Type: text/html");
      client.println();
      getFileContent("404.htm", client);
    }

    client.stop();
    Serial.println("Client getrennt");
  }
  


}

