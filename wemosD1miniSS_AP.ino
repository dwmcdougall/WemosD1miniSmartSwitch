/*  ============================================================================================================

    Sample code to create a setup web portal using a soft access point on an ESP8266.

    For more info, please watch my video at https://youtu.be/1VW-z4ibMXI
    MrDIY.ca
    & 
    https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch/blob/master/sinric.ino

    https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/

    https://github.com/datacute/DoubleResetDetector/tree/master

    https://www.youtube.com/watch?v=l9Gl1yKvMNg
    
  ============================================================================================================== */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <functional>
#include <DNSServer.h>
#include <EEPROM.h>
//#include <DoubleResetDetector.h>

ESP8266WebServer    server(80);   // Port 80? 

//#define DRD_TIMEOUT 5            // Number of seconds after reset during which a subseqent reset will be considered a double reset.
//#define DRD_ADDRESS 0             // RTC Memory Address for the DoubleResetDetector to use
//
//DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

// ------------------------------------------ PROTOTYPES ------------------------------------------
void prepareIds();
void respondToSearch();
boolean connectWifi();
boolean connectUDP();
void startHttpServer();
void turnOnRelay();
void turnOffRelay();
void sendRelayState();
void handleNotFound();
void reboot();


// ------------------------------------------ Variables ------------------------------------------
struct settings 
{
  char ssid[30];
  char password[30];
  char friendlyName[30];
} user_wifi = {};

String defaultFriendlyName = "Watch Winder";
String friendlyNameString;
const int relayPin = 3;  // D1 pin. More info: https://github.com/esp8266/Arduino/blob/master/variants/d1_mini/pins_arduino.h#L49-L61

char AccessPointName[15] = "WemosD1mini_AP";

WiFiUDP UDP;
IPAddress ipMulti(239, 255, 255, 250);
//ESP8266WebServer HTTP(80);                // Changed to "server"
boolean udpConnected = false;
unsigned int portMulti = 1900;              // local port to listen on
unsigned int localPort = 1900;              // local port to listen on
boolean wifiConnected = false;
boolean relayState = false;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];  // buffer to hold incoming packet,
String serial;
String persistent_uuid;
boolean cannotConnectToWifi = false;


const byte DNS_PORT = 53;
DNSServer dnsServer;
IPAddress apIP(172, 217, 28, 1);

// ------------------------------------------ ARDUINO SETUP ------------------------------------------
void setup() 
{
  // Setup Serial Monitor
  Serial.begin(115200);

  // Setup Relay
  pinMode(relayPin, OUTPUT);

  // Grab ESP Unique ID and store in the string "serial" 
  prepareIds();

  // Setup EEPROM for SSID/Password/friendlyName Storage & Retrieval
  
  EEPROM.begin( sizeof(struct settings) );                      // Allocate EEPROM the size of our user settings
  EEPROM.get( 0, user_wifi );                                   // Read from EEPROM address 0 the settings and store in user_wifi struct

  Serial.println(""); 
  Serial.println("***** REBOOTED ***** ");                     

  // Double Reset in 10 seconds erases the WiFi configuration & Friendly name from EEPROM and resets
//  if (drd.detectDoubleReset()) 
//  {
//    Serial.println("Double Reset Detected. Clearing WiFi Settings");
//    drd.stop();    // Don't consider the next reset as a double reset
//
//    Serial.print("EEPROM.length: " );
//    Serial.println( EEPROM.length() , DEC );
//
//    Serial.println(""); 
//    for (int num = 0; num < 30; num++)
//    {
//      Serial.print(user_wifi.friendlyName[num], HEX); 
//    } 
//    Serial.println("");
//
//    
//    
////    for (int i = 0 ; i < EEPROM.length() ; i++) 
////    {
////      EEPROM.write(i, 1);
////    }
////    EEPROM.put(0, user_wifi);
//
//      for(int i = 0; i<30; i++)
//      {
//          user_wifi.ssid[i] = 0xFF;
//          user_wifi.password[i] = 0xFF;
//          user_wifi.friendlyName[i] = 0xFF;
//      }
//
//      EEPROM.put(0, user_wifi);
//      EEPROM.commit();
//      EEPROM.get( 0, user_wifi );                                   // Read from EEPROM address 0 the settings and store in user_wifi struct
//
//      Serial.println(""); 
//      for (int num = 0; num < 30; num++)
//      {
//        Serial.print(user_wifi.friendlyName[num], HEX); 
//      } 
//      Serial.println("");
//
//      EEPROM.end();
//
//      Serial.print("EEPROM cleared" );
//      reboot();
//  } 
//  else 
//  {
//    Serial.println("No Double Reset Detected");
//    //digitalWrite(LED_BUILTIN, HIGH);
//  }

  Serial.print("EEPROM Friendly Name: ");                     // This is the Memory Saved Friendly name
  Serial.println(String(user_wifi.friendlyName));
  Serial.println("");  

  
  //if((user_wifi.ssid[0] != 0xFF) || (user_wifi.ssid[0] != 0x01))
  if( (user_wifi.friendlyName[0] == 0xFF) )
  {
    // The EEPROM hasn't been written to with a friendly name so use the default of "Police Box"
    // Serial.println("*** USE DEFAULT FRIENDLY NAME ***");
    friendlyNameString = defaultFriendlyName; 
  }
  else
  {
    // Use the friendly device name stored in eeprom
    // Serial.println("*** USE EEPROM FRIENDLY NAME ***");
    friendlyNameString = String(user_wifi.friendlyName);
  }

  Serial.print("Using Friendly Name: ");                     // This is the Memory Saved Friendly name
  Serial.println(friendlyNameString);
  Serial.println(""); 

  Serial.print("AccessPointName: ");
  Serial.println(String(AccessPointName));
  Serial.println(""); 

//  for(int num = 0; num < 10; num++)
//  {
//    Serial.print(".");
//    delay(1000); 
//  }

  Serial.print("user_wifi.ssid: ");
  Serial.println(user_wifi.ssid[0], HEX); 
  // Initialise wifi connection
  if((user_wifi.ssid[0] != 0xFF) && (user_wifi.ssid[0] != 0x01))
  {
    // There is a user assigned SSID so try to connect
    wifiConnected = connectWifi();
  }
  

  // only proceed with smart switch code if wifi connection successful, otherwise become Access Point
  if( wifiConnected )
  {
    // There is a user programmed SSID
   // drd.stop();    // Don't consider the next reset as a double reset
    dnsServer.stop();
    
    Serial.println(" ********* Ask Alexa to discover devices ********* ");
    Serial.println("");
    udpConnected = connectUDP();
    
    if (udpConnected)
    {
      // initialise pins if needed 
      startHttpServer();
    }
  }
  else  // No Stored WiFi, setup as AP
  {
    Serial.println("Access Point WemosD1mini_AP");
    Serial.println("Connect on WiFi to configure");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255,0));
    WiFi.softAP(AccessPointName);//, AccessPointPWD);      // Failed to connect, setup Access Point

    // if DNSServer is started with "*" for domain name, it will reply with
    // provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", apIP);

     server.on("/",  handlePortal);             // Call the 'handlePortal' function when a client requests URI "/"
//    server.on([]() {handlePortal()} );
//    server.on("/",  handlePortal);             // Call the 'handlePortal' function when a client requests URI "/"
//    server.on("/generate_204", handlePortal);  // Android captive portal. Maybe not needed. Might be handled by notFound handler.
//    server.on("/fwlink", handlePortal);        // Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP server started");
    
//    server.onNotFound(handleNotFound);      // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"
//    server.begin();
  }
}

// ------------------------------------------ LOOP ------------------------------------------
void loop() 
{
  server.handleClient();
  delay(1);
  
  // if there's data available, read a packet
  // check if the WiFi and UDP connections were successful
  if(wifiConnected)
  {
    if(udpConnected)
    {    
      // if there’s data available, read a packet
      int packetSize = UDP.parsePacket();
      
      if(packetSize) 
      {
        //Serial.println("");
        //Serial.print("Received packet of size ");
        //Serial.println(packetSize);
        //Serial.print("From ");
        IPAddress remote = UDP.remoteIP();
        
        for (int i =0; i < 4; i++) 
        {
          Serial.print(remote[i], DEC);
          if (i < 3) 
          {
            Serial.print(".");
          }
        }
        
        Serial.print(", port ");
        Serial.println(UDP.remotePort());
        
        int len = UDP.read(packetBuffer, 255);
        
        if (len > 0) 
        {
            packetBuffer[len] = 0;
        }

        String request = packetBuffer;
        //Serial.println("Request:");
        //Serial.println(request);
        
        // Issue https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch/issues/24 fix
        if(request.indexOf("M-SEARCH") >= 0) 
        {
            // Issue https://github.com/kakopappa/arduino-esp8266-alexa-multiple-wemo-switch/issues/22 fix
            //if(request.indexOf("urn:Belkin:device:**") > 0) {
             if((request.indexOf("urn:Belkin:device:**") > 0) || (request.indexOf("ssdp:all") > 0) || (request.indexOf("upnp:rootdevice") > 0)) 
             {
                Serial.println("Responding to search request ...");
                respondToSearch();
             }
        }
      }
        
      delay(10);
    }
  } 
  else 
  {
    dnsServer.processNextRequest();                    // Failed to connecto to stored Wifi, this is needed for Access Point only
    //drd.loop();                                        // Call the double reset detector so it can recognize when the timeout expires
  }
}

// ------------------------------------------ FUNCTIONS ------------------------------------------

void prepareIds() 
{
  uint32_t chipId = ESP.getChipId();
  char uuid[64];
  sprintf_P(uuid, PSTR("38323636-4558-4dda-9188-cda0e6%02x%02x%02x"),
        (uint16_t) ((chipId >> 16) & 0xff),
        (uint16_t) ((chipId >>  8) & 0xff),
        (uint16_t)   chipId        & 0xff);

  serial = String(uuid);
  persistent_uuid = "Socket-1_0-" + serial;
}

// ------------------------------------------ Connect Attempt FUNCTION ------------------------------------------
// connect to wifi – returns true if successful or false if not
boolean connectWifi()
{
  boolean state = true;
  int i = 0;
  
  // Set WiFi to station mode and disconnect from an AP if it was previously connected 
  WiFi.mode(WIFI_STA);
  WiFi.begin(user_wifi.ssid, user_wifi.password);            // Connect to the stored network
  Serial.println("");
  Serial.println("Connecting to WiFi");

  byte tries = 0;
  while (WiFi.status() != WL_CONNECTED) 
  {
    Serial.print('.');
    delay(500);
    if (tries++ > 30)                                        // Attempt to connect 30 times
    {
        //drd.loop();                                          // Call the double reset detector so it can recognize when the timeout expires
        tries = 0;
        Serial.println("");
        Serial.print("Connecting to WiFi: ");
        Serial.println(String(user_wifi.ssid));
//      state = false;
//      break;
    }
  }

  if (state)
  {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(user_wifi.ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  return state;
}


// ------------------------------------------ ACCESS POINT PORTAL ------------------------------------------
void handlePortal() 
{

  if (server.method() == HTTP_POST) 
  {

    strncpy(user_wifi.ssid,         server.arg("ssid").c_str(),             sizeof(user_wifi.ssid) );
    strncpy(user_wifi.password,     server.arg("password").c_str(),         sizeof(user_wifi.password) );
    strncpy(user_wifi.friendlyName, server.arg("friendlyDevName").c_str(),  sizeof(user_wifi.friendlyName) );
    
    user_wifi.ssid[server.arg("ssid").length()] = user_wifi.password[server.arg("password").length()] = user_wifi.friendlyName[server.arg("friendlyDevName").length()] = '\0';
    //user_wifi.ssid[server.arg("ssid").length()] = user_wifi.password[server.arg("password").length()] = '\0';

    friendlyNameString = String(user_wifi.friendlyName);
    
    EEPROM.put(0, user_wifi);
    EEPROM.commit();
    EEPROM.end();

    server.send(200,   "text/html",  "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,p{text-align: center}</style> </head> <body><main class='form-signin'> <h1>Wifi Setup</h1> <br/> <p>Your settings have been saved successfully!<br />Rebooting in 3 seconds... You may close this window.</p></main></body></html>" );

    // reset the Arduino
    
    reboot();
  } 
  else 
  {
    // replay to all requests with same HTML
      //server.send(200, "text/html", responseHTML);

      // SSID, and Password
      //  server.send(200,   "text/html", "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title> <style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{cursor: pointer;border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1{text-align: center}</style> </head> <body><main class='form-signin'> <form action='/' method='post'> <h1 class=''>Wifi Setup</h1><br/><div class='form-floating'><label>SSID</label><input type='text' class='form-control' name='ssid'> </div><div class='form-floating'><br/><label>Password</label><input type='password' class='form-control' name='password'></div><br/><br/><button type='submit'>Save</button><p style='text-align: right'><a href='https://www.instructables.com/Automated-Talking-Skull/' style='color: #32C5FF'>talkingSkull</a></p></form></main> </body></html>" );

      // SSID, Password, and Friendly Device Name
      server.send(200,   "text/html", "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title> <style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{cursor: pointer;border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1{text-align: center}</style> </head> <body><main class='form-signin'> <form action='/' method='post'> <h1 class=''>Wifi Setup</h1><br/><div class='form-floating'><label>SSID</label><input type='text' class='form-control' name='ssid'> </div><div class='form-floating'><br/><label>Password</label><input type='password' class='form-control' name='password'></div><div class='form-floating'><br/><label>Device Name</label><input type='text' class='form-control' name='friendlyDevName'></div><br/><br/><button type='submit'>Save</button><p style='text-align: right'><a href='https://www.instructables.com/Automated-Talking-Skull/' style='color: #32C5FF'>talkingSkull</a></p></form></main> </body></html>" );
  }
}

void handleNotFound()
{
    // replay to all requests with same HTML
    server.onNotFound([]() 
    {
      handlePortal();
      //server.send(200, "text/html", responseHTML);
    });


//  server.sendHeader("Location", "/",true); //Redirect to our html web page 
//  server.send(302, "text/plane",""); 
//  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}


// ------------------------------------------ ACCESS POINT PORTAL ------------------------------------------
void respondToSearch() 
{
    Serial.println("");
    Serial.print("Sending response to ");
    Serial.println(UDP.remoteIP());
    Serial.print("Port : ");
    Serial.println(UDP.remotePort());

    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

    String response = 
         "HTTP/1.1 200 OK\r\n"
         "CACHE-CONTROL: max-age=86400\r\n"
         "DATE: Fri, 15 Apr 2016 04:56:29 GMT\r\n"
         "EXT:\r\n"
         "LOCATION: http://" + String(s) + ":80/setup.xml\r\n"
         "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
         "01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
         "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
         "ST: urn:Belkin:device:**\r\n"
         "USN: uuid:" + persistent_uuid + "::urn:Belkin:device:**\r\n"
         "X-User-Agent: redsonic\r\n\r\n";
  
    // Try changing to this if you have problems discovering
    /* https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch/issues/26
    String response =
      "HTTP/1.1 200 OK\r\n"
      "CACHE-CONTROL: max-age=86400\r\n"
      "DATE: Fri, 15 Apr 2016 04:56:29 GMT\r\n"
      "EXT:\r\n"
      "LOCATION: http://" + String(s) + ":80/setup.xml\r\n"
      "OPT: "http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
      "01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
      "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
      "ST: ssdp:all\r\n"
      "USN: uuid:" + persistent_uuid + "::upnp:rootdevice\r\n"
      "X-User-Agent: redsonic\r\n\r\n";
    */

    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    UDP.write(response.c_str());
    UDP.endPacket();                    

    /* add yield to fix UDP sending response. For more informations : https://www.tabsoverspaces.com/233359-udp-packets-not-sent-from-esp-8266-solved */
    yield(); 
  
    Serial.println("Response sent !");
}


// ------------------------------------------ FUNCTIONS ------------------------------------------
void startHttpServer() 
{
    server.on("/index.html", HTTP_GET, []()
    {
      Serial.println("Got Request index.html ...\n");
      server.send(200, "text/plain", "Hello World!");
    });

    server.on("/upnp/control/basicevent1", HTTP_POST, []() 
    {
      Serial.println("########## Responding to  /upnp/control/basicevent1 ... ##########");      

      //for (int x=0; x <= HTTP.args(); x++) {
      //  Serial.println(HTTP.arg(x));
      //}
  
      String request = server.arg(0);      
      Serial.print("request:");
      Serial.println(request);
 
      if(request.indexOf("SetBinaryState") >= 0) 
      {
        if(request.indexOf("<BinaryState>1</BinaryState>") >= 0) 
        {
            Serial.println("Got Turn on request");
            turnOnRelay();
        }
  
        if(request.indexOf("<BinaryState>0</BinaryState>") >= 0) 
        {
            Serial.println("Got Turn off request");
            turnOffRelay();
        }
      }

      if(request.indexOf("GetBinaryState") >= 0) 
      {
        Serial.println("Got binary state request");
        sendRelayState();
      }
            
      server.send(200, "text/plain", "");
    });

    server.on("/eventservice.xml", HTTP_GET, []()
    {
      Serial.println(" ########## Responding to eventservice.xml ... ########\n");
      
      String eventservice_xml = "<scpd xmlns=\"urn:Belkin:service-1-0\">"
        "<actionList>"
          "<action>"
            "<name>SetBinaryState</name>"
            "<argumentList>"
              "<argument>"
                "<retval/>"
                "<name>BinaryState</name>"
                "<relatedStateVariable>BinaryState</relatedStateVariable>"
                "<direction>in</direction>"
                "</argument>"
            "</argumentList>"
          "</action>"
          "<action>"
            "<name>GetBinaryState</name>"
            "<argumentList>"
              "<argument>"
                "<retval/>"
                "<name>BinaryState</name>"
                "<relatedStateVariable>BinaryState</relatedStateVariable>"
                "<direction>out</direction>"
                "</argument>"
            "</argumentList>"
          "</action>"
      "</actionList>"
        "<serviceStateTable>"
          "<stateVariable sendEvents=\"yes\">"
            "<name>BinaryState</name>"
            "<dataType>Boolean</dataType>"
            "<defaultValue>0</defaultValue>"
           "</stateVariable>"
           "<stateVariable sendEvents=\"yes\">"
              "<name>level</name>"
              "<dataType>string</dataType>"
              "<defaultValue>0</defaultValue>"
           "</stateVariable>"
        "</serviceStateTable>"
        "</scpd>\r\n"
        "\r\n";
            
      server.send(200, "text/plain", eventservice_xml.c_str());
    });
    
    server.on("/setup.xml", HTTP_GET, []()
    {
      Serial.println(" ########## Responding to setup.xml ... ########\n");

      IPAddress localIP = WiFi.localIP();
      char s[16];
      sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    
      String setup_xml = "<?xml version=\"1.0\"?>"
            "<root>"
             "<device>"
                "<deviceType>urn:Belkin:device:controllee:1</deviceType>"
                "<friendlyName>"+ friendlyNameString +"</friendlyName>"
                "<manufacturer>Belkin International Inc.</manufacturer>"
                "<modelName>Socket</modelName>"
                "<modelNumber>3.1415</modelNumber>"
                "<modelDescription>Belkin Plugin Socket 1.0</modelDescription>\r\n"
                "<UDN>uuid:"+ persistent_uuid +"</UDN>"
                "<serialNumber>221517K0101769</serialNumber>"
                "<binaryState>0</binaryState>"
                "<serviceList>"
                  "<service>"
                      "<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
                      "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
                      "<controlURL>/upnp/control/basicevent1</controlURL>"
                      "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
                      "<SCPDURL>/eventservice.xml</SCPDURL>"
                  "</service>"
              "</serviceList>" 
              "</device>"
            "</root>\r\n"
            "\r\n";
            
        server.send(200, "text/xml", setup_xml.c_str());
        
        Serial.print("Sending :");
        Serial.println(setup_xml);
    });

    // openHAB support
    server.on("/on.html", HTTP_GET, [](){
         Serial.println("Got Turn on request");
         server.send(200, "text/plain", "turned on");
         turnOnRelay();
       });
 
     server.on("/off.html", HTTP_GET, [](){
        Serial.println("Got Turn off request");
        server.send(200, "text/plain", "turned off");
        turnOffRelay();
       });
 
      server.on("/status.html", HTTP_GET, [](){
        Serial.println("Got status request");
 
        String statrespone = "0"; 
        if (relayState) {
          statrespone = "1"; 
        }
        server.send(200, "text/plain", statrespone);
      
    });
    
    server.begin();  
    Serial.println("HTTP Server started ..");
}


// ------------------------------------------ FUNCTIONS ------------------------------------------
boolean connectUDP()
{
  boolean state = false;
  
  Serial.println("");
  Serial.println("Connecting to UDP");
  
  if(UDP.beginMulticast(WiFi.localIP(), ipMulti, portMulti)) 
  {
    Serial.println("Connection successful");
    state = true;
  }
  else
  {
    Serial.println("Connection failed");
  }
  
  return state;
}

// ------------------------------------------ FUNCTIONS ------------------------------------------
void turnOnRelay() 
{
 digitalWrite(relayPin, HIGH); // turn on relay with voltage HIGH 
 relayState = true;

  String body = 
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>\r\n"
      "<u:SetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
      "<BinaryState>1</BinaryState>\r\n"
      "</u:SetBinaryStateResponse>\r\n"
      "</s:Body> </s:Envelope>";

  server.send(200, "text/xml", body.c_str());
        
  Serial.print("Sending :");
  Serial.println(body);
}

void turnOffRelay() 
{
  digitalWrite(relayPin, LOW);  // turn off relay with voltage LOW
  relayState = false;

  String body = 
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>\r\n"
      "<u:SetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
      "<BinaryState>0</BinaryState>\r\n"
      "</u:SetBinaryStateResponse>\r\n"
      "</s:Body> </s:Envelope>";

  server.send(200, "text/xml", body.c_str());
        
  Serial.print("Sending :");
  Serial.println(body);
}

// ------------------------------------------ FUNCTIONS ------------------------------------------
void sendRelayState() 
{
  String body = 
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>\r\n"
      "<u:GetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
      "<BinaryState>";
      
  body += (relayState ? "1" : "0");
  
  body += "</BinaryState>\r\n"
      "</u:GetBinaryStateResponse>\r\n"
      "</s:Body> </s:Envelope>\r\n";
 
   server.send(200, "text/xml", body.c_str());
}


// --------- Software Reset using the Watchdog Timer ------------------------------------------
void reboot() 
{
  delay(3300);
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while (1) {}
}
