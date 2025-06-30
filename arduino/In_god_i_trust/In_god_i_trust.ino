#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <HardwareSerial.h>

// Your Wi-Fi credentials
const char* ssid = "Free";
const char* password = "password";

// Create server on port 80
WebServer server(80);

// Create Serial for GSM module (e.g., SIM800L or A6)
HardwareSerial GSM(1);

// GSM TX to ESP32 GPIO16 (RX), GSM RX to ESP32 GPIO17 (TX)
#define GSM_RX 16
#define GSM_TX 17

String incomingData = "";
bool newSMS = false;
String senderNumber = "";

void setup() {
  Serial.begin(115200);
  GSM.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected.");
  Serial.print("📡 ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // Configure GSM to notify on new SMS
  GSM.println("AT+CMGF=1");      // Text mode
  delay(1000);
  GSM.println("AT+CNMI=2,1,0,0,0");  // Direct SMS to serial
  delay(1000);

  // Handle POST request to /send_sms
  server.on("/send_sms", HTTP_POST, []() {
    if (server.hasArg("plain") == false) {
      server.send(400, "text/plain", "Body not received");
      return;
    }

    String body = server.arg("plain");
    Serial.println("📥 Received JSON: " + body);

    // Parse JSON manually
    String phone = getValue(body, "phoneNumber");
    String message = getValue(body, "message");

    if (phone.length() > 0 && message.length() > 0) {
      sendSMS(phone, message);
      server.send(200, "application/json", "{\"status\":\"SMS sent\"}");
    } else {
      server.send(400, "application/json", "{\"error\":\"Invalid data\"}");
    }
  });

  server.begin();
  Serial.println("🌐 ESP32 Server started");
}

void loop() {
  server.handleClient();

  // Print IP address every 5 seconds
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 5000) {
    Serial.println("📡 ESP32 IP Address: " + WiFi.localIP().toString());
    lastPrint = millis();
  }

  // Check for incoming SMS
  while (GSM.available()) {
    char c = GSM.read();
    incomingData += c;
    if (incomingData.indexOf("+CMT:") != -1) {
      int start = incomingData.indexOf("\"") + 1;
      int end = incomingData.indexOf("\"", start);
      senderNumber = incomingData.substring(start, end);
      newSMS = true;
    }

    // End of message (assumes \r\n\r\n after SMS)
    if (incomingData.endsWith("\r\n\r\n")) {
      if (newSMS && senderNumber.length() > 0) {
        Serial.println("📨 New SMS received from: " + senderNumber);
        sendSMS(senderNumber, "hello from GSM");
        newSMS = false;
        senderNumber = "";
      }
      incomingData = "";  // Clear buffer
    }
  }
}

// Function to send SMS via GSM module
void sendSMS(String phoneNumber, String message) {
  Serial.println("📤 Sending SMS to " + phoneNumber);
  GSM.println("AT+CMGF=1");  // Set SMS to text mode
  delay(1000);
  GSM.print("AT+CMGS=\"");
  GSM.print(phoneNumber);
  GSM.println("\"");
  delay(1500);
  GSM.print(message);
  GSM.write(26);  // CTRL+Z to send
  delay(5000);
  Serial.println("✅ SMS Sent");
}

// Helper to extract value from JSON (naive parser)
String getValue(String data, String key) {
  int keyIndex = data.indexOf(key);
  if (keyIndex == -1) return "";

  int colonIndex = data.indexOf(":", keyIndex);
  int firstQuote = data.indexOf("\"", colonIndex + 1);
  int secondQuote = data.indexOf("\"", firstQuote + 1);

  if (firstQuote == -1 || secondQuote == -1) return "";

  return data.substring(firstQuote + 1, secondQuote);
}

