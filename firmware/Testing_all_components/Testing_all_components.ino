#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>

// ═══════════════════════════════════════════
// WIFI CREDENTIALS
// ═══════════════════════════════════════════
const char* ssid     = "KP";
const char* password = "gandhi16";

// ═══════════════════════════════════════════
// PIN DEFINITIONS 
// ═══════════════════════════════════════════
#define MOTOR_PWM_PIN   7    // → DRV8838 EN pin 
#define MOTOR_DIR_PIN   6    // → DRV8838 PH pin
#define OPTO1_PIN       4    // → ITR-9606 #1 signal 
#define OPTO2_PIN       5    // → ITR-9606 #2 signal 
#define LED_MATRIX_PIN  14   // → 8x8 LED matrix (internal)
#define NUM_LEDS        64   // 8x8 = 64 total LEDs

// ═══════════════════════════════════════════
// LED MATRIX SETUP
// ═══════════════════════════════════════════
Adafruit_NeoPixel matrix(NUM_LEDS, LED_MATRIX_PIN, NEO_GRB + NEO_KHZ800);

// Colour definitions
#define OFF    matrix.Color(0,  0,  0)
#define GREEN  matrix.Color(0,  20, 0)    // dim green
#define RED    matrix.Color(20, 0,  0)    // dim red
#define BLUE   matrix.Color(0,  0,  20)   // dim blue
#define YELLOW matrix.Color(20, 15, 0)    // dim yellow
#define WHITE  matrix.Color(10, 10, 10)   // dim white
#define ORANGE matrix.Color(20, 8,  0)    // dim orange

// ═══════════════════════════════════════════
// SHARED VARIABLES
// ═══════════════════════════════════════════
volatile unsigned long revPeriod    = 0;
volatile unsigned long lastOpto1    = 0;
volatile unsigned long lastOpto2    = 0;
volatile int           currentRPM   = 0;
volatile unsigned long opto1Count   = 0;
volatile unsigned long opto2Count   = 0;
volatile bool          opto1Flash   = false;
volatile bool          opto2Flash   = false;
volatile bool          motorRunning = false;
volatile int           motorPWM     = 0;

// ═══════════════════════════════════════════
// LEDC (PWM) SETUP FOR MOTOR
// ═══════════════════════════════════════════
#define PWM_CHANNEL    0
#define PWM_FREQUENCY  5000     // 5kHz PWM frequency
#define PWM_RESOLUTION 8        // 8-bit = 0 to 255

// ═══════════════════════════════════════════
// INTERRUPT HANDLERS (WITH SOFTWARE DEBOUNCE)
// ═══════════════════════════════════════════
void IRAM_ATTR opto1ISR() {
    unsigned long now = micros();
    // DEBOUNCE: Ignore flickers faster than 5ms (5000 microseconds)
    // 5ms is fast enough to read a motor spinning at 12,000 RPM cleanly.
    if (now - lastOpto1 > 5000) { 
        revPeriod  = now - lastOpto1;
        lastOpto1  = now;
        opto1Count++;
        opto1Flash = true;
        if (revPeriod > 0) {
            currentRPM = (unsigned long)60000000 / revPeriod;
        }
    }
}

void IRAM_ATTR opto2ISR() {
    unsigned long now = micros();
    // DEBOUNCE filter for Sensor 2
    if (now - lastOpto2 > 5000) {
        lastOpto2  = now;
        opto2Count++;
        opto2Flash = true;
    }
}

// ═══════════════════════════════════════════
// MATRIX HELPER FUNCTIONS
// ═══════════════════════════════════════════
void setPixel(int row, int col, uint32_t color) {
    if (row < 0 || row > 7 || col < 0 || col > 7) return;
    int index = (row * 8) + col;
    matrix.setPixelColor(index, color);
}

void fillAll(uint32_t color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        matrix.setPixelColor(i, color);
    }
}

void fillRow(int row, uint32_t color) {
    for (int col = 0; col < 8; col++) {
        setPixel(row, col, color);
    }
}

void fillCol(int col, uint32_t color) {
    for (int row = 0; row < 8; row++) {
        setPixel(row, col, color);
    }
}

// ═══════════════════════════════════════════
// UPDATE LED MATRIX — visual debug display
// ═══════════════════════════════════════════
void updateMatrix() {
    matrix.clear();

    // ── ROWS 0-1: Motor status ──
    uint32_t motorColor = motorRunning ? GREEN : RED;
    fillRow(0, motorColor);
    fillRow(1, motorColor);

    // ── ROW 2: RPM bar graph ──
    int rpmLEDs = map(currentRPM, 0, 1200, 0, 8);
    rpmLEDs = constrain(rpmLEDs, 0, 8);
    for (int col = 0; col < rpmLEDs; col++) {
        setPixel(2, col, ORANGE);
    }

    // ── ROW 3: Revolution counter ──
    int revDisplay = (opto1Count % 800) / 100;
    for (int col = 0; col < revDisplay; col++) {
        setPixel(3, col, WHITE);
    }

    // ── ROWS 4-5: Opto status ──
    if (opto1Flash) {
        fillRow(4, BLUE);
        opto1Flash = false;   
    }
    if (opto2Flash) {
        fillRow(5, YELLOW);
        opto2Flash = false;
    }

    // ── ROW 6-7: Opto count bars ──
    int o1bar = opto1Count % 8;
    int o2bar = opto2Count % 8;
    for (int col = 0; col < (int)o1bar; col++) {
        setPixel(6, col, BLUE);
    }
    for (int col = 0; col < (int)o2bar; col++) {
        setPixel(7, col, YELLOW);
    }

    matrix.show();
}

// ═══════════════════════════════════════════
// WEB DEBUG PAGE
// ═══════════════════════════════════════════
WebServer server(80);

void handleRoot() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>VLD Test</title>";
    html += "<meta http-equiv='refresh' content='1'>";
    html += "<style>";
    html += "body{background:#0d0d0d;color:#00ff88;font-family:monospace;padding:20px;max-width:600px;}";
    html += "h1{color:#fff;} h2{color:#aaa;border-bottom:1px solid #333;padding-bottom:4px;}";
    html += ".val{color:#00ccff;font-size:1.3em;font-weight:bold;}";
    html += ".ok{color:#00ff88;} .err{color:#ff4444;} .warn{color:#ffaa00;}";
    html += ".box{background:#1a1a1a;border:1px solid #333;padding:15px;margin:10px 0;border-radius:6px;}";
    html += ".bar{background:#333;height:16px;border-radius:4px;margin-top:5px;}";
    html += ".fill{background:#00ccff;height:16px;border-radius:4px;}";
    html += "</style></head><body>";

    html += "<h1>VLD Test Dashboard</h1>";

    // Motor box 
    html += "<div class='box'><h2>Motor (GPIO7=PWM, GPIO6=DIR)</h2>";
    html += "Status: <span class='val'>";
    html += motorRunning ? "<span class='ok'>RUNNING</span>" : "<span class='err'>STOPPED</span>";
    html += "</span><br>";
    html += "PWM value: <span class='val'>" + String(motorPWM) + " / 255</span><br>";
    html += "RPM: <span class='val'>" + String(currentRPM) + "</span><br>";
    html += "Revolution period: <span class='val'>" + String(revPeriod / 1000) + " ms</span>";

    // RPM bar
    int rpmPct = constrain(map(currentRPM, 0, 1200, 0, 100), 0, 100);
    html += "<div class='bar'><div class='fill' style='width:" + String(rpmPct) + "%;background:#00ff88;'></div></div>";
    html += "</div>";

    // Opto 1 box 
    html += "<div class='box'><h2>Opto 1 — Bottom Dead Centre (GPIO4)</h2>";
    html += "Current state: <span class='val'>";
    html += digitalRead(OPTO1_PIN) == LOW ? "<span class='warn'>CLEAR</span>" : "<span class='ok'>BLOCKED</span>";
    html += "</span><br>";
    html += "Total triggers: <span class='val'>" + String(opto1Count) + "</span><br>";
    html += "Last period: <span class='val'>" + String(revPeriod / 1000) + " ms</span></div>";

    // Opto 2 box 
    html += "<div class='box'><h2>Opto 2 — Top Dead Centre (GPIO5)</h2>";
    html += "Current state: <span class='val'>";
    html += digitalRead(OPTO2_PIN) == LOW ? "<span class='warn'>CLEAR</span>" : "<span class='ok'>BLOCKED</span>";
    html += "</span><br>";
    html += "Total triggers: <span class='val'>" + String(opto2Count) + "</span></div>";

    // System box
    html += "<div class='box'><h2>System</h2>";
    html += "Uptime: <span class='val'>" + String(millis() / 1000) + " sec</span><br>";
    html += "Free heap: <span class='val'>" + String(ESP.getFreeHeap()) + " bytes</span><br>";
    html += "WiFi RSSI: <span class='val'>" + String(WiFi.RSSI()) + " dBm</span></div>";

    // LED matrix legend
    html += "<div class='box'><h2>LED Matrix Legend</h2>";
    html += "Row 0-1: GREEN=motor running, RED=stopped<br>";
    html += "Row 2: ORANGE bar = RPM (0-1200)<br>";
    html += "Row 3: WHITE = revolution count<br>";
    html += "Row 4: BLUE flash = Opto1 trigger<br>";
    html += "Row 5: YELLOW flash = Opto2 trigger<br>";
    html += "Row 6-7: Trigger count bars</div>";

    html += "<small>Auto-refreshes every 1 second</small>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

// ═══════════════════════════════════════════
// CORE 1 TASK — WiFi + OTA + Web server
// ═══════════════════════════════════════════
TaskHandle_t webTaskHandle;

void webTask(void* parameter) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        fillAll(GREEN);
        matrix.show();
        delay(500);
        matrix.clear();
        matrix.show();
        
        ArduinoOTA.onStart([]() {
            ledcWrite(MOTOR_PWM_PIN, 0);
            motorRunning = false;
        });
        ArduinoOTA.begin();

        server.on("/", handleRoot);
        server.begin();
    } else {
        setPixel(0, 0, ORANGE); setPixel(0, 7, ORANGE);
        setPixel(3, 3, ORANGE); setPixel(3, 4, ORANGE);
        setPixel(4, 3, ORANGE); setPixel(4, 4, ORANGE);
        setPixel(7, 0, ORANGE); setPixel(7, 7, ORANGE);
        matrix.show();
    }

    while (true) {
        if (WiFi.status() == WL_CONNECTED) {
            ArduinoOTA.handle();
            server.handleClient();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
}

// ═══════════════════════════════════════════
// CORE 0 TASK — Motor + Opto + LED display
// ═══════════════════════════════════════════
void realtimeTask(void* parameter) {
    ledcAttachChannel(MOTOR_PWM_PIN, PWM_FREQUENCY, PWM_RESOLUTION, PWM_CHANNEL);

    pinMode(MOTOR_DIR_PIN, OUTPUT);
    digitalWrite(MOTOR_DIR_PIN, HIGH);

    // Standard INPUT since we hardwired real 10k physical pull-ups 
    pinMode(OPTO1_PIN, INPUT);
    pinMode(OPTO2_PIN, INPUT);

    // Triggers when going from 3.3V (Blocked) to 0V (Clear)
    attachInterrupt(digitalPinToInterrupt(OPTO1_PIN), opto1ISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(OPTO2_PIN), opto2ISR, FALLING);

    int testPhase = 0;
    unsigned long phaseStart = millis();

    while (true) {
        unsigned long now = millis();
        unsigned long elapsed = now - phaseStart;

        if (elapsed > 5000) {
            testPhase = (testPhase + 1) % 4;
            phaseStart = now;
        }

        switch (testPhase) {
            case 0:
                motorPWM = 102;
                ledcWrite(MOTOR_PWM_PIN, motorPWM);
                motorRunning = true;
                break;
            case 1:
                motorPWM = 178;
                ledcWrite(MOTOR_PWM_PIN, motorPWM);
                motorRunning = true;
                break;
            case 2:
                motorPWM = 255;
                ledcWrite(MOTOR_PWM_PIN, motorPWM);
                motorRunning = true;
                break;
            case 3:
                motorPWM = 0;
                ledcWrite(MOTOR_PWM_PIN, 0);
                motorRunning = false;
                break;
        }

        updateMatrix();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// ═══════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════
void setup() {
    Serial.begin(115200);

    matrix.begin();
    matrix.setBrightness(15);   
    matrix.clear();
    matrix.show();

    for (int i = 0; i < NUM_LEDS; i++) {
        matrix.setPixelColor(i, WHITE);
        matrix.show();
        delay(20);
    }
    delay(300);
    matrix.clear();
    matrix.show();

    xTaskCreatePinnedToCore(
        webTask,        
        "WebTask",      
        12000,          
        NULL,
        1,              
        &webTaskHandle,
        1               
    );

    xTaskCreatePinnedToCore(
        realtimeTask,
        "RealtimeTask",
        8000,
        NULL,
        2,              
        NULL,
        0               
    );
}

// ═══════════════════════════════════════════
// MAIN LOOP (Used strictly for diagnostic text)
// ═══════════════════════════════════════════
void loop() {
    // Print live status to the Serial Monitor (115200 baud)
    // You should see 1 when blocked, and 0 when clear.
    Serial.print("Opto 1 Pin Status: ");
    Serial.print(digitalRead(OPTO1_PIN));
    Serial.print("  |  Opto 2 Pin Status: ");
    Serial.println(digitalRead(OPTO2_PIN));
    
    vTaskDelay(200 / portTICK_PERIOD_MS);
}