/*******************************************************************************
* esp-env-monitor
* Arduino program that uses the Sparkfun ESP8266 Thing Dev board
* to send analog values (temp, light) to Exosite using SSL
* For more information, see README file
********************************************************************************
* Tested with Arduino 1.8.5, ESP8266 Thing Dev circa March 2018
*
* Copyright (c) 2010, Exosite LLC
* All rights reserved.
* For License see LICENSE file
*******************************************************************************/
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Exosite.h>
#include <EEPROM.h>

/* Definitions */
#define TEMP_GND 15
#define TEMP_VCC 16
#define LED 5
#define SL_DRIVER_VERSION 1
#define productId "PUTPRODUCTIDHERE" // Must be updated to match the Product ID for each project

/* Constants */
// WiFi credentials
//error-update-ssid,password
const char* ssid     = "PUTWIFISSIDHERE";
const char* password = "PUTWIFIPASSPHRASEHERE";

// Communication URL for Exosite
const char* host = "PUTPRODUCTIDHERE.m2.exosite.io";
const int hostPort = 443;  // use SSL port

// SHA1 fingerprint of the Exosite certificate
const char* fingerprint = "51 8C 0D C5 A1 6C 9E C2 33 11 F9 34 8A 89 47 8A 3B 47 AE 8D";

// Number of Errors before we try a reprovision.
const unsigned char reprovisionAfter = 3;

/* Global Variables */
char macString[18];  // Used to store a formatted version of the MAC Address
byte macData[WL_MAC_ADDR_LENGTH];
String readString = "state&d5&msg";
String prevRead = "";
unsigned char errorCount = reprovisionAfter;  // Force Provision On First Loop

WiFiClientSecure client;
Exosite exosite(&client);


/*******************************************************************************
* Startup Entry Point
*******************************************************************************/
void setup() {
  Serial.begin(115200);
  delay(100);

  // Configure GPIO
  pinMode(TEMP_GND, INPUT); 
  pinMode(TEMP_VCC, INPUT); 
  pinMode(LED, OUTPUT);
  blinkLED(5);
    
  // Init EEPROM
  EEPROM.begin(40);
  
  // Configure Exosite
  exosite.begin();
  exosite.setDomain(productId".m2.exosite.io", hostPort);

  // Configure WiFi
  setup_wifi();

  // Attempt to obtain a cloud identity
  // First, we try to activate in case device identifier has CIK in platform or a new Product ID
  Serial.println("setup: Check Exosite Platform Provisioning");
  Serial.print("setup: Murano Product ID: ");
  Serial.print(productId);
  Serial.print(F(", Device Identifier (MAC Address): "));
  Serial.println(macString);
  if (exosite.provision(productId, productId, macString)) {
    Serial.println("setup: Provision Succeeded");
    EEPROM.commit();
    errorCount = 0;
  } else {
    Serial.println("setup: Provision Check Done");
  }
  
}


/*******************************************************************************
* Main Loop 
*******************************************************************************/
void loop() { 
  String writeString = "";
  String returnString = "";
  String tempString;
  int indexer = 0;
  int lastIndex = -1;
  int d5_val = 0;
  char d5_str[10];

  // Check if we should reprovision.
  if (errorCount >= reprovisionAfter) {
    Serial.println("---- Attempting Reprovision ----");    
    blinkLED(4);
    if (exosite.provision(productId, productId, macString)) {
      Serial.println("Reprovision: Provision Succeeded");
      EEPROM.commit();
      errorCount = 0;
    } else Serial.println("Reprovision: Provision Check Done");
  }
  
  String uptime_str = String(millis()/1000);
  
  writeString += "uptime="+ uptime_str;
  writeString += "&a0="+ String(readTemp());

  //Make Write and Read request to Exosite Platform
  Serial.println("---- Do Read and Write ----");
  if (exosite.writeRead(writeString, readString, returnString)) {
    errorCount = 0;
    if (prevRead != returnString) {
      prevRead = returnString;
      Serial.print("Returned: ");
      Serial.println(returnString);
      Serial.println("Parse out dataport alias values");
  
      // Read out our data ports
      for(;;){
        indexer = returnString.indexOf("=", lastIndex+1);
        if(indexer != 0){
          String alias = "";
          tempString = returnString.substring(lastIndex+1, indexer);
          lastIndex = returnString.indexOf("&", indexer+1);
          alias = tempString;
          if (lastIndex != -1){
            tempString = returnString.substring(indexer+1, lastIndex);
          } else {
            tempString = returnString.substring(indexer+1);
          }
          
          // if we hit the dataport "d5" then control I/O D5
          if (alias == "d5"){
            tempString.toCharArray(d5_str, 10);
            d5_val = atoi(d5_str);
            Serial.print("D5: ");
            Serial.println(d5_val);
            if (0 == d5_val) {
              digitalWrite(LED, 0);
              exosite.writeRead("d5=0", readString, returnString); // set the ds back to 0 to ack
            } else {
              digitalWrite(LED, 1);
              exosite.writeRead("d5=1", readString, returnString); // set the ds back to 0 to ack
            }
          } else if (alias == "msg"){
            Serial.print("Message: ");
            Serial.println(tempString);
            exosite.writeRead("msg=ack", readString, returnString); // set the ds to ack
          } else if (alias == "state"){
            Serial.print("State: ");
            Serial.println(tempString);
            exosite.writeRead("state=ack", readString, returnString); // set the ds to ack
          } else {
            Serial.println("Unknown Alias Dataport");
          }
     
          if(lastIndex == -1)
            break;     
          
        }else{
          Serial.println(F("No Index"));
          break;
        }
      }
    }
    
  } else {
    Serial.println("No Connection");
    errorCount++;
  }
  if (errorCount) {
    blinkLED(1);
    Serial.println("Connection errors, retrying in one second.");
    delay(1000);
  }  else {
    doubleBlinkLED(1);
    delay(10000); // Operate every 10 seconds  
  }
}


/*******************************************************************************
* Helper Functions Below Here
*******************************************************************************/

/*******************************************************************************
* Wifi Setup
*******************************************************************************/
void setup_wifi(void){
  // Connect to a WiFi network - HAR TODO: Use open network to acquire private network credentials
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(100);
  
  // Read MAC Address String in the format FF:FF:FF:FF:FF:FF
  WiFi.macAddress(macData);
  snprintf(macString, 18, "%02X%02X%02X%02X%02X%02X",
           macData[5], macData[4], macData[3], macData[2], macData[1], macData[0]);
  // Print Some Useful Info
  Serial.print(F("wifi: MAC Address: "));
  Serial.println(macString);
}


/*******************************************************************************
* Read Temperature
*******************************************************************************/
int readTemp() { 
  int temp_val;
  pinMode(TEMP_VCC, OUTPUT); 
  pinMode(TEMP_GND, OUTPUT); 
  digitalWrite(TEMP_VCC, 1);
  digitalWrite(TEMP_GND, 0);

  delay(100); // Let supplies settle
  Serial.print("Temp A0 = "); 
  temp_val = readADC();
  Serial.println(String(temp_val));
  
  pinMode(TEMP_VCC, INPUT); 
  pinMode(TEMP_GND, INPUT);

  return temp_val;
}

    
/*******************************************************************************
* Read ADC
*******************************************************************************/
int readADC() { 
    int avgTotal = 0;
    int avgCount = 0;

    for(avgCount = 0; avgCount < 10; avgCount++)
    {
      avgTotal += analogRead(A0);
    }
    return (avgTotal/avgCount);
}


    
/*******************************************************************************
* Blink LED
*******************************************************************************/
void blinkLED(unsigned char n) { 
  unsigned char i;
  
  for(i = 0; i < n; i++) {
    digitalWrite(LED, 1);  // turn LED off
    delay(200);
    digitalWrite(LED, 0);  // turn LED on
    delay(200);
    digitalWrite(LED, 1);  // turn LED off
  }
}


/*******************************************************************************
* Double Blink LED
*******************************************************************************/
void doubleBlinkLED(unsigned char n) { 
  unsigned char i;
  
  for(i = 0; i < n; i++) {
    digitalWrite(LED, 1);  // turn LED off
    delay(200);
    digitalWrite(LED, 0);  // turn LED on
    delay(100);
    digitalWrite(LED, 1);  // turn LED off
    delay(100);
    digitalWrite(LED, 0);  // turn LED on
    delay(100);
    digitalWrite(LED, 1);  // turn LED off
  }
}


