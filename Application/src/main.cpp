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

// ---- Settings ----------------------------------------------------------------

const char *WIFI_NAME = "JouleObserver";
const char *WIFI_PASSWORD = nullptr;  // nullptr = open network, or 8+ characters

const int ATMEGA_RX_PIN = 16;  // UART2 RX, from the ATmega's TX via the level shifter
const int ATMEGA_TX_PIN = 17;  // UART2 TX, to the ATmega's RX (not used yet)
const unsigned long ATMEGA_BAUD = 9600;

const int MAX_PAIRS = 64;      // most waveform samples per channel in one $W line
const size_t MAX_LINE = 1024;  // longer UART lines are dropped

// web/index.html, built in by board_build.embed_files in platformio.ini
extern const uint8_t page_start[] asm("_binary_web_index_html_start");
extern const uint8_t page_end[] asm("_binary_web_index_html_end");

WebServer server(80);
DNSServer dns;

// ---- Latest data from the ATmega -----------------------------------------------

bool haveReadings = false;  // false until the first $M line arrives
uint32_t readingsAt = 0;    // millis() when the last $M line arrived
long vrms_mV = 0, irms_mA = 0, power_mW = 0;

int pairs = 0;  // samples per channel in the last $W line
long dt_us = 0;
int16_t v_mV[MAX_PAIRS], i_mA[MAX_PAIRS];

// ---- Reading the ATmega's lines --------------------------------------------------

// Reads ",123,-45,..." into out[]. Returns how many numbers it read (at most
// maxCount, any after that are ignored), or -1 if the text is garbled.
int readNumbers(const char *text, long *out, int maxCount) {
  int count = 0;
  while (*text == ',' && count < maxCount) {
    char *end;
    out[count] = strtol(text + 1, &end, 10);
    if (end == text + 1) return -1;  // empty field or not a number
    count++;
    text = end;
  }
  return (*text == '\0' || *text == ',') ? count : -1;
}

void handleLine(char *line) {
  Serial.println(line);  // echo to USB, so `pio device monitor` shows what arrived

  if (strncmp(line, "$M", 2) == 0) {
    long n[3];
    if (readNumbers(line + 2, n, 3) != 3) return;
    vrms_mV = n[0];
    irms_mA = n[1];
    power_mW = n[2];
    haveReadings = true;
    readingsAt = millis();
  } else if (strncmp(line, "$W", 2) == 0) {
    static long n[1 + 2 * MAX_PAIRS];
    int count = readNumbers(line + 2, n, 1 + 2 * MAX_PAIRS);
    if (count < 5 || count % 2 == 0 || n[0] <= 0) return;  // dt plus 2 or more whole pairs
    dt_us = n[0];
    pairs = (count - 1) / 2;
    for (int k = 0; k < pairs; k++) {
      v_mV[k] = constrain(n[1 + 2 * k], -32768L, 32767L);
      i_mA[k] = constrain(n[2 + 2 * k], -32768L, 32767L);
    }
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
  float vrms = 14.0f + 0.3f * sinf(2 * PI * t / 23);
  float r = 50 + 27 * sinf(2 * PI * t / 30);  // load resistor, ohms
  float x = 2 * PI * 500 * 0.004f;            // 4 mH at 500 Hz, ohms
  float irms = vrms / sqrtf(r * r + x * x);
  float phi = atan2f(x, r);                   // current lags voltage
  float power = vrms * irms * cosf(phi);

  char text[512];
  int len = snprintf(text, sizeof text, "$M,%ld,%ld,%ld\n$W,200", lroundf(vrms * 1000),
                     lroundf(irms * 1000), lroundf(power * 1000));
  for (int k = 0; k < 20; k++) {  // 2 cycles, 200 us apart, from a rising zero crossing
    float wt = 2 * PI * 500 * k * 200e-6f;
    len += snprintf(text + len, sizeof text - len, ",%ld,%ld",
                    lroundf(1000 * sqrtf(2) * vrms * sinf(wt)) + random(-30, 31),
                    lroundf(1000 * sqrtf(2) * irms * sinf(wt - phi)) + random(-2, 3));
  }
  len += snprintf(text + len, sizeof text - len, "\n");
  for (int k = 0; k < len; k++) feedByte(text[k]);
}
#endif

// ---- Web pages --------------------------------------------------------------------

// GET /: the dashboard.
void sendPage() {
  server.send_P(200, "text/html", (const char *)page_start, page_end - page_start);
}

// GET /api/data: the latest readings and waveform as JSON (see docs/spec.md).
void sendData() {
  String json;
  json.reserve(200 + 16 * pairs);
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
  dns.processNextRequest();
  server.handleClient();
#ifdef MOCK_ATMEGA
  mockAtmega();
#else
  while (Serial2.available()) feedByte(Serial2.read());
#endif
}
