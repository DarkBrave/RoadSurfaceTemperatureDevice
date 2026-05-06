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

void setup() {
  Serial.begin(9600); // starts computer connection to send debug/logs/commands
  Wire.begin(); // starts I2C bus for sensors/RTC

  //while (!Serial); // STOPS code until computer plugged in, enable for debugging but will prevent battery operation

  // test display with splash screen
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // sets up
  Serial.println("OLED intialized");
  display.display();
  delay(500);
  display.clearDisplay();
  display.display();
  // set font sizing info
  display.setTextSize(1);
  display.setTextColor(WHITE);
  

  // SD Card Setup:
  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {
    throwError("SD initialization failed.");
    while (true); // HALTS if SD card isn't inserted
  }
  Serial.println("SD card initialization done.");

  // Log to CSV
  if (!SD.exists(SD_FILENAME)) {
    writeSD("date,time,irChipTemp,irObjectTemp,ambientTemp,ambientHumidity,isData"); // creates headers for CSV data if the file is new
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
  Serial.println(sht4.readSerial(), HEX);
  sht4.setPrecision(SHT4X_HIGH_PRECISION); // sets precision to HIGH (also can be MED or LOW
  sht4.setHeater(SHT4X_NO_HEATER); // disables heater in sensor
}

void loop() {
  delay(1000); // short wait before gathering data

  // record IR data
  float irChipTemp = irSensor.getAmbientTempCelsius(); 
  float irObjectTemp = irSensor.getObjectTempCelsius();

  // record ambient data
  sensors_event_t humidity, temp; 
  sht4.getEvent(&humidity, &temp);
  float ambientTemp = temp.temperature;
  float ambientHumidity = humidity.relative_humidity;

  buttonState = digitalRead(buttonPin);
  if (buttonState == HIGH) {
    isData = false;
  } else {
    isData = true;
  }  

  getTime(); // gets time

  // create String for CSV row format/serial log
  //String csvRow = rtcYear + "-" + rtcMonth + "-" + rtcDay + "," + rtcHour + ":" + rtcMinute + ":" + rtcSecond + "," + irAmbientTemp + "," + irObjectTemp;
  String date = String(rtcYear) + "-" + String(rtcMonth) + "-" + String(rtcDay);
  String time = String(rtcHour) + ":" + String(rtcMinute) + ":" + String(rtcSecond);
  String csvRow = date + "," + time + "," + String(irChipTemp) + "," + String(irObjectTemp) + "," + String(ambientTemp) + "," + String(ambientHumidity) + "," + String(isData);
  writeSD(csvRow);


  // writes data to dsiplay
  display.clearDisplay();
  display.setCursor(0,0);
  oledLine = date + " | " + time;
  display.println(oledLine);
  oledLine = "Ob: " + String(irObjectTemp) + " | Ch: " + String(irChipTemp);
  display.println(oledLine);
  oledLine = "Am: " + String(ambientTemp) + " | Hu: " + String(ambientHumidity);
  display.println(oledLine);
  oledLine = "Status: " + String(isData);
  display.println(oledLine);
  display.display();

  if(Serial.available() != 0) {
    String serialRead = Serial.readString();
    serialRead.trim();

    if (serialRead == "timeset") {
      timeSet();
    } else if (serialRead == "dateset") {
      dateSet();
    } else if (serialRead == "cleardisplay") {
      display.clearDisplay();
      display.display();
      Serial.println("Display cleared");
    } else {
      String errorMessage = "Invalid serial data received: " + serialRead;
      throwError(errorMessage);
    }
  }
}

void getTime() {
  // record date/time from RTC
  rtcYear = rtc.getYear();
  rtcMonth = rtc.getMonth(rtcCentury);
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