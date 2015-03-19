//
// author: Pierrick Brossin <pierrick@bs-network.net>
// version: 0.1
// hardware: Arduino Uno with WiFi shield, one RGB LED and one LCD display
// software: Account on parse.com (application ID and Rest API key)
//
// why NDEBUG: http://stackoverflow.com/questions/5473556/what-is-the-ndebug-preprocessor-macro-used-for-on-different-platforms


// -----------------------------
// todo: function to output hex to LED
// todo: set LED color
// todo: display stuff on LCD
// todo; connect to WiFi when using WiFi shield
// -----------------------------

// debug
#define NDEBUG // comment line to disable debugging
#ifdef NDEBUG
  #define DEBUG_PRINT(x)      Serial.print (x)
  #define DEBUG_PRINTLN(x)    Serial.println (x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// include libraries
#ifdef NDEBUG
  #include <SPI.h>
#endif
#include <Ethernet.h>
#include <ArduinoJson.h> // this guy rocks - https://github.com/bblanchon/ArduinoJson

// parse.com API
#define API_SERVER_HOSTNAME "api.parse.com"
#define API_SERVER_PORT 80
#define URL_PATH "/1/classes/<class>"
#define APPLICATION_ID "0w95nbYtKN8uoFmPA8oMUjLhnzpDYPCZzWZ41A6mg"
#define REST_API_KEY "hFhJ9C1q7FIBkbH8M4QwhksLAPeykVBMWet9Qqvm0"
//#define USER_ID ""
const char* color;
const char* message;

// ethernet stuff
EthernetClient client;
byte mac[] = {  0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#define HTTP_USER_AGENT "my user agent/1.0"
#define BUFFER_SIZE 192 // biggest buffer will be payload and basically it is 181 chars max, let's give it some room for longer messages

// clock watcher
unsigned long previous_millis = 0; // store last time we updated led + LCD
unsigned long interval = 10000; // will most likely be 1 sec once "in prod"

// pins used for RGB LED
//const int redPin = 9;
//const int greenPin = 10;
//const int bluePin = 11;

// default LED color
//define DEFAULT_LED_COLOR "D18030"

// write color to pins
//analogWrite(redPin, currentColorValueRed);
//analogWrite(bluePin, currentColorValueBlue);
//analogWrite(greenPin, currentColorValueGreen);

// setup program
void setup() {
  #ifdef NDEBUG
    // hold on for 1 sec so Serial Monitor can be opened
    delay(1000);
    // start serial port
    Serial.begin(9600);
  #endif

  // set up pins used by RGB LED
  //DEBUG_PRINTLN("Setup RGB pins");
  //pinMode(redPin, OUTPUT);
  //pinMode(greenPin, OUTPUT);
  //pinMode(bluePin, OUTPUT);

  // start ethernet connection
  while(!Ethernet.begin(mac)) { // no need to check millis() as it takes significant amount of time to try to obtain dhcp info
    DEBUG_PRINTLN("Ethernet DHCP failed - retry");
  }

  // connection successful, display IP address
  DEBUG_PRINT("IP address: ");
  DEBUG_PRINTLN(Ethernet.localIP());

  // set default led color + lcd message here
}

// well ... loop
void loop() {
  // initiate connection to api server if interval since last millis passed
  if((previous_millis == 0 || (millis() - previous_millis > interval)) && !client.connected()) {
    if(!initiate_connection_to_api()) {
      DEBUG_PRINTLN("Could not connect to server");

      // last millis is now
      previous_millis = millis();

      return; // void loop() will retrigger automatically
    }
  }

  // if there's an available connection read data
  if(client.connected() and client.available()) {
    // read response
    if(read_response()) {
      DEBUG_PRINT("color: "); DEBUG_PRINTLN(color);
    }
    // update LCD message
    DEBUG_PRINT("message: "); DEBUG_PRINTLN(message);

    // give client time to settle down
    delay(500);

    // stop the client:
    DEBUG_PRINTLN("Disconnect");
    client.stop();

    // last millis is now
    previous_millis = millis();
  }
}

// function that initiates a connection to the parse API and sends expected headers
int initiate_connection_to_api() {
  // let's check if we can get a connection to be established
  if (client.connect(API_SERVER_HOSTNAME, API_SERVER_PORT)) {
    DEBUG_PRINTLN("Successfully connected - sending request");
    client.print("GET "); client.print(URL_PATH); client.println(" HTTP/1.1");
    client.print("Host: "); client.println(API_SERVER_HOSTNAME);
    client.print("User-Agent: "); client.println(HTTP_USER_AGENT);
    client.print("X-Parse-Application-Id: "); client.println(APPLICATION_ID);
    client.print("X-Parse-REST-API-Key: "); client.println(REST_API_KEY);
    client.println("Connection: close");
    client.println();
    return true;
  }

  // if above condition did not return true
  return false;
}

// function that reads and parse response from parse.com
int read_response() {
  char character, buffer[BUFFER_SIZE] = {0};
  int loop=0;
  boolean is_payload=false;

  while (client.available() > 0) {
    // "line" longer than buffer ?
    if(loop >= BUFFER_SIZE) {
        message="buffer too small for data";
        return false;
    }
    
    // read char
    character = client.read();

    // do we care about \r ?
    if(character == '\r') { continue; }

    // append char
    if(character != '\n') { buffer[loop++] = character; continue; } // continue, because we don't want to lose processing time

    // end buffer upon new line
    if(character == '\n' && strlen(buffer) > 0) { buffer[loop] = '\0'; }

    // detect beginning of payload
    if(character == '\n' && strlen(buffer) == 0) {
      DEBUG_PRINTLN("Payload coming...");
      is_payload=true;
      continue; // no need to keep going
    }

    // time to analyze what's in buffer
    if(buffer[loop] == '\0') {
      // http status & headers
      if(!is_payload) {
        // check if buffer is http status code
        if(!strncmp(buffer, "HTTP", 4) == 0) { loop=0; continue; } // don't lose processing time on useless headers
        // there are better ways to extract http status code but we're not doing rocket science here :-)
        char temp[3] = {0}; strncpy(temp, buffer+9, 3); temp[3] = '\0';
        // not 200 ok?
        if(strcmp(temp, "200") != 0) {
          message=buffer;
          return false;
        }
      } else { // payload
        DEBUG_PRINT("PAYLOAD: "); DEBUG_PRINTLN(buffer);
        // instanciate json object
        StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
        // parse payload
        JsonObject& root = jsonBuffer.parseObject(buffer);
        // was able to parse?
        if (!root.success()) {
          message="parsing json failed";
          return false;
        }
        // fetch data
        color = root["results"][0]["color"];
        message = root["results"][0]["message"];
        // missing data ?
        if(color ==  NULL || message == NULL) {
          message="missing value in json";
          return false;
        }
      }
      loop=0;
    }
  }
  // return http status code
  return true;
}
