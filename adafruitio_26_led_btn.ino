#include <WiFi.h>  //Connecting Wifi 
#include <HTTPClient.h>  // To use Http Client 
#include <TimeLib.h>   // To convert time in minutes and seconds
#include "config.h"  // For adafruit
#include <NTPClient.h>   // To get the current Time 
#include <Arduino_JSON.h>  // Jsonify the data
#include <WiFiUdp.h>   // Get global timezone
#include <M5StickC.h>  // For M5stick 
#include <analogWrite.h>  // Analog writing 

const int ldrMainPin=36;
const int ldrFaultyPin=33;
const int ledFaultyPin=0;
const int ledMainPin=26;

int ldrMainStatus;
int ldrFaultyStatus;
String ldrMainValue;
String ldrFaultyValue;

String cloudValue1;  // Street Light ID 
String cloudValue2;  // Location 
String event = "Faulty_Light_Detection";
String iftttApiKey = "o43aCCtwB4tfQxlkMeGaPxxW3KX-Kn1r5xY0UN5ruze";

bool statsPresent=false; //Adafruit stats
bool statsPrevious=false; // Adafruit stats 
AdafruitIO_Feed *led = io.feed("led");
AdafruitIO_Feed *ldr = io.feed("ldr");

int currentMinute=0;
int extractedHour;
int extractedMinute;
int sunriseTime;
int sunsetTime;
float lat;
float lon;
String city="Hof";
String openWeatherMapApiKey = "6ba9999cfc6e8a8d09ca8a9b457a79c3";

String jsonBuffer;
JSONVar myObject;

const char* ssid     = "hmtkpd";
const char* password = "nVgm62yYXaLBcQ3W";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String formattedDate;
String dayStamp;
String timeStamp;


void setup() 
{
  M5.begin();
  io.connect(); // Adafruit
  M5.Lcd.setRotation(3);
  Serial.begin(115200);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  pinMode(ldrMainPin, INPUT);
  pinMode(ldrFaultyPin, INPUT);
  
  pinMode(ledFaultyPin, OUTPUT);
  pinMode(ledMainPin, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
  
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(7200); // Berlin Time 
  }


void loop() 
{
  io.run();
  if(WiFi.status()== WL_CONNECTED){
      
      String serverPath = "https://api.openweathermap.org/data/2.5/weather?q="+city+"&appid="+openWeatherMapApiKey;
      String jsonBuffer = httpGETRequest(serverPath.c_str());
      Serial.println(serverPath.c_str());
      Serial.println(jsonBuffer);
      myObject = JSON.parse(jsonBuffer);
      Serial.println(myObject);
      if (JSON.typeof(myObject) == "undefined") 
      {
        Serial.println("Parsing input failed!");
        return;
      }
      sunriseTime=(JSON.stringify(myObject["sys"]["sunrise"])).toInt();
      sunsetTime=(JSON.stringify(myObject["sys"]["sunset"])).toInt();
      lon=(JSON.stringify(myObject["coord"]["lon"])).toFloat();
      lat=(JSON.stringify(myObject["coord"]["lat"])).toFloat();
      Serial.print("Sunrise Hour:");
      Serial.println(hour(sunriseTime+7200));
      Serial.print("Sunset Hour:");
      Serial.println(hour(sunsetTime+3600));
      } else {
      Serial.println("WiFi Disconnected");
      }
 
  while(!timeClient.update()) 
  {
    timeClient.forceUpdate();
  }
  
  ldrMainStatus = analogRead(ldrMainPin);
  ldrMainValue=String(ldrMainStatus);
  M5.Lcd.setCursor(10,20);
  M5.Lcd.print("Main LDR Value:");
  M5.Lcd.println(ldrMainValue+"  ");
  
  Serial.print("Main LDR Value:");
  Serial.println(ldrMainValue);
  getTime();
  
  if(currentMinute!=extractedMinute)
  {
   ldr->save(ldrMainStatus); 
   currentMinute=extractedMinute;
  }
  ldrFaultyStatus = analogRead(ldrFaultyPin);
  ldrFaultyValue=String(ldrFaultyStatus);
  Serial.print("FAULTY LDR Value:");
  Serial.println(ldrFaultyValue);
  if(extractedHour>=(hour(sunriseTime)) && extractedHour<=(hour(sunsetTime)))           
  {
    
    Serial.println("Morning");
    if(ldrMainStatus>1000 && ldrMainStatus<5000) {
      // Considering Indoors Brightness as per standard defined values of LDR 
      ledOff();
    }
    else {
      ledOn();
    }
   
  }  
  else                                  
  {
    Serial.println("Night");
    if(ldrMainStatus<1000) {
      ledOn();
      checkFaulty();
    } else {
      ledOff();
    }
   
  }
}

void ledOn()
{
 Serial.println("Switching LED ON");
 digitalWrite(ledFaultyPin, HIGH); // To show Faulty LED 
 digitalWrite(ledMainPin, HIGH);
 M5.Lcd.setCursor(10,40);
 M5.Lcd.println("SunLight Intensity Low ");
 M5.Lcd.setCursor(10,60);
 M5.Lcd.println("LED ON ");
 statsPresent=true;
 if(statsPresent != statsPrevious)
 {
    led->save(1);
    statsPrevious=true;
 }  
}

void ledOff()
{
 Serial.println("Swiching LED OFF");
 digitalWrite(ledFaultyPin, LOW);
 digitalWrite(ledMainPin, LOW);
 M5.Lcd.setCursor(10,40);
 M5.Lcd.println("SunLight Intensity High");
 M5.Lcd.setCursor(10,60);
 M5.Lcd.println("LED OFF");
 statsPresent=false;
 if(statsPresent != statsPrevious)
 {
 led->save(0);
 statsPrevious=false;
 }
  }

void checkFaulty() 
{
    
    Serial.println("Checking Faulty LED");
    
      if(ldrFaultyStatus<=300)    //LED is not on
      {
        Serial.println("Faulty LED FOUND");
        cloudValue1="LEDID0001";
        cloudValue2=String(lat)+","+String(lon); 
        sendButtonTrigger();
        Serial.println(cloudValue1);
        Serial.println(cloudValue2);
       }
       else
       {
        M5.Lcd.setCursor(10,60);
        M5.Lcd.println("LED ON ");
        statsPresent=true;
        if(statsPresent != statsPrevious)
         {
         led->save(1);
         statsPrevious=true;
         }
       }
       
}

void getTime()
{
  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);
  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  Serial.print("DATE: ");
  Serial.println(dayStamp);
  // Extract time
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
  Serial.print("TIME: ");
  Serial.println(timeStamp);
  //Extract Hour
  Serial.print("HOUR: ");
  extractedHour=timeStamp.substring(0, 2).toInt();
  Serial.println(extractedHour);
  Serial.print("MINUTE: ");
  extractedMinute=timeStamp.substring(3, 5).toInt();
  Serial.println(extractedMinute);
  
}


void sendButtonTrigger() {
  // Send an HTTP GET request
  if (WiFi.status() == WL_CONNECTED) 
  {
    String serverPath = "https://maker.ifttt.com/trigger/" + event + "/with/key/" + iftttApiKey + "?value1=" +cloudValue1+"&"+"value2="+cloudValue2;
    Serial.println(serverPath);
    jsonBuffer = httpGETRequest(serverPath.c_str());
    Serial.println(jsonBuffer);
    JSONVar myObject = JSON.parse(jsonBuffer);
    // JSON.typeof(jsonVar) can be used to get the type of the var
    if (JSON.typeof(myObject) == "undefined") 
    {
      Serial.println("Parsing input failed!");
      return;
    }
    Serial.print("JSON object = ");
    Serial.println(myObject);
  }
  else 
  {
    Serial.println("WiFi Disconnected");
  }
}

String httpGETRequest(const char* serverName) 
{
  HTTPClient http;
  // Your IP address with path or Domain name with URL path
  http.begin(serverName);
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  String payload = "{}";

  if (httpResponseCode > 0) 
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else 
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}
