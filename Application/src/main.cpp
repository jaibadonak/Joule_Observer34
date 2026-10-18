// Joule Observer WiFi dashboard, for an ESP32-DevKitC-32E.
//
// The ESP32 runs its own WiFi network. Join it with a phone and the phone's
// "sign in to network" screen opens a live dashboard (a captive portal).
//
//   ATmega328P --UART, 9600 baud--> ESP32 UART2 --WiFi--> phone
//               $M and $W lines                  /api/data JSON
//
// Both formats are in docs/spec.md. The dashboard is web/index.html, which the
// build puts inside the firmware.

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include "uart_protocol.h"

// ---- Settings ----------------------------------------------------------------

const char *WIFI_NAME = "JouleObserver";
const char *WIFI_PASSWORD = nullptr;  // nullptr = open network, or 8+ characters

const int ATMEGA_RX_PIN = 16;  // UART2 RX, from the ATmega's TX via the level shifter
const int ATMEGA_TX_PIN = 17;  // UART2 TX, to the ATmega's RX (not used yet)
const unsigned long ATMEGA_BAUD = 9600;

// Mode metadata for the website only; no ESP32 indicator GPIOs are driven.
const uint32_t MODE_TIMEOUT_MS = 1500;
int displayMode = -1;
uint32_t modeAt = 0;

const int MAX_PAIRS = 64;      // most waveform samples per channel in one $W line
const size_t MAX_LINE = 1024;  // longer UART lines are dropped

// web/index.html, built in by board_build.embed_files in platformio.ini
extern const uint8_t page_start[] asm("_binary_web_index_html_gz_start");
extern const uint8_t page_end[] asm("_binary_web_index_html_gz_end");

WebServer server(80);
DNSServer dns;

// ---- Latest data from the ATmega -----------------------------------------------

bool haveReadings = false;  // false until the first $M line arrives
uint32_t readingsAt = 0;    // millis() when the last $M line arrived
long vrms_mV = 0, irms_mA = 0, power_mW = 0;
long total_mWh = 0;
bool haveEnergy = false;
uint32_t sequence = 0, waveSequence = 0, waveformAt = 0, streamEpoch = 1;
uint32_t invalidPackets = 0, droppedLines = 0;
bool haveCalibration = false;
long calibration[11];
uint32_t calibrationAt = 0;
const char *CALIBRATION_KEYS[] = {"adc_reference_mV", "voltage_scale_x100", "current_scale_x1000",
  "v_min", "v_max", "v_mean_x100", "v_rms_x10", "i_min", "i_max", "i_mean_x100", "i_rms_x10"};

int pairs = 0;  // samples per channel in the last $W line
long dt_us = 0;
int32_t v_mV[MAX_PAIRS], i_mA[MAX_PAIRS];

// ---- Reading the ATmega's lines --------------------------------------------------

void handleLine(char *line) {
  Serial.println(line);  // echo to USB, so `pio device monitor` shows what arrived

  if (strncmp(line, "BOOT JouleObserver", 18) == 0) {
    haveReadings = haveEnergy = false;
    haveCalibration = false;
    pairs = 0;
    streamEpoch++;
    displayMode = -1;
  } else if (strncmp(line, "$D,", 3) == 0) {
    // Exactly one digit: reject malformed, missing, extra or out-of-range fields.
    int mode = parseDisplayMode(line);
    if (mode < 0) { invalidPackets++; return; }
    displayMode = mode;
    modeAt = millis();
  } else if (strncmp(line, "$M,", 3) == 0) {
    long n[4];
    int count = readNumbers(line + 2, n, 4);
    if (count < 3 || n[0] < 0 || n[1] < 0 || (count >= 4 && n[3] < 0)) {
      invalidPackets++; return;
    }
    vrms_mV = n[0];
    irms_mA = n[1];
    power_mW = n[2];
    haveEnergy = count >= 4;
    if (haveEnergy) total_mWh = n[3];
    haveReadings = true;
    readingsAt = millis();
    sequence++;
  } else if (strncmp(line, "$W,", 3) == 0) {
    static long n[1 + 2 * MAX_PAIRS];
    int count = readNumbers(line + 2, n, 1 + 2 * MAX_PAIRS);
    if (count < 5 || count % 2 == 0 || n[0] <= 0 || n[0] > 1000000L) {
      invalidPackets++; return;
    }
    dt_us = n[0];
    pairs = min((count - 1) / 2, MAX_PAIRS);
    for (int k = 0; k < pairs; k++) {
      v_mV[k] = n[1 + 2 * k];
      i_mA[k] = n[2 + 2 * k];
    }
    waveformAt = millis();
    waveSequence++;
  } else if (strncmp(line, "$C,", 3) == 0) {
    long c[11];
    int count = readNumbers(line + 2, c, 11);
    if (!validCalibration(c, count)) { invalidPackets++; return; }
    memcpy(calibration, c, sizeof calibration);
    haveCalibration = true;
    calibrationAt = millis();
  } else if (line[0] == '$') {
    invalidPackets++;
  }
  // Any other line (say, human-readable text for a terminal) is ignored.
}

// Collects UART bytes into lines and hands each finished line to handleLine().
void feedByte(char c) {
  static char line[MAX_LINE];
  static size_t len = 0;
  static bool tooLong = false;

  if (c == '\n') {
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ')) len--;
    line[len] = '\0';
    if (!tooLong && len > 0) handleLine(line);
    if (tooLong) droppedLines++;
    len = 0;
    tooLong = false;
  } else if (len < MAX_LINE - 1) {
    line[len++] = c;
  } else {
    tooLong = true;
  }
}

#ifdef MOCK_ATMEGA
// Stands in for the ATmega: models the lab bench (a 14 V, 500 Hz source driving
// 4 mH in series with a resistor that sweeps the load between 2.5 and 7.5 VA)
// and feeds the same $M/$W lines through the same code as real UART data.
void mockAtmega() {
  static uint32_t last = 0;
  if (millis() - last < 500) return;
  last = millis();

  float t = millis() / 1000.0f;
  char modeLine[] = "$D,0";
  modeLine[3] += (millis() / 3000) % 4;
  handleLine(modeLine);
  float vrms = 14.0f + 0.3f * sinf(2 * PI * t / 23);
  float r = 50 + 27 * sinf(2 * PI * t / 30);  // load resistor, ohms
  float x = 2 * PI * 500 * 0.004f;            // 4 mH at 500 Hz, ohms
  float irms = vrms / sqrtf(r * r + x * x);
  float phi = atan2f(x, r);                   // current lags voltage
  float power = vrms * irms * cosf(phi);

  char text[512];
  static double mockEnergy = 0;
  mockEnergy += power * 1000.0 * 0.5 / 3600.0;
  int len = snprintf(text, sizeof text, "$M,%ld,%ld,%ld,%ld\n$W,200", lroundf(vrms * 1000),
                     lroundf(irms * 1000), lroundf(power * 1000), lround(mockEnergy));
  for (int k = 0; k < 20; k++) {  // 2 cycles, 200 us apart, from a rising zero crossing
    float wt = 2 * PI * 500 * k * 200e-6f;
    len += snprintf(text + len, sizeof text - len, ",%ld,%ld",
                    lroundf(1000 * sqrtf(2) * vrms * sinf(wt)) + random(-30, 31),
                    lroundf(1000 * sqrtf(2) * irms * sinf(wt - phi)) + random(-2, 3));
  }
  len += snprintf(text + len, sizeof text - len, "\n");
  for (int k = 0; k < len; k++) feedByte(text[k]);
  char calLine[180];
  int vp = lroundf(vrms * sqrtf(2) * 1000 / 109.26f);
  int ip = lroundf(irms * sqrtf(2) * 1000 / 3.877f);
  snprintf(calLine, sizeof calLine, "$C,5000,10926,3877,%d,%d,42200,%ld,%d,%d,42200,%ld",
    422 - vp, 422 + vp, lroundf(vrms * 10000 / 109.26f),
    422 - ip, 422 + ip, lroundf(irms * 10000 / 3.877f));
  handleLine(calLine);
}
#endif

// ---- Web pages --------------------------------------------------------------------

// GET /: the dashboard.
void sendPage() {
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", (const char *)page_start, page_end - page_start);
}

// GET /api/data: the latest readings and waveform as JSON (see docs/spec.md).
void sendData() {
  String json;
  json.reserve(900 + 26 * pairs);
#ifdef MOCK_ATMEGA
  json += "{\"source\":\"mock\",\"age_ms\":";
#else
  json += "{\"source\":\"uart\",\"age_ms\":";
#endif
  if (haveReadings) {
    json += millis() - readingsAt;
  } else {
    json += "null";
  }
  json += ",\"vrms_mV\":";
  json += vrms_mV;
  json += ",\"irms_mA\":";
  json += irms_mA;
  json += ",\"power_mW\":";
  json += power_mW;
  json += ",\"energy_mWh\":";
  if (haveEnergy && haveReadings) json += total_mWh;
  else json += "null";
  json += ",\"sequence\":"; json += sequence;
  json += ",\"epoch\":"; json += streamEpoch;
  json += ",\"uptime_ms\":"; json += millis();
  json += ",\"display_mode\":"; json += displayMode;
  json += ",\"wave_sequence\":"; json += waveSequence;
  json += ",\"wave_age_ms\":";
  if (pairs) json += millis() - waveformAt;
  else json += "null";
  json += ",\"invalid_packets\":"; json += invalidPackets;
  json += ",\"dropped_lines\":"; json += droppedLines;
  json += ",\"calibration\":";
  if (haveCalibration) {
    json += "{\"age_ms\":"; json += millis() - calibrationAt;
    for (int k = 0; k < 11; k++) {
      json += ",\""; json += CALIBRATION_KEYS[k]; json += "\":"; json += calibration[k];
    }
    json += '}';
  } else json += "null";
  json += ",\"dt_us\":";
  json += dt_us;
  json += ",\"v_mV\":[";
  for (int k = 0; k < pairs; k++) {
    if (k) json += ',';
    json += v_mV[k];
  }
  json += "],\"i_mA\":[";
  for (int k = 0; k < pairs; k++) {
    if (k) json += ',';
    json += i_mA[k];
  }
  json += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

// Any other URL, including the "am I online?" checks a phone makes when it
// joins a network, is redirected to the dashboard. That redirect is what makes
// the phone pop up its sign-in screen with our page in it.
void redirectToPage() {
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

// ---- Main -------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial2.setRxBufferSize(1024);  // holds a whole $W line while the web server is busy
  Serial2.begin(ATMEGA_BAUD, SERIAL_8N1, ATMEGA_RX_PIN, ATMEGA_TX_PIN);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_NAME, WIFI_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  dns.start(53, "*", ip);  // every name a phone looks up points at the ESP32

  server.on("/", sendPage);
  server.on("/api/data", sendData);
  server.on("/favicon.ico", [] { server.send(204); });
  server.onNotFound(redirectToPage);
  server.begin();

  Serial.printf("\nJoin WiFi \"%s\", then the dashboard opens (or go to http://%s)\n", WIFI_NAME,
                ip.toString().c_str());
#ifdef MOCK_ATMEGA
  Serial.println("MOCK build: simulated readings, UART2 is not read");
#endif
}

void loop() {
#ifdef MOCK_ATMEGA
  mockAtmega();
#else
  while (Serial2.available()) feedByte(Serial2.read());
#endif
  dns.processNextRequest();
  server.handleClient();
  if (displayMode >= 0 && millis() - modeAt >= MODE_TIMEOUT_MS) displayMode = -1;
}
