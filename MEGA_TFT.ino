#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>

// ---------------- TFT ----------------
MCUFRIEND_kbv tft;

#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define GRAY    0x8410

String input = "";

// ---------------- ค่า Sensor ----------------
float temp_in, hum_in, temp_out, hum_out;
int ldr, co2;
int sol, evap, fan, led;

void setup() {
  Serial.begin(115200);   // Debug
  Serial1.begin(115200);  // รับจาก ESP32 (TX2 -> RX1 Mega Pin19)

  // ---------------- Init TFT ----------------
  uint16_t ID = tft.readID();
  tft.begin(ID);
  tft.setRotation(1);  // แนวนอน
  tft.fillScreen(BLACK);

  drawLayout();
}

void loop() {
  if (Serial1.available()) {
    input = Serial1.readStringUntil('\n');
    parseData(input);
    updateDashboard();
  }
}

// ---------------- Parsing ----------------
void parseData(String data) {
  String val;

  // Temp/Hum inside
  val = getValue(data, "TEMP_IN:");
  if(val == "" || val == "nan") temp_in = -1;
  else temp_in = val.toFloat();

  val = getValue(data, "HUM_IN:");
  if(val == "" || val == "nan") hum_in = -1;
  else hum_in = val.toFloat();

  // Temp/Hum outside
  val = getValue(data, "TEMP_OUT:");
  if(val == "" || val == "nan") temp_out = -1;
  else temp_out = val.toFloat();

  val = getValue(data, "HUM_OUT:");
  if(val == "" || val == "nan") hum_out = -1;
  else hum_out = val.toFloat();

  // LDR, CO2, Relay
  ldr  = getValue(data, "LDR:").toInt();
  co2  = getValue(data, "CO2:").toInt();
  sol  = getValue(data, "SOL:").toInt();
  evap = getValue(data, "EVAP:").toInt();
  fan  = getValue(data, "FAN:").toInt();
  led  = getValue(data, "LED:").toInt();
}

// ---------------- Get value ----------------
String getValue(String data, String tag) {
  int idx = data.indexOf(tag);
  if (idx < 0) return "";
  int endIdx = data.indexOf(",", idx);
  if (endIdx < 0) endIdx = data.length();
  return data.substring(idx + tag.length(), endIdx);
}


// ---------------- Layout ----------------
void drawLayout() {
  tft.fillScreen(BLACK);
  tft.setTextSize(2);

  // Header
  tft.fillRect(0, 0, 480, 30, BLUE);
  tft.setTextColor(WHITE, BLUE);
  tft.setCursor(160, 5); 
  tft.print("MUSHROOM HOUSE DASHBOARD");

  // Sensor Box
  drawBox(10, 40, 220, 180, "Sensors", CYAN);
  drawBox(250, 40, 220, 180, "Outside", YELLOW);

  // Relay Box
  drawBox(10, 240, 460, 80, "Relay Status", MAGENTA);
}

void drawBox(int x, int y, int w, int h, String title, uint16_t color) {
  tft.drawRect(x, y, w, h, color);
  tft.setTextColor(color, BLACK);
  tft.setCursor(x + 5, y - 15);
  tft.print(title);
}

// ---------------- Update Dashboard ----------------
void updateDashboard() {
  tft.setTextSize(2);
  tft.setTextColor(WHITE, BLACK);

  // --- Inside ---
  tft.setCursor(20, 60);  tft.print("Temp In : "); tft.print(temp_in); tft.print(" C  ");
  tft.setCursor(20, 90);  tft.print("Hum In  : "); tft.print(hum_in);  tft.print(" %  ");
  tft.setCursor(20, 120); tft.print("Light   : "); tft.print(ldr);     tft.print("    ");
  tft.setCursor(20, 150); tft.print("CO2     : "); tft.print(co2);     tft.print(" ppm ");

  // --- Outside ---
  tft.setCursor(260, 60);  tft.print("Temp Out: "); tft.print(temp_out); tft.print(" C  ");
  tft.setCursor(260, 90);  tft.print("Hum Out : "); tft.print(hum_out);  tft.print(" %  ");

  // --- Relay ---
  showRelay(20, 260, "SOL", sol);
  showRelay(140, 260, "EVAP", evap);
  showRelay(260, 260, "FAN", fan);
  showRelay(380, 260, "LED", led);

  // --- Alert (ถ้าเกิน Threshold) ---
  if(temp_in>32 || temp_in<30 || hum_in>75 || hum_in<70 || co2>1500){
    tft.fillRect(0,0,480,30, RED);
    tft.setTextColor(WHITE, RED);
    tft.setCursor(120,5); tft.print("ALERT! CHECK CONDITIONS");
  } else {
    tft.fillRect(0,0,480,30, BLUE);
    tft.setTextColor(WHITE, BLUE);
    tft.setCursor(160,5); tft.print("MUSHROOM HOUSE DASHBOARD");
  }

  if(temp_in >= 0) tft.print(temp_in); else tft.print("--");
if(hum_in >= 0) tft.print(hum_in); else tft.print("--");


}


void showRelay(int x, int y, String name, int state) {
  tft.fillRect(x, y, 100, 50, state ? GREEN : RED);
  tft.setTextColor(WHITE, state ? GREEN : RED);
  tft.setCursor(x + 20, y + 15);
  tft.print(name);
}
