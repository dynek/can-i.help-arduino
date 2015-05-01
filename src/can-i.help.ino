//
// author: Pierrick Brossin <pierrick@bs-network.net>
// version: 0.1
// hardware: Arduino Uno with ESP8266, one RGB LED and one LCD display
// software: Account on parse.com (application ID and Rest API key) with http to https proxy in front (nginx will do)
//
// why NDEBUG: http://stackoverflow.com/questions/5473556/what-is-the-ndebug-preprocessor-macro-used-for-on-different-platforms

// -----------------------------
// todo: function to output hex to LED
// todo: set LED color
// todo; connect to WiFi using ESP8266
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
#include "can-i.help.h";
#ifdef NDEBUG
  #include <SPI.h>
#endif
#include <Ethernet.h>
#include <ArduinoJson.h> // this guy *really* rocks - https://github.com/bblanchon/ArduinoJson
#include <LiquidCrystal.h>

// variables / instanciations
LiquidCrystal lcd(PIN_RS, PIN_ENABLE, PIN_D4, PIN_D5, PIN_D6, PIN_D7);
EthernetClient client;
byte mac[] = {  0x90, 0xA2, 0xDA, 0x00, 0x94, 0xD2 }; // MAC address of your device
const char* color;
const char* message;

// clock watcher
unsigned long previous_millis = 0; // store last time we updated led + LCD
unsigned long interval = 10000; // will most likely be 1 sec once "in prod"

// setup program
void setup() {
  #ifdef NDEBUG
    // hold on for 1 sec so Serial Monitor can be opened
    delay(1000);
    // start serial port
    Serial.begin(9600);
  #endif
  
  // setup lcd column and rows
  lcd.begin(LCD_COL, LCD_ROW);
  display_on_lcd("Initializing ESP8266", 1, false, true);

  // start ethernet connection
  while(!Ethernet.begin(mac)) { // no need to check millis() as it takes significant amount of time to try to obtain dhcp info
    DEBUG_PRINTLN("Ethernet DHCP failed - retry");
    display_on_lcd("Ethernet DHCP failed - retry", 1, false, true);
  }

  // connection successful, display IP address
  DEBUG_PRINT("IP address: ");
  display_on_lcd("IP address:", 1, false, true);
  DEBUG_PRINTLN(Ethernet.localIP());
  lcd.setCursor(0, 1);
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    lcd.print(Ethernet.localIP()[thisByte], DEC);
    if(thisByte < 3) { lcd.print("."); }
  }
  delay(3000);
  
  // set default lcd message
  display_on_lcd(HTTP_USER_AGENT, 1, false, true);
  delay(3000);
}

// well ... loop
void loop() {  
  // initiate connection to api server if interval since last millis passed
  if((previous_millis == 0 || (millis() - previous_millis > interval)) && !client.connected()) {
    if(!send_request()) {
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
    display_on_lcd((char *)message, 1, false, true);

    // give client time to settle down
    delay(500);

    // stop the client:
    DEBUG_PRINTLN("Disconnect");
    client.stop();

    // last millis is now
    previous_millis = millis();
  }
}

void display_on_lcd(char * text, int row, bool centered, bool clear_lcd) {
  // clear LCD ?
  if(clear_lcd) { lcd.clear(); }
  
  // set cursor
  int col = 0;
  if(centered && strlen(text) <= (LCD_COL - 2)) { col = int((LCD_COL-strlen(text))/2); }
  if(row > LCD_ROW) { row = LCD_ROW; }
  lcd.setCursor(col, row - 1);
  
  // display message
  if((row == 1 && strlen(text) <= LCD_COL) or row == 2) {
    lcd.print(text);
  } else if (row == 1 && strlen(text) > LCD_COL) {
    // what shall we print on first line ?
    char temp[LCD_COL] = {0}; strncpy(temp, text, LCD_COL); temp[LCD_COL] = '\0'; // first LCD_COL characters
    char *space = strrchr(temp, ' '); // find last space char
    if(space == NULL) { // too many chars printing whole line
      lcd.print(text);
    } else {
      strncpy(temp, text, space - temp); temp[space - temp] = '\0'; // first line
      lcd.print(temp); // print line
      lcd.setCursor(0, 1);
      lcd.print(text + strlen(temp) + 1); // print second line
    }
  }
}

// function that sends request to the parse API
int send_request() {
  // let's check if we can get a connection to be established
  if (client.connect(API_SERVER_HOSTNAME, API_SERVER_PORT)) {
    DEBUG_PRINTLN("Successfully connected - sending request");
    client.print("GET "); client.print(URL_PATH); client.print("?keys=color,message&where={\"UserID\":{\"__type\":\"Pointer\",\"className\":\"_User\",\"objectId\":\""); client.print(USER_ID); client.println("\"}} HTTP/1.1");
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
