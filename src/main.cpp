#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <HTTPClient.h>
#include <Base64.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#define LED_PIN 2
#define LED_COUNT 12
#define BRIGHTNESS 50
#define MAXIMUM_BRIGHTNESS 255
#define MEDIUM_BRIGHTNESS 130
#define LOW_BRIGHTNESS 100

int maxBrightness = 255;
int minBrightness = 80;

const char *host = "esp32";
const char *ssid = "MoBhaile-2.4";
const char *password = "beerisgood";
const String accessToken = "cbt2e3vsrcrxrabdqqemzlhgpdh7ed5sonaivnfcnqhvqkvsl7ba";
//roasted.app
const String azureUrl = "https://dev.azure.com/lonelysasquatch/roasted/_apis/build/builds?definitions=2&$top=1&api-version=5.1";
const char *baseUrl = "https://dev.azure.com";
const int httpsPort = 443;

//NeoPixel GRB
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

IPAddress local_IP(192, 168, 1, 231);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

unsigned long previousMillis = 0; // will store last time LED was updated
const long interval = 1000;       // interval at which to blink (milliseconds)

WebServer server(80);
bool isConnected = false;
bool inProgress = false;
String lastStatus;
HTTPClient http;

/* Style */
String style =
    "<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
    "input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
    "#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
    "#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
    "form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
    ".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

/* Login page */
String loginIndex =
    "<form name=loginForm>"
    "<h1>BISSELL Login</h1>"
    "<input name=userid placeholder='User ID'> "
    "<input name=pwd placeholder=Password type=Password> "
    "<input type=submit onclick=check(this.form) class=btn value=Login></form>"
    "<script>"
    "function check(form) {"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{window.open('/serverIndex')}"
    "else"
    "{alert('Error Password or Username')}"
    "}"
    "</script>" +
    style;

/* Server Index Page */
String serverIndex =
    "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
    "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
    "<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
    "<label id='file-input' for='file'>   Choose file...</label>"
    "<input type='submit' class=btn value='Update'>"
    "<br><br>"
    "<div id='prg'></div>"
    "<br><div id='prgbar'><div id='bar'></div></div><br></form>"
    "<script>"
    "function sub(obj){"
    "var fileName = obj.value.split('\\\\');"
    "document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
    "};"
    "$('form').submit(function(e){"
    "e.preventDefault();"
    "var form = $('#upload_form')[0];"
    "var data = new FormData(form);"
    "$.ajax({"
    "url: '/update',"
    "type: 'POST',"
    "data: data,"
    "contentType: false,"
    "processData:false,"
    "xhr: function() {"
    "var xhr = new window.XMLHttpRequest();"
    "xhr.upload.addEventListener('progress', function(evt) {"
    "if (evt.lengthComputable) {"
    "var per = evt.loaded / evt.total;"
    "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
    "$('#bar').css('width',Math.round(per*100) + '%');"
    "}"
    "}, false);"
    "return xhr;"
    "},"
    "success:function(d, s) {"
    "console.log('success!') "
    "},"
    "error: function (a, b, c) {"
    "}"
    "});"
    "});"
    "</script>" +
    style;

void setColor(String color)
{
  strip.clear();
  strip.show();
  if (color == "green")
  {
    strip.fill(strip.Color(0, 255, 0), 0, strip.numPixels());
  }
  else if (color == "red")
  {
    strip.fill(strip.Color(255, 0, 0), 0, strip.numPixels());
  }
  else if (color == "yellow")
  {
    strip.fill(strip.Color(255, 255, 0), 0, strip.numPixels());
  }
  else if (color == "blue")
  {
    strip.fill(strip.Color(0, 0, 255), 0, strip.numPixels());
  }
  else if (color == "white")
  {
    strip.fill(strip.Color(255, 255, 255), 0, strip.numPixels());
  }
  else if (color == "orange")
  {
    strip.fill(strip.Color(255, 165, 255), 0, strip.numPixels());
  }
  else if (color == "none")
  {
    strip.clear();
  }

  strip.show();
}

void blinkUpdate()
{
  bool isOn = false;

  if (isOn == true)
  {
    isOn = false;
    setColor("blue");
  }
  else
  {
    isOn = true;
    setColor("none");
  }
}

bool initializeWifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi already connected");
    return true;
  }

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  for (uint8_t i = 0; i < 100; i++)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      // blink the leds while connecting to WiFi
      setColor("none");
      delay(800);
      setColor("blue");
      Serial.print(".");
      delay(800);
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    setColor("white");
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address  ");
    Serial.println(WiFi.localIP());
    return true;
  }

  setColor("none");
  WiFi.disconnect();
  return false;
}

void initializeWebServer()
{
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host))
  { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1)
    {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on(
      "/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart(); }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) 
    {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN))
       { //start with max available size
        Update.printError(Serial);
      }
    } 
    else if (upload.status == UPLOAD_FILE_WRITE)
     {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) 
      {
        Update.printError(Serial);
      }
    } 
    else if (upload.status == UPLOAD_FILE_END) 
    {
      if (Update.end(true)) 
      { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } 
      else 
      {
        Update.printError(Serial);
      }
    } });
  server.begin();
}

void setup()
{
  Serial.begin(115200);

  strip.begin();
  strip.show();
  strip.setBrightness(15);

  isConnected = initializeWifi();
  
  initializeWebServer();
  
}

void getStatus()
{
  try
  {
    char inputString[] = "name:cbt2e3vsrcrxrabdqqemzlhgpdh7ed5sonaivnfcnqhvqkvsl7ba";
    int inputStringLength = sizeof(inputString);

    int encodedLength = Base64.encodedLength(inputStringLength);
    char encodedString[encodedLength];
    Base64.encode(encodedString, inputString, inputStringLength);
    Serial.print("Encoded string is:\t");
    Serial.println(encodedString);

    String encoded = String(encodedString);

    Serial.print("Real Encoded String:\t");
    Serial.println(encoded);

    http.begin(azureUrl);

    //roasted.app
    //http.addHeader("Authorization", "Basic bmFtZTpjYnQyZTN2c3JjcnhyYWJkcXFlbXpsaGdwZGg3ZWQ1c29uYWl2bmZjbnFodnFrdnNsN2Jh");

    //bissell mobile
    http.addHeader("Authorization", "Basic bmFtZTp5NWZnemp0ZXdqc2k1dG5vNGR2ZTVvem9lbnVjeWh3c2NqYmp2aHo1cmxyNHB2M3lvbGZx");

    int httpCode = http.GET();

    if (httpCode > 0)
    {

      String payload = http.getString();
      Serial.println(httpCode);
      Serial.println(payload);

      const size_t capacity = 3 * JSON_ARRAY_SIZE(0) + 2 * JSON_ARRAY_SIZE(1) + 2 * JSON_OBJECT_SIZE(0) + 13 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 3 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + 3 * JSON_OBJECT_SIZE(7) + 2 * JSON_OBJECT_SIZE(8) + JSON_OBJECT_SIZE(10) + JSON_OBJECT_SIZE(33) + 5410;
      DynamicJsonDocument doc(capacity);

      deserializeJson(doc, payload);
      JsonObject value_0 = doc["value"][0];

      const char *status = value_0["status"]; // "completed"
      const char *result = value_0["result"]; // "succeeded"

      Serial.println(status);
      Serial.println(result);

      String statusString = String(status);
      String resultString = String(result);

      if (lastStatus == statusString)
      {
        Serial.print("Last Status: ");
        Serial.println(lastStatus);

        delay(20000);

        return;
      }

      if (statusString == "inProgress")
      {
        inProgress = true;
        setColor("yellow");
        Serial.println("Build In Progress");
        lastStatus = statusString;
      }
      else if (statusString == "completed" && resultString == "succeeded")
      {
        inProgress = false;
        setColor("green");
        Serial.println("Build Successful");
        lastStatus = statusString;
      }
      else if (resultString == "completed" && resultString == "failed")
      {
        inProgress = false;
        setColor("red");
        Serial.println("Build Failed");
        lastStatus = statusString;
      }
    }
    else
    {
      Serial.println("Error on HTTP request");
    }

    http.end();
  }
  catch (const std::exception &e)
  {
    Serial.println("it borked");
  }

  delay(20000);
}

void TestColors()
{
  setColor("blue");
  delay(5000);
  setColor("green");
  delay(5000);
  setColor("yellow");
  delay(5000);
  setColor("red");
  delay(5000);
  setColor("orange");
  delay(5000);
}

uint8_t reconnects = 0;
void loop()
{
  server.handleClient();

  if (initializeWifi())
  {
    Serial.println("Getting Status");
    getStatus();
  }
  else
  {
    Serial.println("Trying to Reconnect to WiFi");
    lastStatus = "balls";
    reconnects = reconnects + 1;
    if (reconnects == 100)
    {
      ESP.restart();
    }

    delay(5000);
  }

  // unsigned long currentMillis = millis();
  // if (currentMillis - previousMillis >= interval)
  // {

  //   // // save the last time you blinked the LED
  //   previousMillis = currentMillis;
  //   //blinkUpdate();

  //   // if the LED is off turn it on and vice-versa:
  //   ledState = not(ledState);
  //   // set the LED with the ledState of the variable:
  //   digitalWrite(led, ledState);
  // }
}