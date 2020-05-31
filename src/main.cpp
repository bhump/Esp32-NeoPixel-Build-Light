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

const char* host = "esp32";
const char *ssid = "MoBhaile-2.4";
const char *password = "beerisgood";
const String accessToken = "cbt2e3vsrcrxrabdqqemzlhgpdh7ed5sonaivnfcnqhvqkvsl7ba";
const String azureUrl = "https://dev.azure.com/BISSELL/BISSELL_IoT/_apis/build/builds?definitions=147&$top=1&api-version=5.1";
const char *baseUrl = "https://dev.azure.com";
const int httpsPort = 443;

WebServer server(80);

//NeoPixel GRBW
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

bool isConnected = false;
bool inProgress = false;
String lastStatus;
HTTPClient http;

/*
 * Login page
 */

const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
 
/*
 * Server Index Page
 */
 
const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
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
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

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

bool initializeWifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi already connected");
    return true;
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
      delay(500);
      setColor("blue");
      Serial.print(".");
      delay(500);
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
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
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
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
}

void setup()
{
  Serial.begin(115200);

  strip.begin();
  strip.show();
  strip.setBrightness(15);

  isConnected = initializeWifi();

  if(isConnected)
  {
    initializeWebServer();
  }
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

    http.addHeader("Authorization", "Basic bmFtZTp5NWZnemp0ZXdqc2k1dG5vNGR2ZTVvem9lbnVjeWh3c2NqYmp2aHo1cmxyNHB2M3lvbGZx");

    int httpCode = http.GET();

    if (httpCode > 0)
    { //Check for the returning code

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
  if (initializeWifi())
  {
    server.handleClient();
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
}