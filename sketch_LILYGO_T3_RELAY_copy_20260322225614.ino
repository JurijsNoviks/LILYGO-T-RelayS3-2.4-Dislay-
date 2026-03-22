#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <LovyanGFX.hpp>

// ===== WIFI =====
const char* ssid = "BALTICOM2G3B";
const char* password = "n7jb9dmywhdj";

// ===== MQTT =====
const char* mqtt_server = "192.168.1.125";
const char* mqtt_user = "zanoza";
const char* mqtt_pass = "12345678";

WiFiClient espClient;
PubSubClient client(espClient);

// ===== RELAY =====
#define DATA_PIN 7
#define CLOCK_PIN 5
#define LATCH_PIN 6
#define OE_PIN 4

uint8_t relay_state = 0;
bool state[6] = {0};

// ===== DISPLAY =====
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Touch_XPT2046 _touch;

public:
  LGFX(void) {

    auto bcfg = _bus.config();
    bcfg.spi_host = SPI2_HOST;
    bcfg.freq_write = 20000000;
    bcfg.pin_sclk = 13;
    bcfg.pin_mosi = 11;
    bcfg.pin_miso = 12;
    bcfg.pin_dc   = 46;
    _bus.config(bcfg);

    _panel.setBus(&_bus);

    auto pcfg = _panel.config();
    pcfg.pin_cs = 8;
    pcfg.pin_rst = 3;
    pcfg.memory_width = 320;
    pcfg.memory_height = 240;
    pcfg.panel_width = 320;
    pcfg.panel_height = 240;
    pcfg.rgb_order = true;
    _panel.config(pcfg);

    auto tcfg = _touch.config();
    tcfg.spi_host = SPI2_HOST;
    tcfg.pin_cs = 14;
    tcfg.x_min = 200;
    tcfg.x_max = 3800;
    tcfg.y_min = 200;
    tcfg.y_max = 3800;
    _touch.config(tcfg);

    _panel.setTouch(&_touch);
    setPanel(&_panel);
  }
};

LGFX tft;

// ===== COLORS =====
uint16_t BG;
uint16_t CYAN;
uint16_t RED;
uint16_t DARK;

// ===== WIFI =====
void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(300);
}

// ===== MQTT =====
void reconnect() {
  while (!client.connected()) {
    client.connect("relay_panel", mqtt_user, mqtt_pass);
  }
}

// ===== RELAY =====
void updateRelay() {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, relay_state);
  digitalWrite(LATCH_PIN, HIGH);
}

// ===== DRAW BUTTON (УЛУЧШЕННЫЙ UI) =====
void drawButton(int i, bool force=false) {

  static bool last[6] = {0};
  if (!force && last[i] == state[i]) return;
  last[i] = state[i];

  int bw = 320/3;
  int bh = 240/2;

  int x = (i%3)*bw;
  int y = (i/3)*bh;

  uint16_t glow = state[i] ? RED : CYAN;

  // === BACKGROUND (glass) ===
  tft.fillRoundRect(x+6, y+6, bw-12, bh-12, 12, DARK);

  // === GLOW EFFECT ===
  for (int k=0; k<3; k++) {
    tft.drawRoundRect(x+2-k, y+2-k, bw-4+2*k, bh-4+2*k, 14, glow);
  }

  // === INNER BORDER ===
  tft.drawRoundRect(x+6, y+6, bw-12, bh-12, 12, glow);

  // === CENTER INDICATOR ===
  int cx = x + bw/2;
  int cy = y + bh/2 - 10;

  uint16_t fill = state[i] ? glow : tft.color565(10,30,40);

  tft.fillRoundRect(cx-20, cy-12, 40, 24, 10, fill);
  tft.drawRoundRect(cx-20, cy-12, 40, 24, 10, glow);

  // === TEXT ===
  tft.setTextDatum(middle_center);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, DARK);

  tft.drawString("R" + String(i+1), cx, y + bh - 25);
}

// ===== SETUP =====
void setup() {

  Serial.begin(115200);

  pinMode(9, OUTPUT);
  digitalWrite(9, HIGH);

  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(OE_PIN, OUTPUT);
  digitalWrite(OE_PIN, LOW);

  tft.init();
  tft.setRotation(6);

  // COLORS после init!
  BG   = TFT_BLACK;
  CYAN = tft.color565(0,200,255);
  RED  = tft.color565(255,40,40);
  DARK = tft.color565(18,18,22);

  tft.fillScreen(BG);

  for (int i=0;i<6;i++) drawButton(i, true);

  setup_wifi();
  client.setServer(mqtt_server,1883);

  ArduinoOTA.begin();
}

// ===== LOOP =====
void loop() {

  ArduinoOTA.handle();

  if (!client.connected()) reconnect();
  client.loop();

  uint16_t x,y;

  if (tft.getTouch(&x,&y)) {

    int bw = 320/3;
    int bh = 240/2;

    int id = (y/bh)*3 + (x/bw);

    if (id>=0 && id<6) {

      state[id] = !state[id];

      if (state[id]) relay_state |= (1<<id);
      else relay_state &= ~(1<<id);

      updateRelay();
      drawButton(id);

      delay(180);
    }
  }
}