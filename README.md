# esp-env-monitor

ESP8266-based temp and light monitor used in Exosite's Minneapolis office.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Overview
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This is a project can be programmed on any Sparkfun ESP8266 Thing Dev 
board (https://www.sparkfun.com/products/13804) to enable it for cloud 
communications. It supports a Dallas One Wire temp sensor hooked up as: pin 1 -
GND; pin 2 - GPIO 2; pin 3 - 3.3.v.

Once this sketch is programmed onto the ESP8266, it will search for open wifi
networks in range.  Once it has found an open network that will allow it to
connect out to the Internet, it will provision itself and pull down proper WiFi
credentials as configured in the cloud, and then will connect with those WiFi
credentials.  If the credentials don't work, it will fall back to the open
network it found.  If that network goes away, it will fall back to the last good
network in non-volatile memory.

Once the system has a solid net connection,  it will just spin happily in a 10
second loop sending Uptime, Analog reading, and Temperature reading (from the
Dallas OneWire chip) to the cloud, and will read a few resources from the cloud - 
most notably: 1) the value of "d5" - a '0' in that resource will turn the LED on, 
and a '1' will turn it off 2) the value of "cmd" - a value of "reprovision" will
force an identity and WiFi credential reprovision sequence.

License is BSD, Copyright 2018 2019, Exosite LLC (see LICENSE file)

Requires: Arduino IDE (1.8.5, 1.8.9 verified), Exosite Arduino library (2.5.2 -
2.6.2 verified), Sparkfun ESP8266 Thing Dev Board Support (2.4.2 verified, 2.5.0
and later break SSL), and Dallas OneWire library (2.3.4 verified)

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Quick Start
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* Install Arduino
  * https://www.arduino.cc/en/Main/Software

* Install libraries
  * The Exosite Arduino library and the Dallas OneWire library can be installed 
    from within the Arduino tool at Sketch->Include Library->Manage Libraries...
  * Support for the Sparkfun ESP8266 Thing Dev board can be installed from 
    within the Arduino tool at Tools->Board:"NN"->Boards Manager...

* Upload the firmware
  * Plug your Sparkfun Thing Dev board into a USB cable and program from the
    Arduino tool
  * To debug, pull up the Arduino Serial Monitor and read the messages to see
    where you are getting stuck.  Could be WiFi settings, could be your router 
    won't let the traffic through.  

* Claim your device
  * Go to the Product as a Service URL (https://j1b0l7vyy5ajk0000.apps.exosite.io/)
    for this system and claim your device.  You can establish an account for this 
    product there and setup web service targets that can interact with your 
    devices.  ExoSense, Exosite's remote condition monitoring tool (fancy 
    dashboarding system), is supported by default - you can claim the device 
    actually from within your ExoSense installation

* Setup preferred WiFi credentials for the device
  * Set the "SSID" and "Passphrase" resource for the desired WiFi connection
    via the cloud resources - the device will pull these down

* Turn on an open WiFi network within range
  * This will allow the device to connect to the Internet and get secure (or
    preferred) credentials.  Once it has received them, it will hum happily
    along doing its thing.  If the device ever has connection errors, it will
    go through a re-connect routine.  If the re-connect routine doesn't work, it
    will always end up back to the last known good network and keep trying either
    that or other open networks in range hoping to find a way back home again.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Do It Yourself

If you want to get into the bits and bytes, iterate on this project, or publish
your own Product as a Service that others can use, here are some tips
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* Create an Exosite account
  * Go to https://exosite.io and sign up!

* Create & Configure a Murano Product
  * From within Murano, go to your Solutions page and Create Solution -> choose 
    Product Type and "IoT Connector with PDaaS Technology"
  * Go to your Product's Settings page and ensure "HTTP API" under Protocol, 
    "Opaque" under Identity, and check "Enable Provisioning" and "Allow devices
    to register their own identity" under "Provisioning"

* Copy your Product ID from Murano and update your Arduino code
  * Click on the "ID" icon in the upper left to copy the Product ID to your 
    clipboard and paste it over the value that says "PUTPRODUCTIDHERE"

* Configure your backup to the backup new-board no connection WiFi parmaters
  * Set the WiFi SSID by replacing "PUTWIFISSIDHERE" with the SSID, and set the
    WiFi Passphrase by replacing "PUTWIFIPASSPHRASEHERE" with the passphrase
  * Note that this is not required - the board will find its own network from
    your cloud settings if it is given an open network to figure things out
    
* Turn on an open WiFi network within range or ensure the WiFi network you
  hard coded is in range
  * This will allow the device to connect to the Internet and get secure (or
    preferred) credentials.  Once it has received them, it will hum happily
    along doing its thing.  If the device ever has connection errors, it will
    go through a re-connect routine.  If the re-connect routine doesn't work, it
    will always end up back to the last known good network and keep trying either
    that or other open networks in range hoping to find a way back home again.

* Power up and give it a shot
  * If you have all the settings correct, the board will auto provision and
    will show up in your Device list and should start flowing data to
    your Murano account.  If you pull up your Product's "Devices" page, you'll
    see a list of devices you have provisioned under the Product - since you
    just created the Product, you'll either see none, or your new device listed
    there.  If you don't see your device yet, you can wait around to see if it
    will appear, or you can debug.
  * You can also open the "Logs" page under your Product area in Murano.  The 
    Logs will show all valid communications with any device in your Product.
  * As soon as the device provisions, you can click the device and then set
    a different WiFi network credentials up in the SSID and Passphrase resources
    for the device

* Configure the Product as a Service application
  * Your product in Murano has its own web service API, webservice I/O mechanism,
    and website that will allow other people to claim & setup devices that they
    have purchased that are programmed with your exact firmware.
  * The URL of your application will be shown in the Product solution in Murano
    under the "www" icon in the management view
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
More Information
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* Exosite Documentation -> https://docs.exosite.com

* More about ExoSense -> https://exosite.com/iot-solutions/condition-monitoring/

* For support, email: support@exosite.com (reference the github project, etc)
  * Web: https://support.exosite.com (forums, knowledge base, ticket system)
  
* FAQ
  * Q: How do I add more sensors to the system?
    * A: Since the ESP8266 only has a single ADC input, any others sensors added 
      to the system need to have a one-wire or I2C interface.  Check out topics 
      on Arduino I2C and one-wire sensors, like: 
      https://github.com/TaaraLabs/OneWireHumidityLight-Demo
    
  * Q: How do I see the information in the ExoSense application?
    * A: In ExoSense, you can both claim your device and begin interacting with it 
      by going to "Unclaimed Devices" and either selecting your device or 
      searching for its identity.  Identity for this system is its MAC address.
