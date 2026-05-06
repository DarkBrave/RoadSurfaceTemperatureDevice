#include <DS3231.h> // RTC library
DS3231 rtc; // declare RTC type
bool rtcCentury = false;
bool rtcH12Flag = false;
bool rtcPmFlag;

int rtcYear, rtcMonth, rtcDay, rtcHour, rtcMinute, rtcSecond;

#include <SD.h> // SD card module library
const int chipSelect = 10; // SD card SPI stuff? whatever CSK pin in plugged into, arduino default here
File sdFile; // SD card variable name, can be assigned to open different files
String SD_FILENAME = "LOG.csv"; // SD card file name we want to write to

#include <DFRobot_MLX90614.h> // MLX IR sensor library
DFRobot_MLX90614_I2C irSensor; // declare IR sensor type

#include "Adafruit_SHT4x.h" // SHT4x ambient sensor library
Adafruit_SHT4x sht4 = Adafruit_SHT4x();  // declare ambient sensor type

#include <Adafruit_GFX.h> // generic display library for screes
#include <Adafruit_SSD1306.h> // specific OLED chip library
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire); // declares OLED display size/type, 32x128 in this case
String oledLine = ""; // sets diplsay text buffer before writing

const int buttonPin = 2; // pin for data gathering from the button
int buttonState = 0;  // variable for reading the pushbutton status
bool isData; // varialbe for CSV to decide if the currently logged data is marked usable, based on buttonState

// setup runs once when the code turns on, sets up everything to work
void setup() {
  Serial.begin(9600); // starts computer connection to send debug/logs/commands
  Wire.begin(); // starts I2C bus for sensors/RTC

  //while (!Serial); // STOPS code until computer plugged in, enable for debugging but will prevent battery operation

  // test display with splash screen
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // sets up OLED display type
  Serial.println("OLED intialized"); // logs that display initilization worked
  display.display(); // tests display with Adafruit Logo
  delay(500); // leaves display as-is so user can verify
  display.clearDisplay(); // blanks dispklay as the rest loads
  display.display(); // loads blank display into memory
  // set font info for display
  display.setTextSize(1); // sets font scale to be small size
  display.setTextColor(WHITE); // sets font to be white text (monochrome display)
  

  // SD Card Setup:
  Serial.print("Initializing SD card..."); // logs to show start of SD setup
  // checks to see if SD card is connected
  if (!SD.begin(chipSelect)) {
    throwError("SD initialization failed.");
    while (true); // HALTS if SD card isn't inserted
  }
  Serial.println("SD card initialization done."); // logs to show end of SD setup

  // headeders for CSV file
  if (!SD.exists(SD_FILENAME)) {
    writeSD("date,time,irChipTemp,irObjectTemp,ambientTemp,ambientHumidity,isData"); // creates headers for CSV data if the CSV doesn't exist yet
  }

  // Setup IR Sensor
  while( NO_ERR != irSensor.begin() ){ 
    throwError("Communication with device failed, please check connection"); 
    delay(3000); 
  } // WAITS until IR temperature sensor is connected properly

  // test IR sensor sleep mode
  irSensor.enterSleepMode(); 
  delay(50); 
  irSensor.enterSleepMode(false); 
  delay(200);

  // Setup/check ambient SHT sensor
  if (!sht4.begin()) { 
    throwError("Couldn't find SHT4x ambient sensor!"); 
    while (true); 
  } // HALTS if ambient sensor isn't connected
  Serial.print("Found SHT4x sensor, Serial number 0x");
  Serial.println(sht4.readSerial(), HEX); // logs sensor serial for logging
  sht4.setPrecision(SHT4X_HIGH_PRECISION); // sets precision to HIGH (also can be MED or LOW)
  sht4.setHeater(SHT4X_NO_HEATER); // disables heater in sensor because we don't need it
}

void loop() {
  delay(1000); // short wait before gathering data

  // record IR data from sensor
  float irChipTemp = irSensor.getAmbientTempCelsius(); 
  float irObjectTemp = irSensor.getObjectTempCelsius();

  // record ambient data from sensor
  sensors_event_t humidity, temp; 
  sht4.getEvent(&humidity, &temp);
  float ambientTemp = temp.temperature;
  float ambientHumidity = humidity.relative_humidity;

  buttonState = digitalRead(buttonPin); // reads current button state
  // assigns data variable to state of button being pushed
  if (buttonState == HIGH) {
    isData = false;
  } else if (buttonState == LOW) {
    isData = true;
  }  else {
    throwError("Button not detected.");
  }

  getTime(); // gets time and assigns it to the global variables

  String date = String(rtcYear) + "-" + String(rtcMonth) + "-" + String(rtcDay); // creates combined date string for csv/screen
  String time = String(rtcHour) + ":" + String(rtcMinute) + ":" + String(rtcSecond); // creates combined time string for csv/screen
  // create the CSV and serial data row combining time/data/data to one string
  String csvRow = date + "," + time + "," + String(irChipTemp) + "," + String(irObjectTemp) + "," + String(ambientTemp) + "," + String(ambientHumidity) + "," + String(isData);


  // writes data to dsiplay
  display.clearDisplay(); // clears/resets the display buffer to be blank
  display.setCursor(0,0); // goes to the top left to begin writing

  // writes date/time to the first/top row
  oledLine = date + " | " + time; 
  display.println(oledLine);
  // writes IR data to the second row
  oledLine = "Ob: " + String(irObjectTemp) + " | Ch: " + String(irChipTemp);
  display.println(oledLine);
  // writes ambient data to the third row
  oledLine = "Am: " + String(ambientTemp) + " | Hu: " + String(ambientHumidity);
  display.println(oledLine);
  // writes status of button to the fourth/bottom row
  oledLine = "Status: " + String(isData);
  display.println(oledLine);

  display.display(); // shows/updates the actual display with our new display buffer

  // handles commands from the computer to help with debug/logging
  if(Serial.available() != 0) {
    String serialRead = Serial.readString(); // reads the command over serial if new input is detected
    serialRead.trim(); // removes leading/trailing spaces for clean input

    if (serialRead == "timeset") {
      timeSet(); // triggers protocol to send the time updated over serial
    } else if (serialRead == "dateset") {
      dateSet(); // triggers protocol to send the date updated over serial
    } else if (serialRead == "cleardisplay") {
      // debug system by clearing the display of garbage data
      display.clearDisplay();
      display.display();
      Serial.println("Display cleared");
    } else {
      // throws short error if weird/bad data is received
      String errorMessage = "Invalid serial data received: " + serialRead;
      throwError(errorMessage);
    }
  }
}

void getTime() {
  // record date/time from RTC and assign to global variables
  rtcYear = rtc.getYear();
  rtcMonth = rtc.getMonth(rtcCentury); // RTC century bit is weird niche thing we don't care about
  rtcDay = rtc.getDate();
  rtcHour = rtc.getHour(rtcH12Flag, rtcPmFlag); // ensure 24 hour time (no AM/PM)
  rtcMinute = rtc.getMinute();
  rtcSecond = rtc.getSecond();
}

void writeSD(String sdData) {
  sdFile = SD.open(SD_FILENAME, FILE_WRITE); // open SD card file
  // record CSV row to SD card
  if (sdFile) {
    Serial.print("Writing data to CSV file: ");
    sdFile.println(sdData); // log data to CSV
    Serial.println(sdData); // print in console

    sdFile.close();
  } else {
    // if the file didn't open, print an error:
    String errorMessage = "error opening SD file to log: " + sdData;
    throwError(errorMessage);
  }
}

void throwError(String errorMessage) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("IMPORTANT:");
  display.println(errorMessage);
  display.display();

  Serial.println(errorMessage);
  delay(2000);
  display.clearDisplay();
}

void timeSet() {
  rtc.setHour(getSerialNumber("Enter the current hour (24 hour time)..."));
  rtc.setMinute(getSerialNumber("Enter the current minute..."));
  rtc.setSecond(getSerialNumber("Enter the current second..."));
}
void dateSet() {
  rtc.setYear(getSerialNumber("Enter the current year (last two digits)..."));
  rtc.setMonth(getSerialNumber("Enter the current month..."));
  rtc.setDate(getSerialNumber("Enter the current day..."));
}


int getSerialNumber(String prompt) {
  Serial.println(prompt);
  while (Serial.available() == 0) {}
  String serialRead = Serial.readString();
  serialRead.trim();
  byte serialReadNum = serialRead.toInt();
  return serialReadNum;
}