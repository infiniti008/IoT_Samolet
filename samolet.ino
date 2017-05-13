#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <AsyncDelay.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "FS.h"


//++ Взятие температуры
  #define ONE_WIRE_BUS D5
  OneWire oneWire(ONE_WIRE_BUS);
  DallasTemperature sensors(&oneWire);
  byte temperature = 10; //Обновляемая переменная температуры
  byte sensor_do_time;// = 250; //Период считывания температуры с датчика
  AsyncDelay delay_sensor_do;
//--
//++ Мигание светодиода температуры
  AsyncDelay delay_temp_mig;
  AsyncDelay delay_temp_do;
  byte mig_temp_col = 10;
  byte mig_temp_count = 0;
  byte mig_temp_stat = 0;
  byte mig_temp_time;// = 64;//Период запуска процесса моргания светодиодом температуры
  byte led_temp = D0;
//--
//++ Мигание светодиода wifi
  AsyncDelay delay_wifi_mig;
  AsyncDelay delay_wifi_do;
  byte mig_wifi_col = 0;
  byte mig_wifi_count = 0;
  byte mig_wifi_stat = 0;
  byte mig_wifi_time;// = 29;//Период запуска процесса моргания светодиодом статуса wifi
  byte led_wifi = D1;
//--

String new_ssid;// = "slava_asus";
String new_pass;// = "11111111";
char *ssid_ap = "espmcu2"; // ssid to access point

byte n = 0; // Счетчик времени подключения к сети
byte tim = 35; //Время ожидания подключения к сети
byte wifi_stat=2; //0 - ap, 1 - client

ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  SPIFFS.begin();
  read_conf();
  //++ Старт обработки датчика температуры
    sensors.begin();
    get_temp();
    delay_sensor_do.start(sensor_do_time*1000, AsyncDelay::MILLIS);
  //--  
  //++ Мигание светодиода температуры
    pinMode(led_temp, OUTPUT); // Пин для светодиода ТЕМПЕРАТУРЫ
    run_mig_temp();
    delay_temp_do.start(mig_temp_time * 1000, AsyncDelay::MILLIS);
  //--
  //++ Мигание светодиода wifi
    pinMode(led_wifi, OUTPUT); // Пин для светодиода WIFI
    run_mig_wifi();
    delay_wifi_do.start(mig_wifi_time*1000, AsyncDelay::MILLIS);
  //--

  
  Serial.println("Booting");
  // Connect to your WiFi network
  wi_setup();
  //++ OTA update
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp_mcu_1");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready to OTA update");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //-- End OTA update
}

void loop() {
  //++ Мигание светодиода температуры
    miganie_temp();
  //--
  //++ Мигание светодиода температуры
    miganie_wifi();
  //--
  //++ Listen to OTA update
    ArduinoOTA.handle();
  //--
  //++ Listen for http requests
    server.handleClient();  
  //--
  //++ Взятие температуры с сенсора
    if (delay_sensor_do.isExpired()) {
      get_temp();
      delay_sensor_do.repeat();
    }
  //--
}

//++ Функция подключения к wifi
void wi_setup(){
  read_wifi();
  char ssid_con[new_ssid.length() + 1];
  char password_con[new_pass.length() + 1];
  new_ssid.toCharArray(ssid_con, new_ssid.length() + 1);
  new_pass.toCharArray(password_con, new_pass.length() + 1);
  Serial.print("'");
  Serial.print(ssid_con);
  Serial.println("'");
  Serial.print("'");
  Serial.print(password_con);
  Serial.println("'");

  
  // Constant
  n = 0;
  // Connect to your WiFi network
  WiFi.mode(WIFI_STA);
  String myHostname = "foo";
  WiFi.hostname(myHostname);
  WiFi.begin(ssid_con, password_con);
  Serial.print("Connecting to ");
  Serial.print(ssid_con);

  // Wait for successful connection to ssid_con
  while (WiFi.status() != WL_CONNECTED && n < tim) {
    delay(500);
    Serial.print(".");
    n=n+1;
  }
  if (WiFi.status() == WL_CONNECTED){
    Serial.println();
    Serial.print("ESP IP Adress: ");
    Serial.println(WiFi.localIP());
    wifi_stat = 1;
    start_server();
  }
  Serial.println();
  // Set up access point
  if(n == tim){
    wifi_stat = 0;
    delay(1000);
    Serial.println("Configuring access point...");
    /* You can remove the password parameter if you want the AP to be open. */
    WiFi.softAP(ssid_ap);
    Serial.print("SSID to connect - ");
    Serial.println(ssid_ap);

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    start_server();
  }
}
//--
//++ Функция старта веб-сервера
  void start_server(){
    //Page - home
    server.on("/", Handle_home);
    server.on("/get_temperature", [](){
      String temp_to_get = String(temperature);
      server.send(200, "text/html", temp_to_get);
    });

    server.on("/test_write", write_conf);
    server.on("/read_conf", read_conf);
    server.on("/read_wifi", read_wifi);
    
    server.on("/cunfigure_wifi", Handle_cunfigure_wifi);
    server.on("/cunfigure_time", Handle_cunfigure_time);
    server.on("/format_spiffs", Handle_format_spiffs);
    server.begin();
    Serial.println("HTTP server started");
    server.serveStatic("/", SPIFFS, "/", "max-age=300"); //86400
  }
//--
//++ Фукции роутера
  void Handle_home(){
    String s = "File open filed";
    File f = SPIFFS.open("/home.html", "r");
    if (!f) {
        Serial.println("file open failed");
    }
    else{
      s = f.readStringUntil('^');
    }
    s.replace("{{temp}}", String(temperature));
    
    s.replace("{{ssid}}", String(new_ssid));
    s.replace("{{pass}}", String(new_pass));
    s.replace("{{delay_get_temp}}", String(sensor_do_time));
    s.replace("{{delay_led_temp}}", String(mig_temp_time));
    s.replace("{{delay_led_wifi}}", String(mig_wifi_time));
    
    server.send(200, "text/html", s);
    f.close();
  }
  void Handle_cunfigure_wifi(){
    server.send(200, "text/html", "we change wifi config");
    if(server.arg("ssid") != ""){
      new_ssid = server.arg("ssid");
//      server.arg("ssid").toCharArray(ssid_con, server.arg("ssid").length() + 1);
    }
    if(server.arg("pass") != ""){
      new_pass = server.arg("pass");
//      server.arg("pass").toCharArray(password_con, server.arg("pass").length() + 1);
    }
    Serial.println("new conf");
    Serial.println(new_ssid);
    Serial.println(new_pass);
    write_wifi();
    server.stop();
    delay(200);
    WiFi.disconnect();
    delay(200);
    WiFi.mode(WIFI_OFF);
    delay(200);
  
    //Restart connection
    wi_setup();
  }
  void Handle_cunfigure_time(){
    server.send(200, "text/html", "We change time config");
    if(server.arg("delay_get_temp") != ""){
      sensor_do_time = server.arg("delay_get_temp").toInt();
      delay_sensor_do.start(sensor_do_time*1000, AsyncDelay::MILLIS);
    }
    if(server.arg("delay_led_temp") != ""){
      mig_temp_time = server.arg("delay_led_temp").toInt();
      delay_temp_do.start(mig_temp_time * 1000, AsyncDelay::MILLIS);
    }
    if(server.arg("delay_led_wifi") != ""){
      mig_wifi_time = server.arg("delay_led_wifi").toInt();
      delay_wifi_do.start(mig_wifi_time*1000, AsyncDelay::MILLIS);
    }
    write_conf();
  }
  void Handle_format_spiffs(){
    server.send(200, "text/html", "We format SPIFFS. Please wait 30 secs for SPIFFS to be formatted");
    Serial.println("Please wait 30 secs for SPIFFS to be formatted");
    SPIFFS.format();
    Serial.println("Spiffs formatted");
  }
//--


void get_temp(){
  // call sensors.requestTemperatures() to issue a global temperature 
  // request to all devices on the bus
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
  // After we got the temperatures, we can print them here.
  // We use the function ByIndex, and as an example get the temperature from the first sensor only.
  Serial.print("Temperature for the device 1 (index 0) is: ");
  temperature = sensors.getTempCByIndex(0);
  Serial.print("temperature = ");
  Serial.println(temperature);
  server.send(200, "text/html", "cfhcvb");

}

//++ Мигание светодиода температуры
  void run_mig_temp(){
    mig_temp_count = 0;
    mig_temp_stat = 1;
    mig_temp_col = temperature;
    Serial.print("mig_temp_col = ");
    Serial.println(mig_temp_col);
    delay_temp_mig.start(500, AsyncDelay::MILLIS);
    delay_temp_do.repeat();
  }
  void miganie_temp(){
    if (delay_temp_do.isExpired()) {
      run_mig_temp();
    }
    if(mig_temp_stat == 1){
      if (delay_temp_mig.isExpired()) {
        if(mig_temp_count%2 == 0){
          digitalWrite(led_temp, 1);
          Serial.print((mig_temp_count/2) + 1);
          Serial.println(" - temp up");
        }
        else if(mig_temp_count%2 != 0){
          digitalWrite(led_temp, 0);
//          Serial.println("temp down");
        }
        mig_temp_count = mig_temp_count + 1;
        if(mig_temp_count < mig_temp_col * 2){
          delay_temp_mig.repeat(); 
        }
        else{
          mig_temp_stat = 0;
        }
      }
    }
  }
//--
//++ Мигание светодиода wifi
  void run_mig_wifi(){
    mig_wifi_count = 0;
    mig_wifi_stat = 1;
    mig_wifi_col = WiFi.status();
    Serial.print("mig_wifi_col = ");
    Serial.println(mig_wifi_col);
    delay_wifi_mig.start(500, AsyncDelay::MILLIS);
    delay_wifi_do.repeat();
  }
  void miganie_wifi(){
    if (delay_wifi_do.isExpired()) {
      run_mig_wifi();
    }
    if(mig_wifi_stat == 1){
      if (delay_wifi_mig.isExpired()) {
        if(mig_wifi_count%2 == 0){
          Serial.print((mig_wifi_count/2) + 1);
          Serial.println(" - wifi up");
          digitalWrite(led_wifi, 1);
        }
        else if(mig_wifi_count%2 != 0){
//          Serial.println("wifi down");
          digitalWrite(led_wifi, 0);
        }
        mig_wifi_count = mig_wifi_count + 1;
        if(mig_wifi_count < mig_wifi_col * 2){
          delay_wifi_mig.repeat(); 
        }
        else{
          mig_wifi_stat = 0;
        }
      }
    }
  }
  //1 - Связь не установлена
  //3 - Подключено к указанной сети
  //6 - wi-fi не подключена
  //4 - В режиме точки доступа
//--

 void write_wifi(){
    server.send(200, "text/html", "write wifi conf success");
    //Первый файл - SSID
    File ssid = SPIFFS.open("/ssid.txt", "w");
    if (!ssid) {
      Serial.println("file creation failed");
    }
    ssid.print(new_ssid);
    ssid.close();
    //Второй файл - Password
    File pass = SPIFFS.open("/pass.txt", "w");
    if (!pass) {
      Serial.println("file creation failed");
    }
    pass.print(new_pass);
    pass.close();
}

void read_wifi(){
  server.send(200, "text/html", "read wifi conf success");
  File ssid = SPIFFS.open("/ssid.txt", "r");
    if (!ssid) {
        Serial.println("File doesn't exist yet");
    } 
    else {
      new_ssid = ssid.readString();
    } 
    ssid.close();
    File pass = SPIFFS.open("/pass.txt", "r");
    if (!pass) {
        Serial.println("File doesn't exist yet");
    } 
    else {
      new_pass = pass.readString();
    } 
    pass.close();
}

 void write_conf(){
  server.send(200, "text/html", "write time conf success");
  File conf = SPIFFS.open("/param.txt", "w");
    if (!conf) {
      Serial.println("file creation failed");
    }
    //Первая строка - период сканирования температуры
    conf.println(String(sensor_do_time));
    //Вторая строка - период моргания светодиода температуры
    conf.println(String(mig_temp_time));
    //Третья строка - период моргания светодиода WiFi
    conf.println(String(mig_wifi_time));
    conf.close();
}
void read_conf(){
  server.send(200, "text/html", "read time conf success");
  File conf = SPIFFS.open("/param.txt", "r");
    if (!conf) {
        Serial.println("File doesn't exist yet");
    } else {
      for (int i=1; i<=3; i++){
        String str=conf.readStringUntil('\n');
        if(i == 1){
          sensor_do_time = str.toInt();
          Serial.print("sensor_do_time");
          Serial.print(" : ");
          Serial.println(sensor_do_time);
        }
        else if(i == 2){
          mig_temp_time = str.toInt();
          Serial.print("mig_temp_time");
          Serial.print(" : ");
          Serial.println(mig_temp_time);
        }
        else if(i == 3){
          mig_wifi_time = str.toInt();
          Serial.print("mig_wifi_time");
          Serial.print(" : ");
          Serial.println(mig_wifi_time);
        }
      }
    }
  conf.flush();
  conf.close();
}
