#include <WiFi.h>
#include <ESP32Servo.h>
#include <WebServer.h>

const char *ssid = "AP01-01";
const char *password = "1qaz2wsx";
WebServer server(80);

//IOpin OUT-A(26), OUT-B(13), IN-A(33), IN-B(32)
#define ledPin   13 //LED out
#define motorPin 26 //Servo out
#define cdsPin 32   //Light sensor in
#define sensorPin 33 //Moisture

Servo myservo; 

void setup() {
  Serial.begin(115200);
  ledcSetup(2, 12800, 8);   // ledcSetup(channel, frequency, number of bits)
  ledcAttachPin(ledPin, 2); // Connect ledPin to channel 0
  myservo.attach(motorPin, 700, 2300);
  pinMode(cdsPin, INPUT);
  pinMode(sensorPin, INPUT);

//WiFi initialize
  Serial.println("initialize");
  Serial.println("WiFi connecting...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println();
  Serial.printf("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("WiFi connected!");

//WebServer準備
  server.on("/", handleSample);
  server.onNotFound(handleNotFound);
//WebServer起動
  server.begin();

//NTP initialize
  configTime(9 * 3600L, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");
  delay(5000);    //3000以下の場合、プログラム書き込みに続く起動で時刻の取得に失敗し。１度リセットが必要
}

//グローバル変数
char rday[10];
char rtime[10];
int  c_onTime[3];    //照明・給水の制御開始時間
int  c_offTime[3];   //照明・給水の制御終了時間
float lux; 
float lux_max = 80.00;  //照明閾値(max)
float lux_min = 50.00;  //照明閾値(min)
int led_pwm = 255; 
float sensorValue = 0;
float moisture = 0;
float mist_max = 80;    //水分閾値(max)
float mist_min = 30;    //水分閾値(min)
int valv_sts = 0;           //0:valve off,1:valve on


void loop() {
  server.handleClient();

//Get date & time from FTP
  struct tm timeInfo;   //tm型の構造体

  // char rday[10];
  // char rtime[10]; 

  getLocalTime(&timeInfo);  //ローカルタイム取得

  sprintf(rday, "%04d/%02d/%02d",
          timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday);
  //Serial.print(rday);
  //Serial.print(" ");

  sprintf(rtime, "%02d:%02d:%02d",
          timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
  Serial.print(rtime);
  Serial.print("/ ");

//時刻の秒換算
  int hhmmss2ss = 0;
  hhmmss2ss = timeInfo.tm_hour *3600 + timeInfo.tm_min *60 + timeInfo.tm_sec;
  // Serial.print("hhmmss2ss:");
  // Serial.println(hhmmss2ss);

//ライトオン時間ステータス
  int onTime[3] = {06, 00, 00};       //日中スタート時間、onTime[0]:hh,[1]:mm,[2]:ss 
  int onTim2ss = onTime[0] *3600 + onTime[1] *60 + onTime[2];
  int offTime[3] = {18, 00, 00};     //夜間スタート時間、offTime[0]:hh,[1]:mm,[2]:ss 
  int offTim2ss = offTime[0] *3600 + offTime[1] *60 + offTime[2];

  for (int i = 0; i < 3; i++) {
    c_onTime[i] = onTime[i];
  }
  for (int i = 0; i < 3; i++) {
    c_offTime[i] = offTime[i];
  }

  int day_sts = 0;             //0:night, 1:day
  if ((onTim2ss <= hhmmss2ss) && (hhmmss2ss < offTim2ss)) {
    day_sts = 1;
  } else {
    day_sts = 0;
  }
  // Serial.print("hhmmss2ss:");
  // Serial.print(hhmmss2ss);
  // Serial.print("/");
  // Serial.print("onTim2ss:");
  // Serial.print(onTim2ss);
  // Serial.print("/");
  // Serial.print("offTim2ss:");
  // Serial.print(offTim2ss);
  // Serial.print("/");

  Serial.print("day_sts:");
  Serial.print(day_sts);
  Serial.print("/ ");

//Read Light sensor
  float cds_ad = analogRead(cdsPin); // Read analog data
  float cds_v = cds_ad * 3.3 / 4095; // Calculation of voltage value
  lux = 10000 * cds_v / (3.3 - cds_v) / 1000; // Calculation of lux value
  Serial.print(lux);
  Serial.print("(Lux)");
  Serial.print("/ ");

//Read Moisture sensor
  sensorValue = analogRead(sensorPin);
  moisture = sensorValue / 2224 * 100; //初期値2224。水につけてanalogReadした値。
  //Serial.print("moisture:");
  Serial.print(moisture);
  Serial.print("(%)");
  Serial.println("/ ");


//++++++++++++++++++++++++++++++++++++++++++++++++++
//機器制御の制御
  switch (day_sts) {
    case (0):       //night time
    //LED control
      ledcWrite(2, 255);  //Lightオフ
    //Servo control
      //myservo.write(1000);
      valv_sts = 0;
      break;
    case (1):       //day time
    //LED control
      if (lux < lux_min) {
        led_pwm -= 50;
        if (led_pwm < 0) {
          led_pwm = 0;
        }
      }
      if (lux > lux_max) {
        led_pwm += 50;
        if (led_pwm > 255) {
          led_pwm = 255;
        }
      }
      ledcWrite(2, led_pwm);
    //Servo control
      if (moisture < mist_min) {
        if (valv_sts == 0) {
          myservo.write(2000);
          delay(500);
          myservo.write(1500);
          valv_sts = 1;
        }
      } else if (moisture > mist_max) {
        if (valv_sts == 1){
          myservo.write(1000);
          delay(500);
          myservo.write(1500);
          valv_sts = 0;
        } else {
          myservo.write(1500);
          valv_sts = 0;
        }
      }

      break;
    default:
    //LED control
      ledcWrite(2, 255);    //Lightオフ  
    //Servo control
      //myservo.write(1000);
      valv_sts = 0;
  }

//Looping interval time
  delay(1000);
}


//TOPページにアクセスしたきの処理関数
void handleSample() {
  String html;

  //HTML記述
  html = "<!DOCTYPE html>";
  html += "<html lang='ja'>";
  html += "<head>";
  html += "<meta charset=\"utf-8\">";
  html += "<title>IoT&AI_6th</title>";
  html += "</head>";
  html += "<body>";
  html += "<h1>水耕栽培ステータス</h1>";
  html += "<p><h2>プランター：No.1</h2></p>";
  html += "<p><h3>+++++++++++++++++++++++++</h3></p>";
  html += "<p><h3>日時：";
  html += rday;
  html += "　　";
  html += rtime;
  html += "</h3></p>";
  html += "<p><h3>制御時間</h3></p>";
  html += "<p><h3>　　>開始　";
  html += c_onTime[0];
  html += "時　";
  html += c_onTime[1];
  html += "分";
  html += "</h3></p>";
  html += "<p><h3>　　>終了　";
  html += c_offTime[0];
  html += "時　";
  html += c_offTime[1];
  html += "分";
  html += "</h3></p>";
  html += "<p><h3>---------------------------------------------</h3></p>";
  html += "<p><h3>照明</h3></p>";
  html += "<p><h3>　　>指定範囲　　：";
  html += lux_min;
  html += "(lux)～";
  html += lux_max;
  html += "(lux)</h3></p>";
  html += "<p><h3>　　>現在の明るさ：";
  html += lux;
  html += "(lux)";
  html += "</h3></p>";
  html += "<p><h3>---------------------------------------------</h3></p>";
  html += "<p><h3>土壌水分</h3></p>";
  html += "<p><h3>　　>指定範囲　　：";
  html += mist_min;
  html += "(%)～";
  html += mist_max;
  html += "(%)</h3></p>";
  html += "<p><h3>　　>現在の水分　：";
  html += moisture;
  html += "(%)";
  html += "</h3></p>";
  html += "<p><h3>　　>バルブ状態　：";
  html += valv_sts;
  html += "　※0:停止、1:給水中";
  html += "</h3></p>";
  html += "</body>";
  html += "</html>";

  // HTMLを出力する
  server.send(200, "text/html", html);
}

//存在しないアドレスへアクセスしたときの処理関数
void handleNotFound(void) {
  server.send(404, "text/plain", "Not Found");
}
