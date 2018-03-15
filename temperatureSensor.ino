#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#define MAX_ERROR_COUNT 2000

struct temperatureSensorState {
  int temperature;
  String temperature_scale;
  unsigned int update_period;
};

temperatureSensorState state = {0, "fahrenheit", 10};

WiFiClientSecure client;
unsigned long error_count = 0;
int temperature;

/*************** MODIFY THESE LINES ***************/

const char* ssid = "BYU-WiFi";//"WIFI SSID";
const char* password = "";//"WIFI PASSWORD";
const char* host = "d1jnz5vf15.execute-api.us-west-2.amazonaws.com";//"API HOST"; // ex) "api.api.com"
const char* path = "/dev/temperatureSensor/6d6f37fbae8947979cd6a95098649875";//"DEVICE PATH"; // ex) "/dev/hvac/"

/*************** Setup & Loop ***************/

void setup() {
  Serial.begin(4800);
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  connectToWifi();
}

void loop() {
  readTemperature();
  sendTemperature();
  if(error_count < MAX_ERROR_COUNT) {
    if(!sendTemperature()) {
      error_count++;  
    }
  }
  else {
    Serial.println("Error sending data");
  }
  
  delay_seconds(state.update_period);
}

/*************** Helper Functions ***************/
void delay_seconds(unsigned int seconds) {
  unsigned long ms = 1000L * seconds;
  delay(ms);
}

void connectToWifi() {
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void readTemperature() {
  //getting the voltage reading from the temperature sensor
  int reading = analogRead(A0);  
  
  // converting that reading to voltage, for 3.3v arduino use 3.3
  float voltage = reading * 3.3;
  voltage /= 1024.0;
  // print out the voltage
  Serial.print(voltage); Serial.println(" volts");
  
  // now print out the temperature
  float temperatureC = (voltage - 0.5) * 100 ;  //converting from 10 mv per degree wit 500 mV offset
                                               //to degrees ((voltage - 500mV) times 100)
  Serial.print(temperatureC); Serial.println(" degrees C");
  
  // now convert to Fahrenheit
  float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
  Serial.print(temperatureF); Serial.println(" degrees F");

  if(state.temperature_scale == "fahrenheit") {
    temperature = (int)temperatureF;
  }
  else {
    temperature = (int)temperatureC;
  }
}

bool sendTemperature() {
  // Connect with TLS
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, 443)) {
    Serial.println("connection failed");
    return false;
  }

  // Send Request
  String body = "{\"temperature\": " + String(temperature) + "}\r\n";
  int length = body.length();
  Serial.print("requesting URL: ");
  Serial.println(path);
  client.print(String("PUT ") + String(path) + " HTTP/1.1\r\n");
  client.print("Host: " + String(host) + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-length: " + String(length) + "\r\n\r\n");
  client.print(body);
  Serial.println("request sent");

  // Get Response
  // Skip Headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  // Read body
  String json = "";
  while (client.available()) {
    json += client.readStringUntil('\r');
  }
  Serial.println("Got data:");
  Serial.println(json);

  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);
  if(!root.success()) {
    Serial.println("JSON PARSING FAILED");
    return false;
  }
  Serial.println("JSON PARSED");

  // Verify and update state
  JsonVariant temperature = root["temperature"];
  if(!temperature.success() || !temperature.is<int>() || temperature != temperature.as<int>()) return false;
  state.temperature = temperature.as<int>();
  Serial.println("temp updated");
  
  JsonVariant temperature_scale = root["temperature_scale"];
  if(!temperature_scale.success() || !temperature_scale.is<char*>()) return false;
  String scale = temperature_scale.as<String>();
  if(scale != "fahrenheit" && scale != "celsius") return false;
  state.temperature_scale = scale;
  Serial.println("scale updated");
  
  JsonVariant update_period = root["update_period"];
  if(!update_period.success() || !update_period.is<unsigned int>()) return false;
  state.update_period = update_period.as<unsigned int>();
  Serial.println("update_period updated");
  return true;
}
