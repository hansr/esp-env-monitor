/*******************************************************************************
  esp-env-monitor
  Arduino program that uses the Sparkfun ESP8266 Thing Dev board
  to read and send temp values from dallas one wire sensor and analog
  values (light or other sensors) to Exosite using SSL
  For more information, see README file
********************************************************************************
  Tested with:
  * Arduino 1.8.5 & 1.8.9
  * ESP8266 Thing Dev circa March 2018
  * Exosite library versions 2.5.2 - 2.6.2
  * OneWire library version 2.3.4
  * ESP8266 Board library version 2.4.2 (NOTE: version 2.5.0 and later break SSL for this sketch)
  
  Copyright (c) 2018 2019, Exosite LLC
  All rights reserved.
  For License see LICENSE file
*******************************************************************************/
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Exosite.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* Definitions */
#define ONE_WIRE_BUS 2
#define ADC_GND_PIN 15
#define ADC_VCC_PIN 16
#define LED 5
#define SL_DRIVER_VERSION 1
#define NV_SIZE 128 // just using a part of the NV / EEPROM sim for our purposes

// This is the product ID for the Exosite IoT Connector.  Go online to <TODO-URL> to claim and configure
// how the device interacts with the rest of the world.
#define productId "PUTPRODUCTIDHERE" 

/* Constants */
// Optional default bomb-proof wifi network credentials for fallback (backup to the backup plan)
// Recommend not using this and just using the cloud credential method (setup SSID/Passphrase resources in the cloud and 
// bring an open network within range of the device allowing it to pulldown the intended credentials)
const char* hardcode_ssid = "PUTWIFISSIDHERE";
const char* hardcode_password = "PUTWIFIPASSPHRASEHERE";
        
// Communication URL for Exosite
const char* host = productId".m2.exosite.io";
const int hostPort = 443;  // use SSL port

// SHA1 fingerprint of the Exosite certificate
const char* fingerprint = "51 8C 0D C5 A1 6C 9E C2 33 11 F9 34 8A 89 47 8A 3B 47 AE AA 8D";

const unsigned char nv_ssid_ptr = 41;
const unsigned char nv_password_ptr = 71;

// Number of communication errors before we try a reprovision.
const unsigned char reprovisionAfter = 3;

/* Global Variables */
char macString[18];  // Used to store a formatted version of the MAC Address
byte macData[WL_MAC_ADDR_LENGTH];
String readString = "state&d5&msg";
String prevRead = "";
unsigned char errorCount = 0;  

WiFiClientSecure client;
Exosite exosite(&client);
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

/*******************************************************************************
  Startup Entry Point
*******************************************************************************/
void setup() {
  Serial.begin(115200);
  delay(100);

  // Configure GPIO
  pinMode(ADC_GND_PIN, INPUT);
  pinMode(ADC_VCC_PIN, INPUT);
  pinMode(LED, OUTPUT);
  blinkLED(5);

  // Init EEPROM
  // we can store up to 512 bytes in the EEPROM - first 40 bytes are the NVCIK (identity) of this device
  // we allocate 128 so we can store the NVCIK (40 bytes), a SSID (up to 30 bytes) and a Passphrase (up to 40 bytes)
  EEPROM.begin(NV_SIZE);
  
  // startup dallas and onewire libraries
  sensors.begin();

  // Configure Exosite
  exosite.begin();
  exosite.setDomain(host, hostPort);

  // Configure wifi and net connection and identity
  connect_iot();  // this will spin forever if it doesn't find a valid network and verify connectivity to Exosite servers

  // send config details to ExoSense
  if (!writeDeviceConfigExoSense()) // failure here is likely due to an incorrect cloud resource setup - must have a "config_io" string resource available 
    Serial.println("Was not able to setup data config for Exosite's ExoSense application, power cycle to retry or manually set via the cloud resource");

}


/*******************************************************************************
  Main Loop
*******************************************************************************/
void loop() {
  String writeString = "";
  String returnString = "";
  String tempString;
  int indexer = 0;
  int lastIndex = -1;
  int d5_val = 0;
  char d5_str[10];
  int a0_val = 0;
  double temp_val = 0.0;

  // Check if we should attempt reconnect (could be for a variety of reasons)
  if (errorCount >= reprovisionAfter) {
    Serial.println("---- Too many connection errors, attempting reprovision ----");
    blinkLED(4);
    connect_iot();
  }

  Serial.println("\r\n---- Collecting board sensor & system data ----");
  // compute number of seconds this board has been running
  String uptime_str = String(millis() / 1000);
  Serial.print("Uptime = ");
  Serial.println(uptime_str);

  // read the connected analog sensor
  a0_val = readAnalogSensor();
  Serial.print("A0 = ");
  Serial.println(String(a0_val));

  // read the connected digital temp sensor
  sensors.requestTemperatures();
  temp_val = sensors.getTempCByIndex(0);
  Serial.print("Temperature = ");
  Serial.println(String(temp_val));

  writeString += "uptime=" + uptime_str;
  writeString += "&a0=" + String(a0_val);
  writeString += "&data_in={ \"001\":" + String(temp_val) + "}";

  //Make Write and Read request to Exosite Platform
  Serial.println("---- Do Read and Write ----");
  if (exosite.writeRead(writeString, readString, returnString)) {
    errorCount = 0;
    if (prevRead != returnString) {
      prevRead = returnString;
      Serial.print("Returned from cloud: ");
      Serial.println(returnString);
      Serial.println("Parsing dataport alias values...");

      // Read out our data ports
      for (;;) {
        indexer = returnString.indexOf("=", lastIndex + 1);
        if (indexer != 0) {
          String alias = "";
          tempString = returnString.substring(lastIndex + 1, indexer);
          lastIndex = returnString.indexOf("&", indexer + 1);
          alias = tempString;
          if (lastIndex != -1) {
            tempString = returnString.substring(indexer + 1, lastIndex);
          } else {
            tempString = returnString.substring(indexer + 1);
          }

          // if we hit the dataport "d5" then control I/O D5
          if (alias == "d5") {
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
          } else if (alias == "msg") {
            Serial.print("Message: ");
            Serial.println(tempString);
            exosite.writeRead("msg=ack", readString, returnString); // set the ds to ack
          } else if (alias == "state") {
            Serial.print("State: ");
            Serial.println(tempString);
            exosite.writeRead("state=ack", readString, returnString); // set the ds to ack
          } else {
            Serial.println("Unknown alias");
          }

          if (lastIndex == -1)
            break;

        } else {
          Serial.println(F("No index"));
          break;
        }
      }
      Serial.println("Done reading dataports");
    } else Serial.println("Same data read as last communication, no action taken.");

  } else {
    Serial.println("No Connection");
    errorCount++;
  }
  if (errorCount) {
    blinkLED(1);
    Serial.println("Connection errors, retrying in one second.");
    delay(1000);
  }  else {
    Serial.println("Read/write cycle complete");
    Serial.println("==================");
    delay(10000); // Operate every 10 seconds
    doubleBlinkLED(1); // Blink the LED when starting up again
  }
}


/*******************************************************************************
  Helper Functions Below Here
*******************************************************************************/

/*******************************************************************************
  IoT etc... Connection Setup
*******************************************************************************/
bool connect_iot(void) {
  bool retry = false;
  unsigned char attempts = 0;
  char ssid[40];
  char password[40];
  char attempted_ssids[15][40]; // burning 600 bytes of ram!!
  const char * nullpass = "";
  unsigned char ssids_saved = 0;
  bool tried = false;
  String returnString = "";

  // the following sequence uses method from USPTO 61/950,079
  do {
    if (0 == ssids_saved) {  // this will happen upon first entry and if we exhaust open networks while looking for a valid cloud-specified network
      // if there are valid strings in the NV EEPROM, pull them in and try them right off the batt
      getNVString(nv_ssid_ptr, ssid, sizeof(ssid));
      getNVString(nv_password_ptr, password, sizeof(password));
    
      // if there was not a valid SSID in NV, try to use the hard-coded credentials, if any
      if (0 == strlen(ssid)) {
        Serial.println("No NV wifi network credentials found, we'll try to initialize with hardcoded credentials.");
        strcpy(ssid, hardcode_ssid);
        strcpy(password, hardcode_password);
      }
    }
    if (connect_wifi(ssid, password) && verifyExositeLink()) {
      retry = false; // we are good to go no matter if we find a good cloud-specified wifi network, or just keep with this insecure one
      Serial.println("Exosite servers available, hello Internet!");
      // This is a good first step!  We are connected.  Now, we see if there is a
      // reall SSID/Passphrase waiting for us that we should be connecting through
      // If there are not secure network credentials, or if the secure network doesn't work
      // we continue on with the open network we have

      ssids_saved = 0;
      strcpy(attempted_ssids[ssids_saved], ssid); // save off our successful open network ID

      // First step is provision / get an identity
      provision_identity();

      // Let's try to read SSID and Passphrase
      if ((exosite.writeRead("", "SSID", returnString)) && (2 < returnString.length()) && (5 + sizeof(ssid) > returnString.length())) {
        returnString = returnString.substring(5); // we ignore the "SSID=" part
        strcpy(ssid, (char *)returnString.c_str());
        Serial.print("Found a cloud-specified WiFi SSID: ");
        Serial.println(ssid);
        // using len of 12 here because cloud resource can't be set to null - assuming no-one will use a 1 char passphrase - sorry in advance...
        if ((exosite.writeRead("", "Passphrase", returnString)) && (12 < returnString.length()) && (11 + sizeof(password) > returnString.length())) {                  
          returnString = returnString.substring(11); // we ignore the "Passphrase=" part
          strcpy(password, (char *)returnString.c_str());
          Serial.print("Found a cloud-specified WiFi passphrase (hidden) of length: ");
          Serial.println(strlen(password));
        } else {
          Serial.println("No passphrase specified, assuming insecure network is specified.");
          strcpy(password, nullpass);
        }
        if (connect_wifi(ssid, password) && verifyExositeLink()) {
          Serial.println("Cloud-specified network looks legit!  Writing to NV and continuing...");
        } else {
          Serial.println("\r\nCloud-specified network is a no-go, reverting back to our tried & true network.");
          strcpy(ssid, attempted_ssids[ssids_saved]);
          strcpy(password, nullpass);
          if (!(connect_wifi(ssid, password) && verifyExositeLink())) {
            Serial.println("Shoot, even that is wedged now, retrying the whole kaboodle");
            retry = true;
          } else Serial.println("Verified we are still good to go, continuing on with life!");
        }


      } else {
        Serial.println("No valid cloud-specified network, continuing on with good network we found");
        // Could be a provisioning problem still @ this stage, but at least we have a good network
      }
    } else retry = true;

    if (true == retry) {
      Serial.println("\r\nSSID didn't work, trying a new one");

      // Set WiFi to station mode and disconnect from an AP if it was previously connected
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      delay(100);
      int n = WiFi.scanNetworks();
      int i = 0;
      Serial.println("Available network scan done");
      if (n == 0) {
        Serial.println("no networks found");
      } else {
        Serial.print(n);
        Serial.println(" networks found");
        for (i = 0; i < n; ++i) {
          if (ENC_TYPE_NONE == WiFi.encryptionType(i)) {
            // Print SSID and RSSI for each open network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
            if(strlen(WiFi.SSID(i).c_str()) < sizeof(ssid)) { // we just full-on ignore any networks with massive SSID size
              strcpy(ssid, (char *)WiFi.SSID(i).c_str());
              tried = false;
              for (int z = 0; z < ssids_saved; z++) { // iterate through ssids we've already attempted so we don't retry a bogus one
                if (0 == strcmp(ssid, attempted_ssids[z])) tried = true;
              }
  
              if (false == tried) {
                strcpy(password, nullpass);
                strcpy(attempted_ssids[ssids_saved], ssid);
                ssids_saved++;
  
                Serial.print("Trying network: ");
                Serial.println(ssid);
                Serial.print("With hidden passphrase of length: ");
                Serial.println(strlen(password));
                break;
              } else {
                Serial.print("Already tried this open network to no avail, skipping ");
                Serial.println(ssid);
              }
            } else Serial.println("Skipping this network, SSID string size too large");
          }
          delay(10);
        }
        if (i == n) { // we have exhausted network options
          ssids_saved = 0; // best bet is just to retry them all...
          Serial.println("Exhausted open networks in range, retrying full list.  Make sure there is an open network in range, make sure the Product ID is set, and make sure a 'Passphrase' and 'SSID' resource with valid credentials are available in the cloud.");
        }
      }

      // Wait a bit before scanning again
      delay(50);

    }
  } while (true == retry);

  setNVString(nv_ssid_ptr, ssid, strlen(ssid));
  setNVString(nv_password_ptr, password, strlen(password));
  EEPROM.commit();   // commit does not release RAM, just programs the flash.  eeprom.end will release RAM and requires eeprom.begin to be called again

  return true;
}


/*******************************************************************************
  getNVString
*******************************************************************************/
void getNVString(int address, char * outbuff, unsigned char buff_len) {
  int i;

  for (i = 0; i < buff_len; i++) {
    *(outbuff + i) = EEPROM.read(address + i);
    if (0 == *(outbuff + i)) break;
  }

  if (i >= (buff_len - 1)) {
    Serial.println("NV read through end of buffer without finding a string termination, setting to null.");
    *outbuff = 0;  // null terminate on first character if we read the whole NV without a value value
  }

}

/*==============================================================================
  setNVString

  Write a string to EEPROM
  =============================================================================*/
void setNVString(int address, char * inbuff, unsigned char buff_len) {
  int i;

  for (i = 0; i < buff_len; i++) {
    EEPROM.write(address + i, *(inbuff + i));
  }
  //null terminate
  EEPROM.write(address + i, 0);
}


/*==============================================================================
  Wipe NV

  Totally erase the EEPROM / NV space.  Just a helper function to reset board state
  for development / debug, not used in real operation.
=============================================================================*/
void wipeNV(void) {

  Serial.println("Erasing the whole NV memory!!!");
 
  for (int i = 0; i < NV_SIZE; i++) {
    EEPROM.write(i, 0);
  }

  EEPROM.end(); // commits changes and shuts down EEPROM until begin is called again
  Serial.println("Spinning forever...");
  while(1);
}



/*******************************************************************************
  Wifi Setup
*******************************************************************************/
bool connect_wifi(char * ssid, char * password) {
  unsigned char count = 0;
  // Connect to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (count++ > 20) return false;
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
  return true;
}


/*******************************************************************************
  Provision Identity
  - Try to get communication credentials from the Exosite servers 
*******************************************************************************/
void provision_identity(void) {
  // Attempt to obtain a cloud identity
  // First, we try to activate in case device identifier has CIK in platform or a new Product ID
  Serial.println("setup: Check Exosite Platform Provisioning");
  Serial.print("setup: Murano Product ID: ");
  Serial.print(productId);
  Serial.print(F(", Device Identifier (MAC Address): "));
  Serial.println(macString);
  if (exosite.provision(productId, productId, macString)) {
    Serial.println("setup: Provision Succeeded");
    EEPROM.commit();   // commit does not release RAM, just programs the flash.  eeprom.end will release RAM and requires eeprom.begin to be called again
    errorCount = 0;
  } else {
    // the provision check can return false with a 409 - which is a normal state if this device has already obtained an identity in flash
    Serial.println("setup: Provision Check Done");
  }
}


/*******************************************************************************
  Verify Exosite Connectivity
  - verify we have a valid IP connection to the Exosite servers
*******************************************************************************/
bool verifyExositeLink() {
  unsigned long rtn = 0;
  Serial.println("Verifying link to Exosite servers");
  rtn = exosite.timestamp();
  Serial.print("Timestamp is: ");
  Serial.println(rtn);
  if (rtn > 1565490216) return true;  // 1565490216 is 8/10/2019 @ around 9.23pm
  else return false;
}

/*******************************************************************************
  Write Device Config ExoSense
  - send config_io, so ExoSense knows we will be sending temperature in celsius
  - ExoSense uses config_io to display information with context
*******************************************************************************/
bool writeDeviceConfigExoSense() {
  int rtn = 0;
  int num_tries = 0;

  String configString = "{\"last_edited\": \"2018-04-16\", \"meta\": \"temperature starter kit\", \"channels\": { \"001\": { \"display_name\": \"Temperature\", \"description\": \"temperature\", \"properties\": { \"data_type\": \"TEMPERATURE\", \"data_unit\": \"DEG_CELSIUS\" } } } }";
  String writeString = "config_io=" + configString;
  String returnString = "";

  do {
    Serial.println("Write Config Attempt: " + String(num_tries));
    rtn = exosite.writeRead(writeString, readString, returnString);

    if (rtn) {
      Serial.println("Write Config Successful");
    }
    if (num_tries++ > 10) {
      Serial.println("Not able to write config, returning failure...");
      return false;
    }
  } while (rtn == 0);
  return true;
}


/*******************************************************************************
  Read Temperature
*******************************************************************************/
int readAnalogSensor() {
  int analog_val;
  pinMode(ADC_VCC_PIN, OUTPUT);
  pinMode(ADC_GND_PIN, OUTPUT);
  digitalWrite(ADC_VCC_PIN, 1);
  digitalWrite(ADC_GND_PIN, 0);

  delay(100); // Let supplies settle

  analog_val = readADC();

  pinMode(ADC_VCC_PIN, INPUT);
  pinMode(ADC_GND_PIN, INPUT);

  return analog_val;
}


/*******************************************************************************
  Read ADC
*******************************************************************************/
int readADC() {
  int avgTotal = 0;
  int avgCount = 0;

  for (avgCount = 0; avgCount < 10; avgCount++)
  {
    avgTotal += analogRead(A0);
  }
  return (avgTotal / avgCount);
}



/*******************************************************************************
  Blink LED
*******************************************************************************/
void blinkLED(unsigned char n) {
  unsigned char i;
  bool initial_state = digitalRead(LED);
  
  for (i = 0; i < n; i++) {
    digitalWrite(LED, 1);  // turn LED off
    delay(200);
    digitalWrite(LED, 0);  // turn LED on
    delay(200);
    digitalWrite(LED, 1);  // turn LED off
  }

  digitalWrite(LED, initial_state);  // turn LED back to where it was
}


/*******************************************************************************
  Double Blink LED
*******************************************************************************/
void doubleBlinkLED(unsigned char n) {
  unsigned char i;
  bool initial_state = digitalRead(LED);
  
  for (i = 0; i < n; i++) {
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
  digitalWrite(LED, initial_state);  // turn LED back to where it was
}
