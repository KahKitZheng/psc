#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

#define DHTPIN 3          // Define the pin the sensor is connected to, three-pin uses 2 & four-pin uses 3
#define DHTTYPE DHT11     // Define the sensor used i.e. (DHT11, DHT21, DHT22)
DHT dht(DHTPIN, DHTTYPE); // Create an instance of DHT

LiquidCrystal_I2C lcd(0x27, 16, 2); // Some 16x2 LCD display uses 0x3F

ESP8266WebServer server(80);
String ssid = "ssid";
String password = "password";

WiFiClient client;
char servername[] = "api.openweathermap.org"; // Remote server we will connect to
String APIKEY = "APIKEY";
String CITYID = "CITYID";

float roomHumidity, roomTemperature; // Values read from sensor
unsigned long previousMillis = 0;    // Store the last temp that was read
const long interval = 2000;          // Interval at which to read sensor, 2 seconds is the shortest

int button, buttonState, oldButtonState, state;

String result, weatherDescription, weatherLocation, Country;
float cityTemperature, cityHumidity;

void setup(void) {
  pinMode(button, INPUT); // Push button

  lcd.init(); // Initializing the LCD
  lcd.backlight();

  Serial.begin(115200);
  dht.begin(); // Initialize temperature & humidity sensor

  WiFi.begin(ssid, password); // Connect to WiFi network
  Serial.print("\n\r \n\rWorking to connect");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("DHT Weather Reading Server");
  Serial.println("Connected to " + ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //Rest API for sensor data
  server.on("/", []() {
    String response = "Welcome to weather server, open /temp, /humidity or /api";
    server.send(200, "text/plain", response);
  });

  server.on("/temp", []() {
    getTemperature();
    String response = "{\"Temperature\":" + String((int)roomTemperature) + "}";
    server.send(200, "application/json", response);
  });

  server.on("/humidity", []() {
    getTemperature();
    String response = "{\"Humidity\":" + String((int)roomHumidity) + "}";
    server.send(200, "application/json", response);
  });

  server.on("/api", []() {
    getTemperature();
    String response = "{\"temperature\":" + String((int)roomTemperature) + ",\"humidity\":" + String((int)roomHumidity) + "}";
    server.send(200, "application/json", response);
  });

  server.begin();
  Serial.println("HTTP server started \n");
}

void loop(void) {
  server.handleClient();

  buttonState = digitalRead(button); // button is the GPIO number of the button is connected to, in my case it's 0
  if (buttonState != oldButtonState) {
    oldButtonState = buttonState;

    if (buttonState == HIGH) {
      state = !state;
    }
  }

  if (state) {
    getTemperature();
    String ts = "Temperature: " + String((int)roomTemperature) + "C";
    String hs = "Humidity:    " + String((int)roomHumidity) + "%";

    lcd.setCursor(0, 0);
    lcd.print(ts);
    lcd.setCursor(0, 1);
    lcd.print(hs);
  } else {
    displayGettingData();
    delay(1000);
    getWeatherData();
    displayWeather(weatherLocation, weatherDescription);
    delay(5000);
    displayConditions(weatherLocation, cityTemperature, cityHumidity);
    delay(5000);
    state = !state;
    lcd.clear();
  }
}

void getTemperature() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    roomTemperature = dht.readTemperature(false); // Read room temperature as Celsius
    roomHumidity = dht.readHumidity();            // Read room humidity (percent)

    if (isnan(roomHumidity) || isnan(roomTemperature)) {
      roomTemperature = 0;
      roomHumidity = 0;
    }
  }
}

void displayGettingData() {
  lcd.clear();
  lcd.print("Getting data");
}

void getWeatherData() {
  if (client.connect(servername, 80)) {
    client.println("GET /data/2.5/weather?id=" + CITYID + "&units=metric&APPID=" + APIKEY);
    client.println("Host: api.openweathermap.org");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();
  } else {
    Serial.println("connection failed"); // Error message if no client connect
    Serial.println();
  }

  while (client.connected() && !client.available())
    delay(1); // Waits for data

  //connected or data available
  while (client.connected() || client.available()) {
    char c = client.read(); // Gets byte from ethernet buffer
    result = result + c;
  }

  client.stop(); // Stop client
  result.replace('[', ' ');
  result.replace(']', ' ');
  Serial.println(result);
  char jsonArray[result.length() + 1];
  result.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';
  StaticJsonBuffer<1024> json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);

  if (!root.success()) {
    Serial.println("parseObject() failed");
  }

  String location = root["name"];
  String country = root["sys"]["country"];
  float temperature = root["main"]["temp"];
  float humidity = root["main"]["humidity"];
  String weather = root["weather"]["main"];
  String description = root["weather"]["description"];

  weatherDescription = description;
  weatherLocation = location;
  Country = country;
  cityTemperature = temperature;
  cityHumidity = humidity;
}

void displayWeather(String location, String description) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(String(location) + ", " + String(Country));
  lcd.setCursor(0, 1);
  lcd.print(description);
}

void displayConditions(String location, float cityTemperature, float Humidity) {
  lcd.clear();
  lcd.print(String(location) + ", " + String(Country));
  lcd.setCursor(0, 1);
  lcd.print("T: " + String((int)cityTemperature) + String((char)223) + "C");
  lcd.print("   ");
  lcd.print("H: " + String((int)Humidity) + "%");
}