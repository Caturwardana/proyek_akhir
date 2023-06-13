//Catur Wardana Data logging

//Library untuk OLED display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//Library untuk RTC dan definisi pin
#include <virtuabotixRTC.h>
virtuabotixRTC myRTC(15, 2, 4); //CLK, DAT, RST

//definisi LED
const int led1 = 26; // Pin untuk LED 1 biru (logged)
const int led2 = 27; // Pin untuk LED 2 green (received)
const int led3 = 14; // Pin untuk LED 3 red (error not logged / recived)

//Sdcard library dan definisi string
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#define SD_CS 5
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK 18
String dataMessage;
String dataMPPT = "";
String fileName = "";
String FileCreated = "";
unsigned int SampleNumber = 0; // Sample number logging
float Fpri;
float Turbinrpm;
float Fpri1, Vpri1, Ipri1, Puis1, Step1, Pwm_gate1, Vsor1, Ibat1;
float sampleVpri1, sampleIpri1, samplePuis1, sampleVsor1, sampleIbat1;
unsigned int ratarata;

// Constants definitions anemometer sensor dan definisi pin
#define windPin 13 // Receive the data from anemometer sensor
const float pi = 3.14159265; // pi number
int period = 5000; // Measurement period (miliseconds)
int delaytime = 10000; // Time between samples (miliseconds)
int radio = 100; // Distance from center windmill to outer cup (mm)
int jml_celah = 12; // jumlah celah sensor

// Variable definitions anemometer sensor
unsigned int Sample = 0; // Sample number
unsigned int counter = 0; // B/W counter for sensor
unsigned int RPM = 0; // Revolutions per minute
float speedwind = 0; // Wind speed (m/s)

// Constants definitions RPM GENERATOR
#define GeneratorRpm 12 // Receive the data from rpm gen sensor
unsigned int counterGen = 0; // B/W counter for sensor
unsigned int RPMGen = 0; // Revolutions per minute

void setup() {
  Serial.begin(115200);

  //Sett time RTC (sekali jika waktu sudah bergeser)
  //myRTC.setDS1302Time(0, 50, 16, 7, 13, 5, 2023); //(sec, min, hr, dow, date, mnth, yr) set date time rtc reupload without slash if date & time sett

  //inisialisasi LED
  pinMode(led1, OUTPUT); // Mengatur pin LED 1 sebagai output
  pinMode(led2, OUTPUT); // Mengatur pin LED 2 sebagai output
  pinMode(led3, OUTPUT); // Mengatur pin LED 3 sebagai output
  digitalWrite(led1, LOW); // Mematikan LED 1
  digitalWrite(led2, LOW); // Mematikan LED 2
  digitalWrite(led3, LOW); // Mematikan LED 3

  //inisialisasi OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Get current date and time untuk menamai file logger
  myRTC.updateTime();
  // Create file name with date and time
  fileName = "/data_" + String(myRTC.year) + String(myRTC.month) + String(myRTC.dayofmonth) + "_" + String(myRTC.hours) +
             String(myRTC.minutes) + String(myRTC.seconds) + ".txt";

  // Set the pins anemometer
  pinMode(windPin, INPUT);
  digitalWrite(windPin, HIGH);

  // Initialize SD card
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SD.begin(SD_CS);

  if (!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    display.setCursor(0, 0);
    display.println("Card Mount Failed");
    display.display();
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    display.setCursor(0, 0);
    display.println("No SD card attached");
    display.display();
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialization failed!");
    display.setCursor(0, 0);
    display.println("SD card init failed!");
    display.display();
    return;
  }

  // Create file with header
  File file = SD.open(fileName.c_str());
  if (!file) {
    Serial.println("Creating file...");
    writeFile(SD, fileName.c_str(), "No,Date,Time,Windspeed_ms,Windspeed_kmh,Turbin_rpm_RPMGen,RPMGen,Turbin_rpm_from_Fpri,Fpri,Vpri,Ipri,Puiss_AVGard,Puiss_AVGesp,Puiss_AVGcom,Step,pwm_gate,Vsor,Ibat \r\n");
    display.setCursor(0, 0);
    FileCreated = "file created " + String(myRTC.hours) + ":" + String(myRTC.minutes) + ":" + String(myRTC.seconds);
    display.println(FileCreated);
    display.display();
  }
  else {
    Serial.println("File already exists");
  }
  file.close();
}

void loop() {
  digitalWrite(led2, LOW); // LED GREEN LOW (kedip)
  Sample++;
  Serial.print(Sample);
  Serial.print(": Start measurement…");
  windvelocity();
  Serial.println(" finished.");
  Serial.print("Counter: ");
  Serial.print(counter);

  RPMcalc();
  RPMGenCalculate();
  Serial.print("; Gen RPM: ");
  Serial.print(RPMGen);
  Serial.print(" RPM ");
  Serial.print("; RPM ANEMO: ");
  Serial.print(RPM);
  Serial.print("; Wind speed: ");
  WindSpeed();
  Serial.print(speedwind * 3.6);
  Serial.print(" (km/h)  ");
  Serial.print(speedwind);
  Serial.println(" (m/sec)");
  rtcupdate();
  logSDCard();

  display.setCursor(0, 0);
  display.println(FileCreated);
  display.println(String(speedwind) + "ms" + String(speedwind * 3.6) + "kmh" + String(RPMGen) + "Grpm" + String(RPMGen / 7) + "Trpm");
  display.display();
  display.clearDisplay();
  dataMPPT = "";
  ratarata = 0;
  sampleVpri1 = 0;
  sampleIpri1 = 0;
  samplePuis1 = 0;
  sampleVsor1 = 0;
  sampleIbat1 = 0;
}


// Measure wind speed
void windvelocity()
{
  speedwind = 0;
  counter = 0;
  counterGen = 0;
  attachInterrupt(digitalPinToInterrupt(windPin), addcount, RISING);
  attachInterrupt(digitalPinToInterrupt(GeneratorRpm), count, RISING);
  unsigned long millis();
  long startTime = millis();
  while (millis() < startTime + period) {
    readserial();
  }

  detachInterrupt(1);
}

void RPMcalc() {
  RPM = ((counter / jml_celah) * 60) / (period / 1000); // Calculate revolutions per minute (RPM)
}

void RPMGenCalculate() {
  RPMGen = ((counterGen / 16) * 60) / (period / 1000); // Calculate revolutions per minute (RPM)
}

void WindSpeed() {
  speedwind = ((2 * pi * radio * RPM) / 60) / 1000; // Calculate wind speed on m/s
}

void addcount() {
  counter++;
}
void count() {
  counterGen++;
}

void rtcupdate() {
  myRTC.updateTime();
}

void readserial() {
  String dataArray = "";

  while (Serial.available() > 0) {
    char data = Serial.read();

    // jika karakter yang diterima adalah kurung siku pertama, maka mulai simpan data array
    if (data == '[') {
      dataArray += data;
    }

    // simpan karakter ke variabel dataArray jika sedang menyimpan data array
    if (dataArray.length() > 0) {
      dataArray += data;
      delay(1);
    }

    // jika karakter yang diterima adalah kurung siku terakhir, maka keluar dari loop
    if (data == ']') {
      break;
    }
  }

  // keluar dari fungsi jika tidak berhasil menangkap data array
  if (dataArray.length() == 0 || dataArray.indexOf(',') == -1) {
    return;
  }

  // hapus semua karakter "[" dan "]" dari dataArray
  dataArray.replace("[", "");
  dataArray.replace("]", "");

  int startIndex = 0;
  int endIndex = dataArray.indexOf(",");

  // Membaca dan mengkonversi token pertama menjadi float
  Fpri1 = dataArray.substring(startIndex, endIndex).toFloat();

  // Melanjutkan pemecahan data untuk token-token selanjutnya
  startIndex = endIndex + 1;
  endIndex = dataArray.indexOf(",", startIndex);

  Vpri1 = dataArray.substring(startIndex, endIndex).toFloat();

  startIndex = endIndex + 1;
  endIndex = dataArray.indexOf(",", startIndex);

  Ipri1 = dataArray.substring(startIndex, endIndex).toFloat();

  startIndex = endIndex + 1;
  endIndex = dataArray.indexOf(",", startIndex);

  Puis1 = dataArray.substring(startIndex, endIndex).toFloat();

  startIndex = endIndex + 1;
  endIndex = dataArray.indexOf(",", startIndex);

  Step1 = dataArray.substring(startIndex, endIndex).toFloat();

  startIndex = endIndex + 1;
  endIndex = dataArray.indexOf(",", startIndex);

  Pwm_gate1 = dataArray.substring(startIndex, endIndex).toFloat();

  startIndex = endIndex + 1;
  endIndex = dataArray.indexOf(",", startIndex);

  Vsor1 = dataArray.substring(startIndex, endIndex).toFloat();

  startIndex = endIndex + 1;
  endIndex = dataArray.length();

  Ibat1 = dataArray.substring(startIndex, endIndex).toFloat();

  sampleVpri1 = sampleVpri1 + Vpri1;
  sampleIpri1 = sampleIpri1 + Ipri1;
  samplePuis1 = samplePuis1 + Puis1;
  sampleVsor1 = sampleVsor1 + Vsor1;
  sampleIbat1 = sampleIbat1 + Ibat1;

  ratarata++;
}

// Write the sensor readings on the SD card
void logSDCard() {
  SampleNumber++;
  dataMPPT = String(Fpri1) + "," + String(sampleVpri1 / ratarata) + "," + String(sampleIpri1 / ratarata) + "," +
             String(samplePuis1 / ratarata) + "," + String((sampleVpri1 / ratarata) * (sampleIpri1 / ratarata)) + "," + String("") + "," + String(Step1) + "," + String(Pwm_gate1) + "," + String(sampleVsor1 / ratarata) + "," +
             String(sampleIbat1 / ratarata);
             
  Serial.println("==========");
  Serial.println(ratarata);
  Serial.println(dataMPPT);
  Serial.println("==========");

  if (dataMPPT.length() == 0 || dataMPPT.indexOf(',') == -1) {
    Serial.println("Write failed no data MPPT received or invalid data format");
    digitalWrite(led3, HIGH); // LED RED HIGH
    digitalWrite(led2, LOW); // LED GREEN LOW (not received)
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(FileCreated);
    display.println(String(speedwind) + "ms" + String(speedwind * 3.6) + "kmh" + String(Fpri) + "rpm");
    display.println("No data MPPT/Invalid");
    display.display();
    loop();
  }

  digitalWrite(led3, LOW); // LED RED LOW
  digitalWrite(led2, HIGH); // LED GREEN HIGH (received)

  int posKoma = dataMPPT.indexOf(','); // mencari posisi koma pertama dalam string
  Fpri = dataMPPT.substring(0, posKoma).toFloat(); // mengambil substring sebelum koma dan mengonversi menjadi tipe data float
  Turbinrpm = Fpri * 7 ; //rpm Generator, angka dibelakang * kalikan dengan perbandingan roda puli untuk mendapatkan rpm rotor

  dataMessage = String(SampleNumber) + "," + String(myRTC.dayofmonth) + "/" + String(myRTC.month) + "/" + String(myRTC.year) + "," +
                String(myRTC.hours) + ":" + String(myRTC.minutes) + ":" + String(myRTC.seconds) + "," +
                String(speedwind) + "," +  String(speedwind * 3.6) + "," +  String(RPMGen / 7) + "," +  String(RPMGen) + "," +  String(Turbinrpm) + "," +
                String(dataMPPT) + "\r\n";

  Serial.print("Save data: ");
  Serial.println(dataMessage);
  display.print(dataMessage);
  appendFile(SD, fileName.c_str(), dataMessage.c_str());
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS & fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    digitalWrite(led3, HIGH); // LED RED HIGH
    digitalWrite(led1, LOW); // LED BIRU LOW
    display.println("Fail open file");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
    digitalWrite(led3, LOW); // LED RED LOW
    digitalWrite(led1, HIGH); // LED BIRU HIGH
    display.println("File written");
  } else {
    Serial.println("Write failed");
    digitalWrite(led3, HIGH); // LED RED HIGH
    digitalWrite(led1, LOW); // LED BIRU LOW
    display.println("Write failed");
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS & fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    digitalWrite(led3, HIGH); // LED RED HIGH
    digitalWrite(led1, LOW); // LED BIRU LOW
    display.println("Fail open file");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
    digitalWrite(led3, LOW); // LED RED LOW
    digitalWrite(led1, HIGH); // LED BIRU HIGH
    display.println("Message appended");
  } else {
    Serial.println("Append failed");
    digitalWrite(led3, HIGH); // LED RED HIGH
    digitalWrite(led1, LOW); // LED BIRU LOW
    display.println("Fail append file");
  }
  file.close();
}
