#include <SPI.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define ONE_WIRE_BUS 2
#define UP_BUTTON 3
#define SELECT_BUTTON 4
#define DOWN_BUTTON 5

bool upButtonState = false;
bool lastUpButtonState = false;
bool selectButtonState = false;
bool lastSelectButtonState = false;
bool downButtonState = false;
bool lastDownButtonState = false;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature temperatureSensor(&oneWire);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// state
enum States {LOGGING, WIFI_CONFIG, ACCESS_POINT_CONFIG, PASSWORD_CONFIG};

// SPI commands
const byte SIMO_TEMPERATURE = B00000001;
const byte SIMO_CONNECT_WIFI = B00000010;
const byte SIMO_ACCESS_POINT = B00000011;
const byte SIMO_PASSWORD = B00000100;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

float temperature = 0;



void setup() {
  Serial.begin(115200);
  
  digitalWrite(SS, HIGH); // disable Slave Select
  SPI.begin ();
  SPI.setClockDivider(SPI_CLOCK_DIV128);//divide the clock by 128

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
    Serial.println("SSD1306 allocation failed");
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.display();
}

void loop() {
  upButtonState = digitalRead(UP_BUTTON);
  selectButtonState = digitalRead(SELECT_BUTTON);
  downButtonState = digitalRead(DOWN_BUTTON);

  static States state;
  
  switch(state) {
    case LOGGING :
      state = logging();
      break;
      
    case WIFI_CONFIG :
      state = wifiConfig();
      break;
  
    case PASSWORD_CONFIG : {
      static unsigned int position;
      static String password;
      if (editValue(&password, &position)) {
        state = WIFI_CONFIG;
        position = 0;

        // send temperature data to slave
        digitalWrite(SS, LOW); // enable Slave Select
        SPI.transfer(SIMO_PASSWORD); // transmit command
        for (unsigned int i = 0; i < password.length(); i++) { // dransmit data asociated with command
          SPI.transfer(password[i]);
        }
        SPI.transfer('\r');
        digitalWrite(SS, HIGH); // disable Slave Select
        
      } else {
        state = PASSWORD_CONFIG;
      }
    } break;

    case ACCESS_POINT_CONFIG : {
      static unsigned int position;
      static String accesPoint;
      if (editValue(&accesPoint, &position)) {
        state = WIFI_CONFIG;
        position = 0;

        // send temperature data to slave
        digitalWrite(SS, LOW); // enable Slave Select
        SPI.transfer(SIMO_ACCESS_POINT); // transmit command
        for (unsigned int i = 0; i < accesPoint.length(); i++) { // dransmit data asociated with command
          SPI.transfer(accesPoint[i]);
        }
        SPI.transfer('\r');
        digitalWrite(SS, HIGH); // disable Slave Select
        
      } else {
        state = ACCESS_POINT_CONFIG;
      }
    } break;
   
    default :
      state = LOGGING;
      break;
  }
  lastUpButtonState = upButtonState;
  lastSelectButtonState = selectButtonState;
  lastDownButtonState = downButtonState;
}

States logging () {

  if ((selectButtonState == true) && (lastSelectButtonState == false)) { // select button pressed switch to config menu
    return WIFI_CONFIG;
  }
  
  temperatureSensor.requestTemperatures();
  temperature = temperatureSensor.getTempCByIndex(0);

  // send temperature data to slave
  digitalWrite(SS, LOW); // enable Slave Select
  SPI.transfer(SIMO_TEMPERATURE);
  String sendData = String(temperature);
  for (unsigned int i = 0; i < sendData.length(); i++) {
    SPI.transfer(sendData[i]);
  }
  SPI.transfer('\r');
  digitalWrite(SS, HIGH); // disable Slave Select
      
  display.clearDisplay();
  display.drawRect(0, 0, SCREEN_WIDTH, 11, SSD1306_WHITE);
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(2, 2);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.print("Temperature: ");
  display.print(temperature);
  display.print(char(248)); // degree charicter
  display.print("C");
  display.display();

  return LOGGING;
}

States wifiConfig () {
  enum MenuItem {BACK, ACCESS_POINT, PASSWORD, APPLY, END};
  const String menuItemText[] = {"Back", "AP", "Paswrd", "Ok"};
  static MenuItem menuPosition;

  if ((upButtonState == true) && (lastUpButtonState == false)) { // menu up
    if (menuPosition == 0) {
      menuPosition = END - 1;
    } else {
      menuPosition = menuPosition - 1;
    }
    
  } else if ((downButtonState == true) && (lastDownButtonState == false)) { // menu down
    if (menuPosition == END - 1) {
      menuPosition = 0;
    } else {
      menuPosition = menuPosition + 1;
    }
    
  } else if ((selectButtonState == true) && (lastSelectButtonState == false)) { // select button pressed
    switch(menuPosition) {
      case BACK :
        return LOGGING;
        break;
        
      case ACCESS_POINT : {
        return ACCESS_POINT_CONFIG;
        break; }
  
      case PASSWORD : {
        return PASSWORD_CONFIG;
        break; }

      case APPLY : {
        Serial.println("Apply");
        // send temperature data to slave
        digitalWrite(SS, LOW); // enable Slave Select
        SPI.transfer(SIMO_CONNECT_WIFI);
        digitalWrite(SS, HIGH); // disable Slave Select
        return LOGGING;
        break; }
    }
  }
  
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale

  for (MenuItem i = 0; i < END; i = i + 1) {
    display.setCursor(2, 2 + i * 10);     // Start at top-left corner
    if (menuPosition == i) {
      display.drawRect(0, i*10, SCREEN_WIDTH, 11, SSD1306_WHITE);
    }
    display.setTextColor(SSD1306_WHITE);
    display.print(menuItemText[i]);
  }
  display.display();

  return WIFI_CONFIG;
}

bool editValue (String* value, unsigned int* position) { // returns true if value finished entering false if otherwise
  const int maxLength = 5; // set to one less than the maximum amount of charicters
  const int symbolsLength = 1+26;
  const char symbols[symbolsLength] = " abcdefghijklmnopqrstuvwxyz"; // list of posable chars
  unsigned int charIndex; // current index into above string 

  // increase the length of the string to edit char at value
  while (value->length() < *position + 1) { 
    *value = *value + " ";
  }

  // get charIndex at position
  for (unsigned int i = 0; i < symbolsLength; i++) {
    if ((*value)[value->length() - 1] == symbols[i]) {
      charIndex = i;
      break;
    }
  }

  // button input
  if ((upButtonState == true) && (lastUpButtonState == false)) { // menu up cycle down a char at position
    charIndex--;
    if (charIndex == -1) {
      charIndex = symbolsLength - 1;
    }
    (*value)[*position] = symbols[charIndex];
  } else if ((downButtonState == true) && (lastDownButtonState == false)) { // menu down cycle up a char at position
    charIndex++;
    if (charIndex == symbolsLength) {
      charIndex = 0;
    }
    (*value)[*position] = symbols[charIndex];

  } else if ((selectButtonState == true) && (lastSelectButtonState == false)) { // select button pressed increase position or exit
    if (*position == maxLength) { // ran out of chars exit editor
      while ((*value)[value->length() - 1] == ' ') { // remove empty spaces at the end
        value->remove(value->length() - 1);
      }
      return true;
    }
    *position = *position + 1; // go to next position
  }

  // update display
  display.clearDisplay();
  display.drawRect(0, 4, maxLength * 7 + 4, 11, SSD1306_WHITE);
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(2, 6);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.print(*value);
  display.drawTriangle(*position * 6 + 4, 0, *position * 6 + 2, 2, *position * 6 + 6, 2, SSD1306_WHITE); // draw charicter highlight
  display.drawTriangle(*position * 6 + 4, 18, *position * 6 + 2, 16, *position * 6 + 6, 16, SSD1306_WHITE); // draw charicter highlight
  display.display();
  
  return false;
}
