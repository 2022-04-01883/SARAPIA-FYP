#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

// Define ESP32 pins
#define RXD2 16  // ESP32 RX to SIM800 TX
#define TXD2 17  // ESP32 TX to SIM800 RX
#define GPS_RX 4 // GPS TX to ESP32 RX (you can change this pin)

HardwareSerial sim800(2); // SIM800 on Serial2
HardwareSerial gpsSerial(1); // GPS on Serial1
TinyGPSPlus gps;

bool awaitingSMS = false;
String senderNumber = "";

// Function declarations
void sendCommand(String cmd, int waitMs = 2000);
void sendSMS(String number, String message);

void setup() {
  Serial.begin(115200);
  sim800.begin(9600, SERIAL_8N1, RXD2, TXD2);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, -1); // Only RX for GPS

  delay(5000);
  Serial.println("Initializing SIM800C...");

  sendCommand("AT", 2000);
  sendCommand("AT+CMGF=1", 2000);          
  sendCommand("AT+CSCS=\"GSM\"", 2000);    
  sendCommand("AT+CNMI=2,2,0,0,0", 2000);  
}

void loop() {
  // Read GPS data
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (sim800.available()) {
    String response = sim800.readString();
    Serial.println(">> " + response);

    if (response.indexOf("+CMT:") != -1) {
      int start = response.indexOf("\"") + 1;
      int end = response.indexOf("\"", start);
      senderNumber = response.substring(start, end);
      awaitingSMS = true;
    }

    if (awaitingSMS && response.indexOf("\n", response.indexOf("+CMT:")) != -1) {
      delay(2000);

      String locationMessage = "Hello from GSM.\n";

      if (gps.location.isValid()) {
        locationMessage += "Lat: ";
        locationMessage += String(gps.location.lat(), 6);
        locationMessage += ", Lon: ";
        locationMessage += String(gps.location.lng(), 6);
      } else {
        locationMessage += "GPS not fixed yet.";
      }

      sendSMS(senderNumber, locationMessage);
      awaitingSMS = false;
    }
  }
}

void sendCommand(String cmd, int waitMs) {
  sim800.println(cmd);
  delay(waitMs);
  while (sim800.available()) {
    Serial.write(sim800.read());
  }
}

void sendSMS(String number, String message) {
  Serial.println("Sending SMS to: " + number);

  sendCommand("AT+CMGF=1", 2000);
  sendCommand("AT+CSCS=\"GSM\"", 2000);

  sim800.print("AT+CMGS=\"");
  sim800.print(number);
  sim800.println("\"");
  delay(3000);

  sim800.print(message);
  delay(1000);
  sim800.write(26); // Ctrl+Z
  delay(6000);

  Serial.println("SMS sent.");
}
