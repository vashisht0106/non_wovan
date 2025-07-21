#include <driver/gpio.h>
#include "esp_timer.h"
#include <Preferences.h>

#define PULSE_PIN       18
#define DIR1_PIN        19
#define DIR2_PIN        21
#define START_SENSOR    35
#define STOP_SENSOR     34

const float steps_per_cm = 80.0;
const uint64_t step_interval_us = 200;

float bag_length_cm = 30.0;
int total_steps = 0;

volatile int step_count = 0;
volatile bool motor_running = false;
esp_timer_handle_t pulse_timer;

uint64_t start_time = 0, stop_time = 0;
bool completed = false;

Preferences prefs;

// ✅ Continuous stepping logic
void IRAM_ATTR pulseStep(void* arg) {
  if (!motor_running) return;

  gpio_set_level((gpio_num_t)PULSE_PIN, 1);
  ets_delay_us(20);
  gpio_set_level((gpio_num_t)PULSE_PIN, 0);

  step_count++;
  if (step_count >= total_steps) {
    motor_running = false;
    step_count = 0;  // Reset for continuous operation
    stop_time = esp_timer_get_time();
    completed = true;
  }
}

void startMotor() {
  if (!motor_running) {
    step_count = 0;
    motor_running = true;
    start_time = esp_timer_get_time();
    esp_timer_start_periodic(pulse_timer, step_interval_us);
    Serial.println("▶️ Stepper started by command...");
  }
}

void stopMotor() {
  if (motor_running) {
    motor_running = false;
    esp_timer_stop(pulse_timer);
    Serial.println("⏹️ Stepper stopped by command.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17
  delay(1000);

  pinMode(PULSE_PIN, OUTPUT);
  pinMode(DIR1_PIN, OUTPUT);
  pinMode(DIR2_PIN, OUTPUT);
  pinMode(START_SENSOR, INPUT);
  pinMode(STOP_SENSOR, INPUT);

  digitalWrite(DIR1_PIN, LOW);
  digitalWrite(DIR2_PIN, LOW);

  const esp_timer_create_args_t timer_args = {
    .callback = &pulseStep,
    .name = "stepper_timer"
  };
  esp_timer_create(&timer_args, &pulse_timer);

  // Load stored bag length
  prefs.begin("bag", true);
  bag_length_cm = prefs.getFloat("length", 30.0);
  prefs.end();

  total_steps = bag_length_cm * steps_per_cm;

  Serial.printf("🟢 System ready. Bag Length: %.2f cm → Steps: %d\n", bag_length_cm, total_steps);
  Serial.println("💬 Send `1` to START, `0` to STOP, `BAG:<length>` to update.");
}

void loop() {
  // 🔁 Read serial commands from ESP32-A
  if (Serial2.available()) {
    String input = Serial2.readStringUntil('\n');
    input.trim();
  Serial.println("📥 Incoming Serial2: " + input);
    if (input == "1") {
      startMotor();
    } else if (input == "0") {
      stopMotor();
    } else if (input.startsWith("BAG:")) {
      String val = input.substring(4);
      float newLen = val.toFloat();
      if (newLen > 15 && newLen < 35) {
        bag_length_cm = newLen;
        total_steps = bag_length_cm * steps_per_cm;

        prefs.begin("bag", false);
        prefs.putFloat("length", bag_length_cm);
        prefs.end();

        Serial.printf("✅ Bag length set: %.2f cm → Steps: %d\n", bag_length_cm, total_steps);
      } else {
        Serial.println("❌ Invalid bag length: " + input);
      }
    }
  }

  // 🔁 Optional sensor start
  bool start_trigger = digitalRead(START_SENSOR);
  bool stop_trigger = digitalRead(STOP_SENSOR);
   if (start_trigger == LOW && stop_trigger == HIGH && !motor_running) {
    step_count = 0;
    motor_running = true;
    esp_timer_start_periodic(pulse_timer, step_interval_us);
    Serial.println("▶️ Motor started. Stepper running...");
  }
    if (stop_trigger == LOW && motor_running) {
    motor_running = false;
    esp_timer_stop(pulse_timer);
    Serial.printf("⛔️ Stop sensor = LOW: Motor stopped at %d steps\n", step_count);
  }

  // 🔁 Report each bag cycle complete
  if (completed) {
    completed = false;
    uint64_t elapsed_time = stop_time - start_time;
    Serial.printf("✅ Completed %d steps in %.2f ms for Bag Length: %.2f cm\n",
                  total_steps, elapsed_time / 1000.0, bag_length_cm);
    start_time = esp_timer_get_time();  // reset for next cycle
  }

  delay(1);
}
