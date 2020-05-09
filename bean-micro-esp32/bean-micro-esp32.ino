#include <WiFi.h>
#include <Arduino_JSON.h>
#include <HTTPClient.h>
#include <Servo.h>

// var time
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile SemaphoreHandle_t timerSemaphore;
const int timer_period = 1000000; // (value in microseconds)

// wifi connection
const char* ssid = "Mi9x";
const char* password =  "00001111";
int connectionCount = 0;
const int max_connection_attemp = 3;

// bean-app
int beanType1Stock = 100;
int beanType2Stock = 100;
boolean getOrderFlag = false;
boolean hasOrderFlag = false;
boolean processingOrderFlag = false;

// order
int orderId;
String orderUsername;
int orderBeanType;
int orderBeanNum;
String orderTimestamp;


// var server
String defaultUrl = "http://35.240.143.70:5000";
String updateStockApiUrl = "/updateStock";
String getOrderApiUrl = "/getOrder";
String completeOrderApiUrl = "/completeOrder";
JSONVar jsonObject;


Servo myservo;
int servoPin = 5;


void setup() {

  Serial.begin(115200);
  Serial.println("ESP32 Starting...");

  // fucntion bean-app
  myservo.attach(servoPin);
  connectToWifi();
  startTimer();

  updateStockApi(1, beanType1Stock);
  updateStockApi(2, beanType2Stock);
}

void loop() {
  // debug with serial to update stock example [1,20]
  if (Serial.available()) {
    //    int beanType  = Serial.readStringUntil(',').toInt();
    //    int beanNum = Serial.readStringUntil('\0').toInt();
    //    updateStockApi(beanType, beanNum);

    //    getOrderApi();

    String aUsername = Serial.readStringUntil(',');
    int aOrderId = Serial.readStringUntil('\0').toInt();
    completeOrderApi(aUsername, aOrderId);
  }

  // bean-app
  if (!processingOrderFlag && getOrderFlag) {
    getOrderFlag = false;
    getOrderApi();
  }

  if (hasOrderFlag) {
    if (validateOrder()) {
      processingOrderFlag = true;
      processOrder();

      // debug start
      if (orderBeanType == 1) {
        beanType1Stock -= orderBeanNum;
      } else if (orderBeanType == 2) {
        beanType1Stock -= orderBeanNum;
      }
      updateStockApi(1, beanType1Stock);
      updateStockApi(2, beanType2Stock);
      // debug end

      
      processingOrderFlag = false;
      completeOrderApi(orderUsername, orderId);
      hasOrderFlag = false;

    }

  }
}

void connectToWifi() {
  delay(1000);
  String connectionInfo = "Connecting [ssid: ";
  connectionInfo.concat(ssid);
  connectionInfo.concat(", password: ");
  connectionInfo.concat(password);
  connectionInfo.concat("]");
  WiFi.begin(ssid, password);

  Serial.println("--------- Connecting Wifi ---------");
  while (WiFi.status() != WL_CONNECTED) {
    connectionCount++;
    delay(1000);
    Serial.println(connectionInfo);
    if (connectionCount == max_connection_attemp) {
      Serial.println("ESP32 Restarting..");
      ESP.restart();
    }
  }
  Serial.print("Connected [ssid: ");
  Serial.print(ssid);
  Serial.println("]");
  Serial.println("-----------------------------------");
}

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  getOrderFlag = true;
  portEXIT_CRITICAL_ISR(&timerMux);
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
}
void startTimer() {
  timerSemaphore = xSemaphoreCreateBinary();
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, timer_period, true);
  timerAlarmEnable(timer);
}

void stopTimer() {
  timerEnd(timer);
  timer = NULL;
}

void updateStockApi(int beanType, int beanNum) {
  Serial.println("----------- updateStockApi ------------");
  Serial.print("updateStockApi() url: ");
  String url = String(defaultUrl);
  url.concat(updateStockApiUrl);
  url.concat("/");
  url.concat(beanType);
  url.concat("/");
  url.concat(beanNum);
  Serial.println(url);

  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.print(httpCode);

      Serial.println(payload);
    }
    else {
      Serial.println("Error on HTTP request");
    }
    http.end();
  }
  Serial.println("-----------------------------------");
}

void getOrderApi() {
  Serial.println("---------- getOrderApi ----------");
  Serial.print("getOrderApi() url: ");
  String url = String(defaultUrl);
  url.concat(getOrderApiUrl);
  Serial.println(url);
  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.print(httpCode);

      Serial.println(payload);

      jsonObject = JSON.parse(payload);
      if (JSON.typeof(jsonObject) == "undefined") {
        hasOrderFlag = false;
        Serial.println("No order!! ------------------------");
        Serial.println("-----------------------------------");
        return;
      }

      Serial.println("Order details -----------");
      if (jsonObject.hasOwnProperty("order_id")) {
        orderId = (int) jsonObject["order_id"];

        Serial.print("order_id: ");
        Serial.println(orderId);
      }
      if (jsonObject.hasOwnProperty("username")) {
        orderUsername = (const char*) jsonObject["username"];

        Serial.print("username: ");
        Serial.println(orderUsername);
      }
      if (jsonObject.hasOwnProperty("bean_type")) {
        orderBeanType = (int) jsonObject["bean_type"];

        Serial.print("bean_type: ");
        Serial.println(orderBeanType);
      }
      if (jsonObject.hasOwnProperty("bean_num")) {
        orderBeanNum = (int) jsonObject["bean_num"];

        Serial.print("bean_num: ");
        Serial.println(orderBeanNum);
      }
      if (jsonObject.hasOwnProperty("timestamp")) {
        orderTimestamp = (const char*) jsonObject["timestamp"];

        Serial.print("timestamp: ");
        Serial.println(orderTimestamp);
      }
      hasOrderFlag = true;

    }
    else {
      Serial.println("Error on HTTP request");
    }
    http.end();
  }
  Serial.println("-----------------------------------");
}

void completeOrderApi(String username, int orderId) {
  Serial.println("--------- completeOrderApi ---------");
  Serial.print("completeOrderApi() url: ");
  String url = String(defaultUrl);
  url.concat(completeOrderApiUrl);
  url.concat("/");
  url.concat(username);
  url.concat("/");
  url.concat(orderId);
  Serial.println(url);

  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.print(httpCode);

      Serial.println(payload);
    }
    else {
      Serial.println("Error on HTTP request");
    }
    http.end();
  }
  Serial.println("-----------------------------------");
}

boolean validateOrder() {
  Serial.println("---------- validateOrder ----------");
  boolean res;
  if (orderBeanType == 1)
    res =  orderBeanNum > beanType1Stock ? false : true;
  else if (orderBeanType == 2)
    res = orderBeanNum > beanType2Stock ? false : true;
  Serial.print("validateOrder: ");
  Serial.println(res);
  Serial.println("-----------------------------------");
  return res;
}

void processOrder() {
  Serial.println("---------- processOrder -----------");
  if (orderBeanType == 1) {
    for (int i = 0; i < orderBeanNum; i++) {
      myservo.write(90);
      delay(1000);
      myservo.write(0);
      delay(1000);
    }
  }
  else if (orderBeanType == 2) {
    for (int i = 0; i < orderBeanNum; i++) {
      myservo.write(90);
      delay(1000);
      myservo.write(180);
      delay(1000);
    }
  }
  myservo.write(90);
  Serial.println("order completed!");
  Serial.println("-----------------------------------");
}
