#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <Wire.h>
// ===== MPU6050 =====
#define MPU_ADDR       0x68
#define PWR_MGMT_1     0x6B
#define ACCEL_XOUT_H   0x3B

// ===== Pins =====
#define SDA_PIN 21
#define SCL_PIN 22
#define BTN_PIN 17

//void writeReg(uint8_t reg, uint8_t val);
//bool readBytes(uint8_t reg, uint8_t *buf, uint8_t len);
//static inline int16_t toInt16(uint8_t hi, uint8_t lo);
//int readButtonDebounced();

// ===================== Wi-Fi =====================
const char* WIFI_SSID = "LIRONA";
const char* WIFI_PASS = "ENTER PASSWORD HERE";

// ===================== AWS IoT Endpoint =====================
const char* AWS_ENDPOINT = "a1niwewq072dxv-ats.iot.us-east-1.amazonaws.com";
const int   AWS_PORT     = 8883;

// ===================== MQTT =====================
const char* MQTT_CLIENT_ID = "magic_wand";

static const char* TOPIC_RAW = "wand/raw";
static const char* TOPIC_EVT = "wand/event";
static const char* TOPIC_TLM = "wand/telemetry";

// ===================== Certificates =====================
const char* AWS_ROOT_CA = R"EOF(
-----ENTER CERTIFICATE-----
)EOF";

const char* DEVICE_CERT = R"EOF(
-----ENTER CERTIFICATE-----


)EOF";

const char* PRIVATE_KEY = R"EOF(
-----ENTER RSA PRIVATE KEY-----

)EOF";

// ===================== Clients =====================
WiFiClientSecure net;
PubSubClient mqtt(net);

// ===================== Utils =====================
static void connect_wifi() {
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
}

static void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Syncing time");
  time_t now = time(nullptr);

  while (now < 1700000000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }

  Serial.println();
  Serial.println("Time synced");

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.printf("UTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

static void setup_tls() {
  net.setCACert(AWS_ROOT_CA);
  net.setCertificate(DEVICE_CERT);
  net.setPrivateKey(PRIVATE_KEY);

  net.setTimeout(10); 

  Serial.println("TLS certificates loaded");
}

static void connect_mqtt() {
  mqtt.setServer(AWS_ENDPOINT, AWS_PORT);

  Serial.println("Testing TLS TCP connect...");
  if (!net.connect(AWS_ENDPOINT, AWS_PORT)) {
    Serial.println("TLS TCP connect failed (DNS/port/TLS/time/certs)");
  } else {
    Serial.println("TLS TCP connect OK");
    net.stop();
  }

  Serial.println("Connecting to MQTT...");
  while (!mqtt.connected()) {
    if (mqtt.connect(MQTT_CLIENT_ID)) {
      Serial.println("MQTT connected");
    } else {
      Serial.print("MQTT connect failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" retrying in 2s");
      delay(2000);
    }
  }
}

static void ensure_mqtt_connected() {
  if (!mqtt.connected()) {
    connect_mqtt();
  }
}

// ===================== Publish helpers =====================
static bool publish_event(const char* type, const char* detail) {
  char payload[200];
  const unsigned long ts = millis();

  if (detail && detail[0] != '\0') {
    snprintf(payload, sizeof(payload),
             "{\"ts_ms\":%lu,\"type\":\"%s\",\"detail\":\"%s\"}",
             ts, type, detail);
  } else {
    snprintf(payload, sizeof(payload),
             "{\"ts_ms\":%lu,\"type\":\"%s\"}",
             ts, type);
  }

  bool ok = mqtt.publish(TOPIC_EVT, payload);
  Serial.print("publish_event -> ");
  Serial.println(ok ? "OK" : "FAIL");
  return ok;
}

static bool publish_telemetry(int rssi, int mode, int battery_mv) {
  char payload[220];
  const unsigned long ts = millis();

  snprintf(payload, sizeof(payload),
           "{\"ts_ms\":%lu,\"rssi\":%d,\"mode\":%d,\"battery_mv\":%d,\"mqtt\":%s}",
           ts, rssi, mode, battery_mv, mqtt.connected() ? "true" : "false");

  bool ok = mqtt.publish(TOPIC_TLM, payload);
  Serial.print("publish_telemetry -> ");
  Serial.println(ok ? "OK" : "FAIL");
  return ok;
}

static uint32_t seq_raw = 0;
static bool publish_raw(float ax, float ay, float az, float gx, float gy, float gz) {
  char payload[240];
  const unsigned long ts = millis();
  const unsigned long seq = ++seq_raw;

  snprintf(payload, sizeof(payload),
           "{\"ts_ms\":%lu,\"seq\":%lu,\"imu\":{\"ax\":%.3f,\"ay\":%.3f,\"az\":%.3f,\"gx\":%.3f,\"gy\":%.3f,\"gz\":%.3f}}",
           ts, seq, ax, ay, az, gx, gy, gz);

  bool ok = mqtt.publish(TOPIC_RAW, payload);
  // Serial.println(ok ? "raw OK" : "raw FAIL");
  return ok;
}

void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool readBytes(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

static inline int16_t toInt16(uint8_t hi, uint8_t lo) {
  return (int16_t)((hi << 8) | lo);
}

int readButtonDebounced() {
  int s1 = digitalRead(BTN_PIN);
  delayMicroseconds(2000);
  int s2 = digitalRead(BTN_PIN);
  if (s1 != s2) return 0;
  return (s2 == LOW) ? 1 : 0;
}

// ===================== Your sensor stubs =====================
// 1) IMU read: MPU6050 -> ax,ay,az in "raw counts", gyro not used (set 0)
static bool read_imu(float &ax, float &ay, float &az, float &gx, float &gy, float &gz) {
  uint8_t b[14];

  // default outputs (safe)
  ax = ay = az = 0.0f;
  gx = gy = gz = 0.0f;

  // Read 14 bytes starting at ACCEL_XOUT_H
  if (!readBytes(ACCEL_XOUT_H, b, 14)) {
    // I2C read failed
    ax = ay = az = 0.0f;
    gx = gy = gz = 0.0f;
    return false;
  }

  // --------- Accel: convert to g (assuming default ±2g) ---------
  const int16_t ax_i = toInt16(b[0], b[1]);
  const int16_t ay_i = toInt16(b[2], b[3]);
  const int16_t az_i = toInt16(b[4], b[5]);

  // Default accel sensitivity for MPU6050 is 16384 LSB/g when AFS_SEL=0 (±2g)
  const float ACC_LSB_PER_G = 16384.0f;
  ax = (float)ax_i / ACC_LSB_PER_G;
  ay = (float)ay_i / ACC_LSB_PER_G;
  az = (float)az_i / ACC_LSB_PER_G;

  // --------- Gyro: convert to deg/s (assuming default ±250 dps) ---------
  const int16_t gx_i = toInt16(b[8],  b[9]);
  const int16_t gy_i = toInt16(b[10], b[11]);
  const int16_t gz_i = toInt16(b[12], b[13]);

  // Default gyro sensitivity for MPU6050 is 131 LSB/(deg/s) when FS_SEL=0 (±250 dps)
  const float GYRO_LSB_PER_DPS = 131.0f;
  gx = (float)gx_i / GYRO_LSB_PER_DPS;
  gy = (float)gy_i / GYRO_LSB_PER_DPS;
  gz = (float)gz_i / GYRO_LSB_PER_DPS;

  return true;
}

// 2) Mode: minimal implementation based on whether the button is pressed
// 0 = idle (not pressed), 1 = active (pressed)
static int read_mode() {
  const int btn = readButtonDebounced();  // 1 if pressed, 0 otherwise
  return (btn == 1) ? 1 : 0;
}

// 3) Battery mV: not present in your code, so return -1 (unknown)
// If you later add an ADC pin, we can compute real mV.
static int read_battery_mv() {
  return -1;
}

// 4) Event detection: returns true only on a rising edge (not pressed -> pressed)
// This prevents spamming events while holding the button.
static bool check_button_event() {
  static int prev_pressed = 0;           // 0/1
  const int cur_pressed = readButtonDebounced();

  const bool rising = (prev_pressed == 0 && cur_pressed == 1);
  prev_pressed = cur_pressed;

  return rising;
}


// ===================== Arduino =====================
void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000); // 100kHz stability
  delay(100);

  writeReg(PWR_MGMT_1, 0x00);  // wake up
  delay(50);

  delay(1000);

  connect_wifi();
  syncTime();      
  setup_tls();

  connect_mqtt();

  publish_event("boot", "device_up");
}

void loop() {
  ensure_mqtt_connected();
  mqtt.loop();

  const uint32_t now = millis();

  const int pressed = readButtonDebounced();

  static int prev_pressed = 0;
  if (pressed != prev_pressed) {
    if (pressed == 1) publish_event("press", "btn");
    else              publish_event("release", "btn");
    prev_pressed = pressed;
  }
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 500) {
    lastPrint = millis();
    Serial.print("pressed=");
    Serial.println(pressed);
  }

  if (pressed == 0) {
    delay(1);
    return;
  }
  static uint32_t last_raw_ms = 0;
  if (now - last_raw_ms >= 20) { // 20ms => 50Hz
    last_raw_ms = now;

    float ax, ay, az, gx, gy, gz;
    if (read_imu(ax, ay, az, gx, gy, gz)) {
      publish_raw(ax, ay, az, gx, gy, gz);
    } else {
      publish_event("imu_err", "i2c_fail");
    }
  }

  static uint32_t last_tlm_ms = 0;
  if (now - last_tlm_ms >= 2000) { // 2000ms => 0.5Hz
    last_tlm_ms = now;

    const int rssi = WiFi.RSSI();
    const int mode = 1;
    const int batt = read_battery_mv();
    //publish_telemetry(rssi, mode, batt);
  }

  // if (check_button_event()) {
  //   publish_event("click", "btn1");
  // }

  delay(1);
}
