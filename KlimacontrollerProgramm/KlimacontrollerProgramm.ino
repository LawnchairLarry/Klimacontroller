  /**********************************TO DO********************************
 * - andere IDE suchen
 * - Wlan Verbindung benutzerfreundlicher machen
 * - Klimaparametereingabe benutzerfreundlicher machen
 * - Versorgungsspannung auf Lüfterspannung angleichen
 * - PWM fähigerer Lüfter wählen oder "Anstupser" programmieren ### Gelöst: Dickerer Elko
 * - PWM Einschaltschwelle einstellen
 * - EMV Verträglichkeit?
 * - I Regler?
 ***********************************************************************/

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SPIFFS.h>

#define SEALEVELPRESSURE_HPA (1013.25) // Durchschnittliches Luftdruckniveau auf Meereshöhe
#define FanPWMoutputPin 2
#define FanPWMfreq 1000
#define FanPWMChan 0
#define HeaterPin 18
#define HumidifyerPin 19

//const char* SSID = "Vodafone Homespot Deluxe";
//const char* PASSWORD = "NotSuperSafe";
//const char* SSID = "Vodafone-EB14","NextGen";
//const char* PASSWORD = "SaTi#2819#","DasistdasPasswortfuermeineFRITZ!Box!";
const char* SSID = "NextGen";
const char* PASSWORD = "DasistdasPasswortfuermeineFRITZ!Box!";
float Temperature = 0;
float TargetTemperature = 25.0;
float TemperatureDiff = 0;
float Temperature_P_factor = 120;  // Proportionalitätsfaktor
float TemperatureHysteresis = -1;  // Temperatur Hysterese um Flackern zu vermeiden
float HumidityHysteresis = 2;
float Humidity = 0.0;
float TargetHumidity = 50;
float HumidityDiff = 0;
uint32_t DutyCycle = 0;

//-----------------------------------Functions---------------------------------------------
void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  // calculate duty, 8191 from 2 ^ 13 - 1
  uint32_t duty = (8191 / valueMax) * min(value, valueMax);

  // write duty to LEDC
  ledcWrite(channel, duty);
}

//--------------------------------Object creating------------------------------------
AsyncWebServer server(80);    // erstellt ein Websererobjekt
Adafruit_BME280 bme;          // I2C

//-----------------------------------Setup----------------------------------------
void setup(){
  Serial.begin(115200);

  if(!SPIFFS.begin(true)){
    Serial.println("Dateisystem konnte nicht initialisiert werden.");
    return;
  }

//Sensor initialisierung
  Serial.println("Sensor wird initialisiert.");
  unsigned status;
  status = bme.begin(0x76);
  if (!status) {
      Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
      Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
      Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
      Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
      Serial.print("        ID of 0x60 represents a BME 280.\n");
      Serial.print("        ID of 0x61 represents a BME 680.\n");
      while (1) delay(10);
  }
  printValues();

//PWM Setup
  ledcSetup(0,FanPWMfreq,13);
  ledcAttachPin(FanPWMoutputPin,0);

  
/*
 // Start Acces Point
  WiFi.softAP(SSID, PASSWORD);
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.softAPIP());
*/

  // Connect to Wi-Fi
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi");
  }
  
  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());
  
  server.on("/gauge.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(SPIFFS, "/gauge.min.js");
  });
 
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(SPIFFS, "/index.html", String(), false, replaceVariable);
  });
 
  server.begin();

  pinMode(HeaterPin,OUTPUT);
  pinMode(HumidifyerPin,OUTPUT);
}

//--------------------------Main--------------------------------------------
void loop() {
  delay(1000);         //Regelschleife wird einmal pro Sekunde ausgeführt
  Humidity = bme.readHumidity();  //Luftfeuchte einlesen
  Temperature = bme.readTemperature();  //Temperatur einlesen
  printValues();  //Messwerte ausgeben
  HumidityDiff = Humidity - TargetHumidity; //Luftfeuchtedifferenz feststellen
  TemperatureDiff = Temperature - TargetTemperature;  //Temperaturdifferenz feststellen

  Serial.print("Temperature Difference: "); //Für Debugzwecke ausgeben
  Serial.println(TemperatureDiff);
  if(TemperatureDiff > 0){                // Wenn Temperatur Differenz größer null...
    digitalWrite(HeaterPin,LOW);          // Heizung ausschalten
    DutyCycle = TemperatureDiff * Temperature_P_factor; //Duty Cycle berechnen
  }
  else if(TemperatureDiff < TemperatureHysteresis){   // Wenn Hysterese unterschritten... 
    digitalWrite(HeaterPin,HIGH);                     // dann Heizung einschalten
  }
  ledcAnalogWrite(0,DutyCycle); // berechneten Duty Cycle an PWL Kanal 0 übergeben
  Serial.print("DutyCycle: ");  // Duty Cycle ausgeben für Debug Zwecke
  Serial.println(DutyCycle);

  Serial.print("Humidity difference: ");  // Für Debugzwecke ausgeben
  Serial.println(HumidityDiff);
  if(HumidityDiff < (HumidityHysteresis *(-1))){  // Luftfeuchte Zweipunktregelung
    digitalWrite(HumidifyerPin,HIGH);
  }
  else if (HumidityDiff > (HumidityHysteresis)){
    digitalWrite(HumidifyerPin,LOW);
  }
}
//---------------------------Some more Functions--------------------------------------

String replaceVariable(const String& var) {
  if (var == "HUMIDITY")
    return String(bme.readHumidity(), 2);
  if (var == "TEMPERATURE")
    return String(bme.readTemperature(), 2);
  return String();
}

void printValues() {
    Serial.print("Temperature = ");
    Serial.print(bme.readTemperature());
    Serial.println(" °C");

    Serial.print("Pressure = ");

    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(" hPa");

    Serial.print("Approx. Altitude = ");
    Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
    Serial.println(" m");

    Serial.print("Humidity = ");
    Serial.print(bme.readHumidity());
    Serial.println(" %");

    Serial.println();
}
