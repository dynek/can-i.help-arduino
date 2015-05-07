// Not so pretty but these are things I don't want to share on github :-)

// prototyping
void display_on_lcd(char * text, int row = 1, bool centered = false, bool clear_lcd = false);
int send_request();
int read_response();

// parse.com API
#define API_SERVER_HOSTNAME "api.parse.com"
#define API_SERVER_PORT 80
#define URL_PATH "/1/classes/<class>"
#define APPLICATION_ID "0w95nbYtKN8uoFmPA8oMUjLhnzpDYPCZzWZ41A6mg"
#define REST_API_KEY "hFhJ9C1q7FIBkbH8M4QwhksLAPeykVBMWet9Qqvm0"
#define USER_ID "y0i344AsYx"

// network stuff
#define HTTP_USER_AGENT "my user agent/1.0"
#define BUFFER_SIZE 192 // biggest buffer will be payload and basically it is 148 chars without message so max should be 180

// LCD
#define LCD_COL 16
#define LCD_ROW 2
#define PIN_RS 14
#define PIN_ENABLE 15
#define PIN_D4 16
#define PIN_D5 17
#define PIN_D6 18
#define PIN_D7 19

// pins used for RGB LED
#define REDPIN 3
#define GREENPIN 5
#define BLUEPIN 6
