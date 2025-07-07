#include <Arduino.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "ESPAsyncWiFiManager.h"  

//variables
AsyncWebServer server(80);
DNSServer dns;
bool onbardPinOn = false;
int TIME_INTERVAL = 86400;//86400;  //seconds  every day is 86,400
//int CHLORINE_AMT = 20;  ///NEED TO CHANGE TO WHAT IS 10 Oz run time in seconds
int elapseTime = 0;
bool dosing = false;
int dosingSecond = 0;
// Simulated sensor values
float chlorAmount = 10;
float CHLOR_CONVERSION = 0.26666;
float acidAmt = 0;
float phDown = 0;

bool ledState = false;
const int ledPin = 2; // Example GPIO pin
bool pumpState = false;
const int pumpPin = 5;


// put function declarations here:
void configModeCallback (AsyncWiFiManager *myWiFiManager);
void doseChlorine(int amt);
bool timeToDose(int elapse);
float ozToSeconds(float intput);

String processor(const String& var) {
  return String();
}

// --------------------------------------------------
// HTML + CSS + JS (as raw string)
// --------------------------------------------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Pool Dashboard</title>
  <style>
    body {
      margin: 0;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto;
      background: url('https://images.unsplash.com/photo-1504457046785-27dba1a704b4?auto=format&fit=crop&w=800&q=80') no-repeat center center fixed;
      background-size: cover;
      color: #fff;
    }
    .overlay {
      background: rgba(0, 0, 0, 0.6);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .card {
      background: rgba(255, 255, 255, 0.95);
      color: #333;
      padding: 20px;
      border-radius: 12px;
      width: 100%;
      max-width: 340px;
      box-shadow: 0 4px 20px rgba(0,0,0,0.2);
      text-align: center;
    }
    h2 {
      font-size: 22px;
      margin-bottom: 16px;
      color: #006994;
    }
    .sensor {
      font-size: 16px;
      margin: 8px 0;
    }
    input[type="number"] {
      width: 80px;
      padding: 6px;
      font-size: 16px;
      text-align: center;
      border-radius: 6px;
      border: 1px solid #ccc;
      margin-left: 6px;
    }
    .btn {
      margin-top: 10px;
      padding: 10px 20px;
      font-size: 16px;
      background: #006994;
      color: white;
      border: none;
      border-radius: 6px;
      cursor: pointer;
    }
    .btn:hover {
      background: #005377;
    }
    .switch {
      position: relative;
      display: inline-block;
      width: 50px;
      height: 28px;
    }
    .switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0; left: 0; right: 0; bottom: 0;
      background-color: #ccc;
      transition: 0.4s;
      border-radius: 28px;
    }
    .slider:before {
      position: absolute;
      content: "";
      height: 22px; width: 22px;
      left: 3px; bottom: 3px;
      background-color: white;
      transition: 0.4s;
      border-radius: 50%;
    }
    input:checked + .slider {
      background-color: #2196F3;
    }
    input:checked + .slider:before {
      transform: translateX(22px);
    }
    #ledLabel {
      font-size: 14px;
      margin-top: 6px;
    }
  </style>
</head>
<body>
  <div class="overlay">
    <div class="card">
      <h2>Pool Dashboard</h2>
      <div class="sensor">
        <strong>Chlorine:</strong>
        <input type="number" id="chlorInput" step="0.1"> Oz.
      </div>
      <div class="sensor">
        <strong>acidAmout:</strong>
        <input type="number" id="acidInput" step="0.1"> Oz.
      </div>
      <div class="sensor">
        <strong>PhDown:</strong>
        <input type="number" id="phInput" step="0.01"> Oz.
      </div>
      <button class="btn" onclick="updateVariables()">Save</button>

      <div class="toggle-switch" style="margin-top: 20px;">
        <label class="switch">
          <input type="checkbox" id="ledToggle" onclick="toggleLED()">
          <span class="slider"></span>
        </label>
        <p id="ledLabel">LED is OFF</p>
      </div>
    </div>
  </div>

  <script>
  let autoRefresh = true;
  let refreshInterval;

  async function fetchData() {
    if (!autoRefresh) return;
    const res = await fetch('/data');
    const data = await res.json();
    document.getElementById('chlorInput').value = data.chlorAmount;
    document.getElementById('acidInput').value = data.acidAmt;
    document.getElementById('phInput').value = data.phDown;
    document.getElementById('ledToggle').checked = data.ledState;
    document.getElementById('ledLabel').textContent = data.ledState ? "LED is ON" : "LED is OFF";
  }

  function toggleLED() {
    fetch('/toggle').then(fetchData);
  }

  function updateVariables() {
  const chlorAmt = document.getElementById('chlorInput').value;
  const acidAmt = document.getElementById('acidInput').value;
  const phDown = document.getElementById('phInput').value;

  autoRefresh = false;

  fetch(`/update?chlorAmt=${chlorAmt}&acidAmt=${acidAmt}&phDown=${phDown}`)
    .then(response => response.text())
    .then(text => {
      console.log("Update response:", text);
      setTimeout(() => {
        autoRefresh = true;
        fetchData();
      }, 1000); // slight delay to allow server to update
    });
}


  function stopAutoRefresh() {
    autoRefresh = false;
  }

  // Pause auto-refresh while editing
  document.addEventListener('DOMContentLoaded', function () {
    ['chlorInput', 'acidInput', 'phInput'].forEach(id => {
      const input = document.getElementById(id);
      input.addEventListener('focus', stopAutoRefresh);
    });

    fetchData();
    refreshInterval = setInterval(fetchData, 3000);
  });
</script>
</body>
</html>
)rawliteral";



void setup() {
  // put your setup code here, to run once:
    Serial.begin(115200);



  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

    ///////////////////Start WiFi ///////////////////////////////////////
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  AsyncWiFiManager wifiManager(&server, &dns);
  //reset settings - for testing
  //wifiManager.resetSettings();
  //wifiManager.setSTAStaticIPConfig(IPAddress(192,168,1,175), IPAddress(192,168,1,1), IPAddress(255,255,255,0), IPAddress(192,168,1,1), IPAddress(192,168,1,1));
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Pool_Dosing")) {
    Serial.println("failed to connect and hit timeout");
    Serial.println("restarting esp");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
  delay(50);
  //Serial.print("FreeHeap is :");
  //Serial.println(ESP.getFreeHeap());
  delay(50);
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  //server.begin();

  pinMode(2,OUTPUT);
  digitalWrite(2,LOW);
  pinMode(5,OUTPUT);
  digitalWrite(5,LOW);
  //AsyncWebServer server(90);
  Serial.println("\nConnected to WiFi. IP: " + WiFi.localIP().toString());

  // Serve HTML UI
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // JSON data endpoint
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"chlorAmount\":" + String(chlorAmount, 1) + ",";
    json += "\"acidAmt\":" + String(acidAmt, 1) + ",";
    json += "\"phDown\":" + String(phDown, 2) + ",";
    json += "\"ledState\":" + String(ledState ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });

  // Toggle LED
  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    ledState = !ledState;
    pumpState = !pumpState;
    digitalWrite(ledPin, ledState);
    digitalWrite(pumpPin, pumpState);
    request->send(200, "text/plain", "OK");
  });

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
  if (request->hasParam("chlorAmt")) {
    chlorAmount = request->getParam("chlorAmt")->value().toFloat();
  }
  if (request->hasParam("acidAmt")) {
    acidAmt = request->getParam("acidAmt")->value().toFloat();
  }
  if (request->hasParam("phDown")) {
    phDown = request->getParam("phDown")->value().toFloat();
  }
  request->send(200, "text/plain", "Variables updated");
});

  server.begin();

}

void loop() {
  // put your main code here, to run repeatedly:
  /*if(onbardPinOn){
    digitalWrite(2,LOW);
    onbardPinOn = false;
    //Serial.print("off");
  }else{
    digitalWrite(2,HIGH);
    onbardPinOn = true;
     //Serial.print("on");
  }*/
  
  delay(1000);
  if(!dosing){
    elapseTime++;
  }
  //Serial.print("1,");
  if(timeToDose(elapseTime) && dosing){
    //Serial.print("1.1,");
    //Serial.println(elapseTime);
    //Serial.println(dosing);
    doseChlorine(ozToSeconds(chlorAmount));
  }



}


// put function definitions here:
void doseChlorine(int amt){
  //Serial.print("2,");
  dosingSecond++;
  //Serial.print("dosingSecond");Serial.print(dosingSecond);
  if(dosingSecond >= amt){
    dosingSecond = 0;
    Serial.print("|");
    dosing = false;
    //elapseTime = 0;
    digitalWrite(5,LOW);
    digitalWrite(2,LOW);
    //dosingSecond = 0;
    //elapseTime = 0;
  }else{
    //Serial.print("3.2,");
    digitalWrite(5,HIGH);
    digitalWrite(2,HIGH);
    Serial.print("->");
  }
}

bool timeToDose(int elapse){
  if(elapse >= TIME_INTERVAL){
    //Serial.print("2.1,");
    elapseTime = 0;
    dosing =true;
  }else{
    //Serial.print("2.2,");
    //dosing = false;
  }
  return dosing;
}

/////////////////////////////////////////////////////////////
void configModeCallback (AsyncWiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  //myWiFiManager->startConfigPortal("ATOAWC");addDailyDoseAmt
  //myWiFiManager->autoConnect("DOSER");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());

}

float ozToSeconds(float input){
  return input/CHLOR_CONVERSION;
}