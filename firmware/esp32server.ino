#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>  // Persistent storage
#include <Wire.h>

const char* ssid = "esp32network";
const char* password = "hopeandtechology";

WebServer server(80);
Preferences prefs;

int machineState = 0;  // Default to stopped

// Function to handle /baglength?data=...
void HandleBaglength() {
  String data = server.arg("data");
  Serial.println("📨 Bag Length received: " + data);

  prefs.begin("app", false);
  prefs.putString("baglength", data);
  prefs.end();

  Serial2.println("BAG:" + data);
  Serial.println("📤 Serial2 → Sent bag length: BAG:" + data);

  server.send(200, "text/plain", "✅ Bag length saved");
}

void HandleMachineSpeed() {
  String data = server.arg("data");
  Serial.println("📨 Bag BPM received: " + data);

  int bpmValue = data.toInt();
  if (bpmValue < 20 || bpmValue > 72) {
    server.send(400, "text/plain", "❌ BPM out of range (20–72)");
    return;
  }

  // Save BPM to flash
  prefs.begin("app", false);
  prefs.putInt("bpm", bpmValue);   // Store as integer
  prefs.end();

  // Send structured data to Arduino: [commandType, bpm]
  Wire.beginTransmission(8);       // Address of Arduino
  Wire.write(0x02);                // Command ID for BPM update
  Wire.write(bpmValue);            // Actual BPM value
  byte status = Wire.endTransmission();

  if (status == 0) {
    Serial.println("📤 Sent to I2C: [0x02, " + String(bpmValue) + "]");
    server.send(200, "text/plain", "✅ Bag BPM saved");
  } else {
    Serial.println("❌ I2C Transmission Failed");
    server.send(500, "text/plain", "❌ I2C Error");
  }
}


void HandleGetBPM() {
  prefs.begin("app", true);  // read-only mode
  int bpmValue = prefs.getInt("bpm", -1);  // default -1 if not set
  prefs.end();

  if (bpmValue >= 20 && bpmValue <= 72) {
    server.send(200, "text/plain", String(bpmValue));
  } else {
    server.send(200, "text/plain", "na");  // invalid or not set
  }
}



// Function to handle /machinestart
void HandleMachineStart() {
  Serial.println("🟢 Machine Start Triggered");

  prefs.begin("app", false);
  prefs.putInt("machine_state", 1);  // Save 1 permanently
  prefs.end();

  machineState = 1;

  Wire.beginTransmission(8);
   Wire.write(0x01); 
  Wire.write(machineState);
  byte status = Wire.endTransmission();
  Serial.println("📤 I2C → Sent machine state: 1");
  Serial.println("📡 I2C status: " + String(status));

  Serial2.println("1");
  Serial.println("📤 Serial2 → Sent machine start signal: 1");

  server.send(200, "text/plain", "🟢 Machine Started");
}

// Function to handle /machinestop
void HandleMachineStop() {
  Serial.println("🔴 Machine Stop Triggered");

  prefs.begin("app", false);
  prefs.putInt("machine_state", 0);
  prefs.end();

  machineState = 0;

  Wire.beginTransmission(8);
   Wire.write(0x01); 
  Wire.write(machineState);
  byte status = Wire.endTransmission();
  Serial.println("📤 I2C → Sent machine state: 0");
  Serial.println("📡 I2C status: " + String(status));

  Serial2.println("0");
  Serial.println("📤 Serial2 → Sent machine stop signal: 0");

  server.send(200, "text/plain", "🔴 Machine Stopped");
}

// Your main machine logic
void runMachineLogic() {
  Serial.println("⚙️ Machine is running...");
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  delay(1000);

  pinMode(21, INPUT_PULLUP);
  pinMode(22, INPUT_PULLUP);
  Wire.begin(21, 22);

  if (WiFi.softAP(ssid, password)) {
    Serial.println("✅ Wi-Fi Access Point Started");
    Serial.print("📶 SSID: ");
    Serial.println(ssid);
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("❌ Failed to start Access Point");
  }

  // Load persisted state
  prefs.begin("app", true);
  machineState = prefs.getInt("machine_state", 0);
  String lastLength = prefs.getString("baglength", "N/A");
  String lastBpm = prefs.getString("bpm", "N/A");
  prefs.end();

  Serial.print("📦 Last Bag Length: ");
  Serial.println(lastLength);
  Serial.print("⚙️ Last Bpm(bag per minuit): ");
  Serial.println(lastBpm);
  Serial.print("🔄 Last Machine State: ");
  Serial.println(machineState);

  // Register routes
  server.on("/baglength", HandleBaglength);
  server.on("/bpm", HandleMachineSpeed);
  server.on("/machinestart", HandleMachineStart);
  server.on("/machinestop", HandleMachineStop);
  server.on("/getbpm", HandleGetBPM);

  server.on("/getbaglength", []() {
    prefs.begin("app", true);
    String bagLength = prefs.getString("baglength", "N/A");
    prefs.end();

    Serial2.println("BAG:" + bagLength);
    Serial.println("📤 Serial2 → Sent get bag length: BAG:" + bagLength);

    server.send(200, "text/plain", bagLength);
  });



 



  server.on("/getmachinestate", []() {
    prefs.begin("app", true);
    int state = prefs.getInt("machine_state", 0);
    prefs.end();

    server.send(200, "text/plain", state == 1 ? "running" : "stopped");
  });

  server.begin();
  Serial.println("🚀 Web server started at port 80");
}

void loop() {
  server.handleClient();
  if (machineState == 1) {
    runMachineLogic();
  }
}
