#include <Bridge.h>
#include <BridgeClient.h>
#include <BridgeServer.h>
#include <BridgeUdp.h>
#include <Console.h>
#include <FileIO.h>
#include <dht11.h>
#include <stdio.h>

dht11 DHT;
#define DHT11_PIN 2
#define SEN114_PIN A1

#define DRY 605
#define WET 740

#define CONTROL_FILE "/mnt/sd/yunbot/data/yunbot.control.dat"
#define DAT_FILE "/mnt/sd/yunbot/data/yunbot.dat"
#define LOG_FILE "/mnt/sd/yunbot/data/output.log"

#define DATA_CAPTURE_DELAY 10000
#define WATER_TIME_LIMIT 20000
#define WATER_LIMIT_WAIT 30000

int motorPin = A0;
int blinkPin = 13;

// Dry = 605 and lower
// Moist =
// Wet = 740

boolean serialEnabled = false;
boolean waterPumpRequest = false;
boolean waterPumpLimitReached = false;
long lastWaterPumpTime = -1;
long lastWaterLimitReachTime = -1;

void setup()
{
  Bridge.begin();
  FileSystem.begin();

  if (serialEnabled) {
    Serial.begin(9600);
    while(!Serial);  
  }

  logMessage("YUNBOT started.\n");

  pinMode(motorPin, OUTPUT);
  pinMode(blinkPin, OUTPUT);
}

String getTimeStamp() {
  String result;
  Process time;
  time.begin("date");
  time.addParameter("+%D-%T");  
  time.run(); 

  while(time.available()>0) {
    char c = time.read();
    if(c != '\n')
      result += c;
  }

  return result;
}

void writeData(String dataString) {
  
  File dataFile = FileSystem.open(DAT_FILE, FILE_APPEND);

  if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();
  }
}

void logMessage(String dataString) {

  if (serialEnabled) {
    Serial.println(dataString);
  }
  
  File dataFile = FileSystem.open(LOG_FILE, FILE_APPEND);

  if (dataFile) {
      dataFile.println("[" + getTimeStamp() + "]: " + dataString);
      dataFile.close();
  }
}

void readControlData() {

  File dataFile = FileSystem.open(CONTROL_FILE, FILE_READ);

  if (dataFile) {
      char c;
      char data[10];

      //w:0.
      int i = 0;
      while ((dataFile.available() > 0) && (i < 10)) {
          data[i] = dataFile.read();          
          i++;
      }

      boolean waterControl = false;

      char *t, *saveptr;
      String msg;
      // First strtok iteration
      t = strtok_r(data,":",&saveptr);

      if (*t == 'w') {
        waterControl = true;
        logMessage("Water control read.");
      }           
      
      // Second strtok iteration
      t = strtok_r(NULL,".",&saveptr);
      if (*t == '1') {
        if (!waterPumpLimitReached) {
          waterPumpRequest = true;
          logMessage("Requesting water pump on.");
        } else {
          if (millis() - lastWaterLimitReachTime > WATER_LIMIT_WAIT) {
            // Clear limit after wait time
            lastWaterLimitReachTime = -1;
            waterPumpLimitReached = false;
            logMessage("Clearing water limit - wait time reached.");
          } else {
            logMessage("Request for water pump, but water limit still in place.");
          }
        }
      } else {
        waterPumpRequest = false;
      }
      
      dataFile.close();
  }
}

void logTimeMessage(String message, long ms){    
   char str[120];
   char msgbuf[120];
   message.toCharArray(msgbuf, 120);
   sprintf(str, msgbuf, (ms/1000));
   logMessage(str);
}

void loop()
{
  logMessage("Reading sensor data.");

  String dataString;
  dataString += getTimeStamp();
  dataString += ",";  
  int moisture = analogRead(SEN114_PIN);
  
  int chk;
  chk = DHT.read(DHT11_PIN);
  dataString += String(chk);
  dataString += ",";
  dataString += String(DHT.humidity);
  dataString += ",";
  dataString += String(DHT.temperature);
  dataString += ",";
  dataString += String(moisture);

  logMessage("Storing sensor data.");
  writeData(dataString);

   char str[120];
   sprintf(str, "Waiting %d seconds...", (DATA_CAPTURE_DELAY/1000));
   logMessage(str);
   delay(DATA_CAPTURE_DELAY);

   logMessage("Reading control data.");
   readControlData();

   if (waterPumpRequest) {
    logMessage("Water pump on.");
     digitalWrite(motorPin, HIGH);     
     if (lastWaterPumpTime < 0) {
      lastWaterPumpTime = millis();
     }
   } else {
    logMessage("Water pump off.");
     digitalWrite(motorPin, LOW);
     lastWaterPumpTime = -1;
   }

   if (lastWaterPumpTime > 0) {    
    long elapsedWatering = millis()-lastWaterPumpTime;
    logTimeMessage("Elapsed watering: %d seconds", elapsedWatering);
  
     // Stop the watering once the time limit is reached
     if (elapsedWatering > WATER_TIME_LIMIT) {
       waterPumpRequest = false;
       waterPumpLimitReached = true;
       lastWaterLimitReachTime = millis();
       logMessage("Exceeded watering time limit, shutting down pump.");
     }
   }
}
