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
float irChipTemp;
float irObjectTemp;

#include "Adafruit_SHT4x.h" // SHT4x ambient sensor library
Adafruit_SHT4x sht4 = Adafruit_SHT4x();  // declare ambient sensor type
float ambientTemp;
float ambientHumidity;

#include <Adafruit_GFX.h> // generic display library for screes
#include <Adafruit_SSD1306.h> // specific OLED chip library
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire); // declares OLED display size/type, 32x128 in this case
String oledLine = ""; // sets diplsay text buffer before writing

const int buttonPin = 2; // pin for data gathering from the button
int buttonState = 0;  // variable for reading the pushbutton status
int oldData = 0;
bool isData = false; // varialbe for CSV to decide if the currently logged data is marked usable, based on buttonState
int dataChunk = 0; // variable for current "chunk" of button being pushed, resetable

// setup runs once when the code turns on, sets up everything to work
void setup() {
  Serial.begin(9600); // starts computer connection to send debug/logs/commands
  Wire.begin(); // starts I2C bus for sensors/RTC
  pinMode(buttonPin, INPUT_PULLUP); // sets pin to read

  //while (!Serial); // STOPS code until computer plugged in, enable for debugging but will prevent battery operation

  // test display with splash screen
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // sets up OLED display type
  Serial.println("OLED intialized"); // logs that display initilization worked
  display.display(); // tests display with Adafruit Logo
  delay(500); // leaves display as-is so user can verify
  clearDisplay(true);
  // set font info for display
  display.setTextSize(1); // sets font scale to be small size
  display.setTextColor(WHITE); // sets font to be white text (monochrome display)
  

  // SD Card Setup:
  Serial.print("Initializing SD card..."); // logs to show start of SD setup
  // checks to see if SD card is connected
  while(!SD.begin(chipSelect)) {
    throwError("SD initialization failed.");
    delay(3000);
  }
  Serial.println("SD card initialization done."); // logs to show end of SD setup

  // headeders for CSV file
  if (!SD.exists(SD_FILENAME)) {
    writeSD("date,time,irChipTemp,irObjectTemp,ambientTemp,ambientHumidity,isData,dataChunk"); // creates headers for CSV data if the CSV doesn't exist yet
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
  sdFile = SD.open(SD_FILENAME, FILE_WRITE); // open SD card file based on specified name
  sdFile.close();

  updateSensors();

  updateButtonState();

  getTime(); // gets time and assigns it to the global variables

  String date = String(rtcYear) + "-" + String(rtcMonth) + "-" + String(rtcDay); // creates combined date string for csv/screen
  String time = String(rtcHour) + ":" + String(rtcMinute) + ":" + String(rtcSecond); // creates combined time string for csv/screen
  // create the CSV and serial data row combining time/data/data to one string
  String csvRow = date + "," + time + "," + String(irChipTemp) + "," + String(irObjectTemp) + "," + String(ambientTemp) + "," + String(ambientHumidity) + "," + String(isData) + "," + String(dataChunk);
  writeSD(csvRow);

  // writes data to dsiplay
  clearDisplay(false); // clears/resets the display buffer to be blank

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
  oledLine = "Status: " + String(isData) + " | Chunk: " + String(dataChunk);
  display.println(oledLine);

  display.display(); // shows/updates the actual display with our new display buffer

  // handles commands from the computer to help with debug/logging
  if(Serial.available() != 0) {
    handleSerialCommands();
  }
}

void clearDisplay(bool displayImmediately) {
  display.clearDisplay();
  display.setCursor(0,0);
  if (displayImmediately == true) {
    display.display();
    Serial.println("Display cleared");
  } else {
    Serial.println("Display buffer cleared");
  }
}


void handleSerialCommands() {
  String serialRead = Serial.readString(); // reads the command over serial if new input is detected
  serialRead.trim(); // removes leading/trailing spaces for clean input

  if (serialRead == "timeset") {
    timeSet(); // triggers protocol to send the time updated over serial
  } else if (serialRead == "dateset") {
    dateSet(); // triggers protocol to send the date updated over serial
  } else if (serialRead == "chunkreset") {
    dataChunk = 0; // resets the chunk counter back to zero
  } else if (serialRead == "cleardisplay") {
    clearDisplay(true); // debug system by clearing the display of garbage data
  } else {
    throwError("Invalid serial data received: " + serialRead); // throws short error if weird/bad data is received
  }
}

void updateSensors() {
  // record IR data from sensor
  irChipTemp = irSensor.getAmbientTempCelsius(); 
  irObjectTemp = irSensor.getObjectTempCelsius();

  // record ambient data from sensor
  sensors_event_t humidity, temp; 
  sht4.getEvent(&humidity, &temp);
  ambientTemp = temp.temperature;
  ambientHumidity = humidity.relative_humidity;

  if(irObjectTemp <= 0 || irObjectTemp >= 100) {
    throwError("Extreme IR Object temperature detected",irObjectTemp);
  }
}

void updateButtonState() {
  buttonState = digitalRead(buttonPin); // reads current button state
  // assigns data variable to state of button being pushed
  if (buttonState == HIGH) {
    oldData = isData;
    isData = false;
  } else if (buttonState == LOW) {
    // if this is the start of a new "chunk"/past row was not pushed, increase the chunk counter
    if (isData == false && oldData == 0) {
      dataChunk++;
    }
    isData = true;
  }  else {
    throwError("Button not detected.");
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
  sdFile = SD.open(SD_FILENAME, FILE_WRITE); // open SD card file based on specified name
  // records CSV row to SD card
  if (sdFile) {
    Serial.print("Writing data to CSV file: ");
    sdFile.println(sdData); // log data row to CSV
    Serial.println(sdData); // print it as well in the console
    sdFile.close();
  } else {
    // if the file didn't open, print an error:
    throwError("Error opening SD card to write", sdData);
    if (!SD.begin(chipSelect)) {
      throwError("SD initialization failed.");
    }
  }
}

void throwError(String errorMessage) {
  clearDisplay(false);// clears display to allow for more room for the error message
  // formats/shows text for the error message to display based on specified string
  display.println("IMPORTANT:");
  display.println(errorMessage);
  // displays the error for a few seconds to allow human reading
  display.display();
  Serial.println(errorMessage);
  delay(2000);
  clearDisplay(true);
}
void throwError(String errorMessage, String errorData) {
  clearDisplay(false);// clears display to allow for more room for the error message
  // formats/shows text for the error message to display based on specified string
  display.println("IMPORTANT:");
  display.println(errorMessage);
  // displays the error for a few seconds to allow human reading
  display.display();
  Serial.print(errorMessage);
  Serial.print(": ");
  Serial.println(errorData);
  delay(2000);
  clearDisplay(true);
}
void throwError(String errorMessage, int errorData) {
  clearDisplay(false);// clears display to allow for more room for the error message
  // formats/shows text for the error message to display based on specified string
  display.println("IMPORTANT:");
  display.println(errorMessage);
  // displays the error for a few seconds to allow human reading
  display.display();
  Serial.print(errorMessage);
  Serial.print(": ");
  Serial.println(errorData);
  delay(2000);
  clearDisplay(true);
}

void timeSet() {
  // one-by-one prompts user for each aspect of the time
  rtc.setHour(getSerialNumber("Enter the current hour (24 hour time)..."));
  rtc.setMinute(getSerialNumber("Enter the current minute..."));
  rtc.setSecond(getSerialNumber("Enter the current second...")+2);
}
void dateSet() {
  // one-by-one prompts user for each aspect of the date
  rtc.setYear(getSerialNumber("Enter the current year (last two digits)..."));
  rtc.setMonth(getSerialNumber("Enter the current month..."));
  rtc.setDate(getSerialNumber("Enter the current day..."));
}

// code used when we need to receieve an integer over the serial data
int getSerialNumber(String prompt) {
  Serial.println(prompt); // prints prompt to the console for what it needs
  while (Serial.available() == 0) {} // waits until serial receives some input data
  String serialRead = Serial.readString(); // reads the string over serial
  serialRead.trim(); // trims of extra spaces at start/end to prevent confusing errors
  int serialReadNum = serialRead.toInt(); // attempts to cast the string input to a byte, could fail/crash
  return serialReadNum; // sends back out the integer as the output
}
