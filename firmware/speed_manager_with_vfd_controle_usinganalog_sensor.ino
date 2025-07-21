#include <Wire.h>
#include <EEPROM.h>

// === Pin Configuration ===
const int vfdPwmPin = 6;
const int pwmPin1 = 9;
const int pwmPin2 = 10;
const int analogPin1 = A0;
const int analogPin2 = A1;

// === BPM Configuration ===
int bagBPM = 30;
const int bpmMin = 20;
const int bpmMax = 72;

// === BPM → Fixed PWM Mapping ===
const int bpmToPwm[] = {
  24, 25, 26, 27, 28, 31, 32, 34, 35, 37, 39,
  40, 42, 43, 44, 46, 47, 49, 50, 52, 54,
  55, 57, 58, 59, 61, 63, 64, 66, 67, 69,
  70, 71, 73, 74, 76, 78, 79, 80, 82, 84,
  85, 87, 88, 90, 92, 93, 94, 95, 97, 99,
  100, 102, 103
};

// === Voltage & PWM Settings ===
const float minVoltage = 2.2;
const float maxVoltage = 4.0;
const float referenceVoltage = 5.0;
const int adcMax = 1023;

const int minPWM = 10;
const int maxPWM1 = 125;
const int maxPWM2 = 135;

// === Smoothing & Timing ===
const float smoothingFactor = 0.6;
const float emergencyStopMargin = 0.1;
const int updateInterval = 10;

// === Voltage Trend Filter ===
bool useVoltageTrend = false;
#define HISTORY_SIZE 4
float voltage1History[HISTORY_SIZE] = {0};
float voltage2History[HISTORY_SIZE] = {0};
int historyIndex = 0;
const int stableTrendCount = 2;
const float noiseThreshold = 0.005;

// === State Variables ===
int controlState = 0;
int lastControlState = -1;
int currentPWM1 = 0;
int currentPWM2 = 0;
float filteredVoltage1 = 0.0;
float filteredVoltage2 = 0.0;
unsigned long lastUpdate = 0;

bool debug = false;

void setup() {
  if (debug) Serial.begin(9600);

  Wire.begin(8); // I2C slave address
  Wire.onReceive(receiveEvent);

  pinMode(pwmPin1, OUTPUT);
  pinMode(pwmPin2, OUTPUT);
  pinMode(vfdPwmPin, OUTPUT);

  controlState = EEPROM.read(0);
  bagBPM = constrain(EEPROM.read(1), bpmMin, bpmMax);

  controlState == 1 ? runMotors() : stopMotors();

  if (debug) {
    Serial.print("🔄 EEPROM State: "); Serial.println(controlState);
    Serial.print("📦 EEPROM BPM: "); Serial.println(bagBPM);
  }
}

void loop() {
  if (millis() - lastUpdate >= updateInterval) {
    lastUpdate = millis();
    controlState == 1 ? updatePWM() : stopMotors();
  }
}

void receiveEvent(int howMany) {
  if (howMany < 2) return;

  int cmd = Wire.read();
  int value = Wire.read();

  if (cmd == 0x01) {
    controlState = (value == 1) ? 1 : 0;
    EEPROM.write(0, controlState);
    controlState == 1 ? runMotors() : stopMotors();
  } else if (cmd == 0x02) {
    int newBPM = constrain(value, bpmMin, bpmMax);
    if (bagBPM != newBPM) {
      bagBPM = newBPM;
      EEPROM.write(1, bagBPM);
    }
  }

  if (debug) {
    Serial.print("📥 I2C CMD: "); Serial.print(cmd);
    Serial.print(" | Value: "); Serial.println(value);
  }
}

void runMotors() {
  if (lastControlState != 1) {
    if (debug) Serial.println("🟢 Motors STARTED");
    lastControlState = 1;
  }
}

void stopMotors() {
  if (lastControlState != 0) {
    if (debug) Serial.println("🔴 Motors STOPPED");
    lastControlState = 0;
  }

  digitalWrite(pwmPin1, LOW);
  digitalWrite(pwmPin2, LOW);
  digitalWrite(vfdPwmPin, LOW);

  currentPWM1 = 0;
  currentPWM2 = 0;
}

void updatePWM() {
  float raw1 = analogRead(analogPin1) * referenceVoltage / adcMax;
  float raw2 = analogRead(analogPin2) * referenceVoltage / adcMax;

  filteredVoltage1 = smoothingFactor * raw1 + (1.0 - smoothingFactor) * filteredVoltage1;
  filteredVoltage2 = smoothingFactor * raw2 + (1.0 - smoothingFactor) * filteredVoltage2;

  voltage1History[historyIndex] = filteredVoltage1;
  voltage2History[historyIndex] = filteredVoltage2;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;

  bool stable1 = useVoltageTrend ? isVoltageStable(voltage1History) : true;
  bool stable2 = useVoltageTrend ? isVoltageStable(voltage2History) : true;

  int targetPWM1 = computePWM(filteredVoltage1, stable1, maxPWM1);
  int targetPWM2 = computePWM(filteredVoltage2, stable2, maxPWM2);

  applyPWM(pwmPin1, currentPWM1, targetPWM1);
  applyPWM(pwmPin2, currentPWM2, targetPWM2);

  int vfdPWM = getFixedPwmFromBPM(bagBPM);
  analogWrite(vfdPwmPin, vfdPWM);

  if (debug) {
    Serial.print("PWM1: "); Serial.print(currentPWM1);
    Serial.print(" | PWM2: "); Serial.print(currentPWM2);
    Serial.print(" | V1: "); Serial.print(filteredVoltage1, 3);
    Serial.print(" | V2: "); Serial.print(filteredVoltage2, 3);
    Serial.print(" | BPM: "); Serial.print(bagBPM);
    Serial.print(" | VFD PWM: "); Serial.println(vfdPWM);
  }
}

void applyPWM(int pin, int &current, int target) {
  if (target == 0) {
    digitalWrite(pin, LOW);
    current = 0;
    if (debug) {
      Serial.print("🛑 PWM to 0 → Pin ");
      Serial.println(pin);
    }
  } else {
    current = adaptiveRamp(current, target);
    analogWrite(pin, current);
  }
}

int computePWM(float voltage, bool isStable, int maxPWM) {
  if (!isStable || voltage < (minVoltage - emergencyStopMargin) || voltage > (maxVoltage + emergencyStopMargin)) {
    if (debug) Serial.println("⚠️ Voltage unstable or out of range");
    return 0;
  }

  float x = constrain((voltage - minVoltage) / (maxVoltage - minVoltage), 0.0, 1.0);
  float curve = pow(1 - x, 3);
  return constrain(minPWM + (int)((maxPWM - minPWM) * curve), minPWM, maxPWM);
}

int adaptiveRamp(int current, int target) {
  if (target == 0) return 0;
  int diff = abs(target - current);
  int step = constrain(diff / 6 + 1, 4, 10);
  if (current < target) current = min(current + step, target);
  else current = max(current - step, target);
  return current;
}

bool isVoltageStable(float history[]) {
  int up = 0, down = 0;
  for (int i = 1; i < HISTORY_SIZE; i++) {
    float diff = history[i] - history[i - 1];
    if (abs(diff) < noiseThreshold) continue;
    if (diff > 0) up++;
    else down++;
  }
  return (up >= stableTrendCount || down >= stableTrendCount);
}

int getFixedPwmFromBPM(int bpm) {
  bpm = constrain(bpm, bpmMin, bpmMax);
  return bpmToPwm[bpm - bpmMin];
}
