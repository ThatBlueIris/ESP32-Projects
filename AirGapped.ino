#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

#define RGB_PIN  48
#define BOOT_BTN 0

// Network Configuration (Offline, no internet uplink)
const char* AP_SSID = "ESP32-VAULT-AIRGAP";
const char* AP_PASS = "vault1234";
const byte DNS_PORT = 53;

// Master PIN
const char* MASTER_PIN = "1337";

// Low-intensity LED helper (0-255 scale, values kept 0-15 for eye comfort)
void setLed(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_PIN, r, g, b);
}

WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

// Session & Security State
bool sessionUnlocked = false;
bool waitingForPresence = false;
unsigned long presenceTimer = 0;
unsigned long lastActivityTime = 0;

const unsigned long PRESENCE_TIMEOUT_MS = 15000; // 15 seconds to tap button
const unsigned long SESSION_TIMEOUT_MS  = 90000; // 90 seconds auto-lock idle

// -------------------------------------------------------------
// HTML Page Generators
// -------------------------------------------------------------

String getLoginPage(String errorMsg = "") {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{background:#0b0f19;color:#f8fafc;font-family:system-ui,-apple-system,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;padding:20px;box-sizing:border-box;}";
  html += ".card{background:#151e32;padding:2rem;border-radius:14px;box-shadow:0 12px 30px rgba(0,0,0,0.6);width:100%;max-width:340px;text-align:center;border:1px solid #1e293b;}";
  html += "input{width:100%;padding:14px;margin:14px 0;background:#0b0f19;border:1px solid #334155;border-radius:8px;color:#fff;box-sizing:border-box;font-size:18px;text-align:center;letter-spacing:6px;}";
  html += "button{width:100%;padding:12px;background:#38bdf8;border:none;border-radius:8px;color:#0b0f19;font-weight:700;font-size:15px;cursor:pointer;}";
  html += ".err{color:#f87171;font-size:13px;margin-bottom:8px;}</style></head><body>";
  html += "<div class='card'><h2 style='margin-top:0;'>Air-Gapped Vault</h2><p style='color:#94a3b8;font-size:13px;'>Hardware Presence Verification Required</p>";
  if (errorMsg != "") html += "<div class='err'>" + errorMsg + "</div>";
  html += "<form method='POST' action='/auth'>";
  html += "<input type='password' name='pin' placeholder='PIN' maxlength='8' required autofocus>";
  html += "<button type='submit'>Verify Master PIN</button>";
  html += "</form></div></body></html>";
  return html;
}

String getWaitingPresencePage() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='2;url=/vault'>";
  html += "<style>body{background:#0b0f19;color:#f8fafc;font-family:system-ui,sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;}";
  html += ".card{background:#151e32;padding:2rem;border-radius:14px;box-shadow:0 12px 30px rgba(0,0,0,0.6);width:100%;max-width:340px;text-align:center;border:1px solid #1e293b;}";
  html += ".pulse{display:inline-block;width:16px;height:16px;border-radius:50%;background:#38bdf8;box-shadow:0 0 12px #38bdf8;animation:pulse 1s infinite alternate;}";
  html += "@keyframes pulse{from{transform:scale(0.85);opacity:0.5;}to{transform:scale(1.25);opacity:1;}}</style></head><body>";
  html += "<div class='card'><div class='pulse'></div>";
  html += "<h3>Hardware Lock Engaged</h3><p style='color:#cbd5e1;font-size:14px;'>PIN Accepted. Tap physical <b>BOOT</b> on the ESP32 to authorize.</p>";
  html += "<p style='color:#64748b;font-size:12px;'>Window expires in 15 seconds...</p>";
  html += "</div></body></html>";
  return html;
}

String getDashboardPage() {
  lastActivityTime = millis();
  
  prefs.begin("sec_store", true);
  int count = prefs.getInt("count", 0);

  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{background:#0b0f19;color:#f8fafc;font-family:system-ui,sans-serif;padding:20px;margin:0;}";
  html += ".container{max-width:540px;margin:0 auto;}";
  html += ".hdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:15px;}";
  html += ".badge{background:#10b981;color:#0b0f19;padding:4px 10px;border-radius:20px;font-size:11px;font-weight:700;}";
  html += ".item{background:#151e32;padding:14px 16px;border-radius:10px;margin-bottom:12px;border:1px solid #1e293b;border-left:4px solid #38bdf8;}";
  html += ".row{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;}";
  html += ".label{font-size:12px;color:#94a3b8;font-weight:600;text-transform:uppercase;}";
  html += ".val-row{display:flex;align-items:center;gap:8px;}";
  html += ".val{font-family:monospace;font-size:14px;word-break:break-all;color:#38bdf8;flex:1;}";
  html += "button{padding:6px 12px;border:none;border-radius:6px;font-size:12px;cursor:pointer;font-weight:600;}";
  html += ".btn-ghost{background:#1e293b;color:#cbd5e1;}";
  html += ".btn-del{background:#ef444420;color:#f87171;}";
  html += ".card-add{background:#151e32;padding:16px;border-radius:10px;border:1px solid #1e293b;margin-top:20px;}";
  html += "input{width:100%;padding:10px;margin:6px 0 12px 0;background:#0b0f19;border:1px solid #334155;border-radius:6px;color:#fff;box-sizing:border-box;}";
  html += ".btn-add{background:#38bdf8;color:#0b0f19;width:100%;padding:10px;font-size:14px;}";
  html += ".btn-lock{background:#ef4444;color:#fff;width:100%;padding:12px;border-radius:8px;font-size:15px;margin-top:25px;}";
  html += "</style></head><body>";
  
  html += "<div class='container'><div class='hdr'><div><h2 style='margin:0;'>Secure Vault</h2>";
  html += "<span style='font-size:12px;color:#64748b;'>Auto-locks after 90s idle</span></div>";
  html += "<span class='badge'>HARDWARE AUTH</span></div>";

  // List all stored secrets dynamically
  if (count == 0) {
    html += "<p style='color:#64748b;text-align:center;padding:20px;'>No secrets stored yet. Add one below.</p>";
  } else {
    for (int i = 0; i < count; i++) {
      String keyLabel = "k_" + String(i);
      String keyVal   = "v_" + String(i);
      String label = prefs.getString(keyLabel.c_str(), "");
      String value = prefs.getString(keyVal.c_str(), "");

      if (label != "") {
        html += "<div class='item'>";
        html += "<div class='row'><span class='label'>" + label + "</span>";
        html += "<a href='/del?idx=" + String(i) + "' onclick='return confirm(\"Delete this secret?\");'><button class='btn-del'>Delete</button></a></div>";
        html += "<div class='val-row'>";
        html += "<span class='val' id='v" + String(i) + "'>••••••••••••</span>";
        html += "<button class='btn-ghost' onclick='toggle(\"v" + String(i) + "\", \"" + value + "\")'>Show</button>";
        html += "<button class='btn-ghost' onclick='navigator.clipboard.writeText(\"" + value + "\");alert(\"Copied!\");'>Copy</button>";
        html += "</div></div>";
      }
    }
  }
  prefs.end();

  // Add new secret form
  html += "<div class='card-add'><h4 style='margin-top:0;margin-bottom:10px;'>Add New Secret</h4>";
  html += "<form method='POST' action='/add'>";
  html += "<label class='label'>Label / Service</label><input type='text' name='lbl' placeholder='e.g., GitHub Personal Token' required>";
  html += "<label class='label'>Secret Value</label><input type='text' name='val' placeholder='e.g., ghp_xxxxxxxxxxxxxx' required>";
  html += "<button type='submit' class='btn-add'>Save to Secure NVS</button>";
  html += "</form></div>";

  html += "<a href='/lock'><button class='btn-lock'>Lock Vault Immediately</button></a></div>";

  // Script to toggle mask and copy
  html += "<script>function toggle(id, realVal) {";
  html += "var el = document.getElementById(id);";
  html += "if (el.innerText === '••••••••••••') { el.innerText = realVal; } else { el.innerText = '••••••••••••'; }";
  html += "}</script>";

  html += "</body></html>";
  return html;
}

// -------------------------------------------------------------
// Setup & Routing
// -------------------------------------------------------------

void setup() {
  pinMode(BOOT_BTN, INPUT_PULLUP);

  // Initialize NVS storage and seed default entries if fresh
  prefs.begin("sec_store", false);
  if (!prefs.isKey("count")) {
    prefs.putInt("count", 2);
    prefs.putString("k_0", "GitHub Deployment Token");
    prefs.putString("v_0", "ghp_ProdDeployToken_8912xAlpha");
    prefs.putString("k_1", "AWS Root Recovery Seed");
    prefs.putString("v_1", "echo-tango-7749-cipher-delta");
  }
  prefs.end();

  // Start Isolated SoftAP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  // Setup Captive Portal DNS: redirect every query to the ESP32's IP
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Web Handlers
  server.on("/", HTTP_GET, []() {
    if (sessionUnlocked) {
      server.send(200, "text/html", getDashboardPage());
    } else {
      server.send(200, "text/html", getLoginPage());
    }
  });

  server.on("/auth", HTTP_POST, []() {
    if (server.hasArg("pin")) {
      String enteredPin = server.arg("pin");
      if (enteredPin == MASTER_PIN) {
        waitingForPresence = true;
        presenceTimer = millis();
        setLed(0, 0, 15); // Dim Blue: waiting for physical presence
        server.send(200, "text/html", getWaitingPresencePage());
        return;
      }
    }
    // Failed PIN attempt
    setLed(15, 0, 0); // Dim Red: Denied
    server.send(200, "text/html", getLoginPage("Invalid Master PIN"));
    delay(400);
    setLed(6, 4, 0);  // Back to Dim Amber
  });

  server.on("/vault", HTTP_GET, []() {
    if (sessionUnlocked) {
      server.send(200, "text/html", getDashboardPage());
    } else {
      server.send(200, "text/html", getWaitingPresencePage());
    }
  });

  // CRUD: Add a secret
  server.on("/add", HTTP_POST, []() {
    if (!sessionUnlocked) {
      server.sendHeader("Location", "/");
      server.send(303);
      return;
    }
    if (server.hasArg("lbl") && server.hasArg("val")) {
      prefs.begin("sec_store", false);
      int count = prefs.getInt("count", 0);
      String keyLabel = "k_" + String(count);
      String keyVal   = "v_" + String(count);
      prefs.putString(keyLabel.c_str(), server.arg("lbl"));
      prefs.putString(keyVal.c_str(), server.arg("val"));
      prefs.putInt("count", count + 1);
      prefs.end();
    }
    server.sendHeader("Location", "/vault");
    server.send(303);
  });

  // CRUD: Delete a secret (by index, shifts remaining elements)
  server.on("/del", HTTP_GET, []() {
    if (!sessionUnlocked) {
      server.sendHeader("Location", "/");
      server.send(303);
      return;
    }
    if (server.hasArg("idx")) {
      int targetIdx = server.arg("idx").toInt();
      prefs.begin("sec_store", false);
      int count = prefs.getInt("count", 0);

      if (targetIdx >= 0 && targetIdx < count) {
        // Shift remaining keys down
        for (int i = targetIdx; i < count - 1; i++) {
          String nextLbl = prefs.getString(("k_" + String(i + 1)).c_str(), "");
          String nextVal = prefs.getString(("v_" + String(i + 1)).c_str(), "");
          prefs.putString(("k_" + String(i)).c_str(), nextLbl);
          prefs.putString(("v_" + String(i)).c_str(), nextVal);
        }
        // Remove the last duplicate key
        prefs.remove(("k_" + String(count - 1)).c_str());
        prefs.remove(("v_" + String(count - 1)).c_str());
        prefs.putInt("count", count - 1);
      }
      prefs.end();
    }
    server.sendHeader("Location", "/vault");
    server.send(303);
  });

  // Manual Lock Endpoint
  server.on("/lock", HTTP_GET, []() {
    sessionUnlocked = false;
    waitingForPresence = false;
    setLed(6, 4, 0); // Dim Amber
    server.sendHeader("Location", "/");
    server.send(303);
  });

  // Captive Portal standard catch-all redirects (Android, iOS, Windows)
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302, "text/plain", "");
  });

  server.begin();
  setLed(6, 4, 0); // Dim Amber: Ready & Locked
}

// -------------------------------------------------------------
// Main Loop
// -------------------------------------------------------------

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // 1. Inactivity Session Timeout (Auto-lock after 90 seconds)
  if (sessionUnlocked && (millis() - lastActivityTime > SESSION_TIMEOUT_MS)) {
    sessionUnlocked = false;
    waitingForPresence = false;
    setLed(6, 4, 0); // Return to Dim Amber
  }

  // 2. Presence Window Timeout (15 seconds to tap button)
  if (waitingForPresence && (millis() - presenceTimer > PRESENCE_TIMEOUT_MS)) {
    waitingForPresence = false;
    setLed(6, 4, 0); // Aborted: Return to Dim Amber
  }

  // 3. Physical BOOT Button Interactions
  if (digitalRead(BOOT_BTN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BOOT_BTN) == LOW) {
      
      // Case A: Waiting for presence -> Unlock Vault
      if (waitingForPresence) {
        waitingForPresence = false;
        sessionUnlocked = true;
        lastActivityTime = millis();
        setLed(0, 15, 0); // Dim Green: Authenticated
      }
      // Case B: Already unlocked -> Hardware Panic Lock
      else if (sessionUnlocked) {
        sessionUnlocked = false;
        setLed(6, 4, 0); // Dim Amber: Locked immediately
      }

      // Wait until button is released
      while (digitalRead(BOOT_BTN) == LOW) {
        delay(20);
      }
      delay(200);
    }
  }
}