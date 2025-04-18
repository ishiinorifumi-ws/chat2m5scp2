#include <M5Unified.h>        // M5Unifiedライブラリを使用
#include <WiFi.h>             // WiFi接続用
#include <HTTPClient.h>       // HTTPリクエスト用
#include <WiFiClientSecure.h> // HTTPS接続用
#include <time.h>             // 時刻取得用

// ↓↓↓ --- ここから設定 --- ↓↓↓

// Wi-Fi設定
const char* ssid = "YOUR_WIFI_SSID";       // ★あなたのWi-Fi SSIDに書き換えてください
const char* password = "YOUR_WIFI_PASSWORD"; // ★あなたのWi-Fi パスワードに書き換えてください

// AWS API Gateway エンドポイント設定
// 例: "https://{api-id}.execute-api.{region}.amazonaws.com/{stage}/status"
const String awsApiUrl = "YOUR_ENDPOINT_URL/starus"; // ★あなたのAPI Gateway /status エンドポイントURLに書き換えてください

// ポーリング時間帯設定 (24時間表記)
const int START_HOUR = 8;  // ポーリング開始時刻（この時間を含む）
const int END_HOUR = 21; // ポーリング終了時刻（この時間は含まない）

// ポーリング間隔 (ミリ秒)
const unsigned long pollingInterval = 10000; // 10秒ごとにチェック

// NTP設定 (時刻同期用)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9 * 3600; // JST (UTC+9)
const int daylightOffset_sec = 0;

// ↑↑↑ --- 設定ここまで --- ↑↑↑


unsigned long previousMillis = 0; // 前回のポーリング時刻

WiFiClientSecure clientSecure; // HTTPS通信用のクライアント

// NTPサーバーと時刻同期を行う関数
void syncNtpTime() {
  Serial.println("Syncing time with NTP server...");
  M5.Display.fillRect(0, M5.Display.height() - 20, M5.Display.width(), 20, BLACK);
  M5.Display.setCursor(0, M5.Display.height() - 20);
  M5.Display.println("Syncing NTP...");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  int retry = 0;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    M5.Display.print(".");
    delay(1000);
    retry++;
    if (retry >= 30) {
      Serial.println("\nFailed to obtain time from NTP.");
      M5.Display.println("\nNTP Failed!");
      return;
    }
  }
  Serial.println("\nTime synchronized successfully!");
  M5.Display.println("\nNTP OK!");

  m5::rtc_time_t rtc_time;
  rtc_time.hours = timeinfo.tm_hour;
  rtc_time.minutes = timeinfo.tm_min;
  rtc_time.seconds = timeinfo.tm_sec;
  m5::rtc_date_t rtc_date;
  rtc_date.year = timeinfo.tm_year + 1900;
  rtc_date.month = timeinfo.tm_mon + 1;
  rtc_date.date = timeinfo.tm_mday;
  M5.Rtc.setTime(&rtc_time);
  M5.Rtc.setDate(&rtc_date);

  Serial.print("Current Time (JST): ");
  Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
  M5.Display.printf("%02d:%02d:%02d\n", rtc_time.hours, rtc_time.minutes, rtc_time.seconds);
  delay(1000);
}


// Wi-Fi接続処理
void connectWiFi() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setCursor(0, 10);
  M5.Display.printf("Connecting to WiFi...\nSSID: %s\n", ssid);
  Serial.printf("Connecting to WiFi...\nSSID: %s\n", ssid);
  WiFi.begin(ssid, password);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    M5.Display.print(".");
    count++;
    if (count > 40) { // 20秒経っても繋がらない場合は諦める
      M5.Display.println("\nFailed to connect.");
      Serial.println("\nFailed to connect.");
      delay(5000);
      return;
    }
  }
  M5.Display.println("\nWiFi connected!");
  M5.Display.print("IP: "); M5.Display.println(WiFi.localIP());
  Serial.println("\nWiFi connected!"); Serial.print("IP address: "); Serial.println(WiFi.localIP());
  delay(1000);
}

// ★★★ ブザーを鳴らす処理 (音程・長さを変更したバージョン) ★★★
void buzz() {
  Serial.println("BUZZ! (Chime: E5-C5)");

  M5.Display.fillScreen(BLUE);
  M5.Display.setCursor(10, M5.Display.height() / 2 - 10);
  M5.Display.setTextSize(2);
  M5.Display.print("NEW ARRIVAL!");

  // --- 音階と長さの設定 (ユーザー指定の値に変更) ---
  int freq1 = 659;     // 1音目（ピン）の周波数 (E5 - ミ)
  int duration1 = 800; // 1音目の長さ (ミリ秒)
  int pause = 100;     // 音と音の間の無音時間 (ミリ秒)
  int freq2 = 523;     // 2音目（ポーン）の周波数 (C5 - ド)
  int duration2 = 600; // 2音目の長さ (ミリ秒)
  // --- 設定変更ここまで ---

  M5.Speaker.tone(freq1, duration1);
  delay(duration1 + pause);

  M5.Speaker.tone(freq2, duration2);
  delay(duration2 + 50);

  M5.Speaker.stop();

  M5.Display.setTextSize(1);
  M5.Display.fillScreen(BLACK);
}

// AWS API Gateway をチェックする処理
void checkWebApp() {
  HTTPClient http;
  String fullUrl = awsApiUrl;

  Serial.print("[HTTP] Requesting: "); Serial.println(fullUrl);
  M5.Display.fillRect(0, M5.Display.height()-10, M5.Display.width(), 10, BLACK);
  M5.Display.setCursor(0, M5.Display.height()-10); M5.Display.print("Checking...");
  clientSecure.setInsecure(); // テスト用。必要に応じてコメントアウトまたは証明書設定

  if (http.begin(clientSecure, fullUrl)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) { // 200 OK を期待
        String payload = http.getString(); payload.trim();
        Serial.printf("[HTTP] Code: %d, Payload: %s\n", httpCode, payload.c_str());
        M5.Display.fillRect(0, M5.Display.height()-10, M5.Display.width(), 10, BLACK);
        M5.Display.setCursor(0, M5.Display.height()-10);
        M5.Display.printf("Code:%d %s", httpCode, payload.substring(0, 8).c_str());
        if (payload == "NEW") { buzz(); }
        else if (payload == "OLD") { /* Nothing */ }
        else { Serial.println("Unknown payload received."); M5.Display.print(" Unknown"); }
    } else {
      Serial.printf("[HTTP] GET... failed, code: %d\n", httpCode);
      String errorPayload = (http.getSize() > 0) ? http.getString() : "";
      Serial.printf("[HTTP] Error payload: %s\n", errorPayload.c_str());
      M5.Display.fillRect(0, M5.Display.height()-10, M5.Display.width(), 10, BLACK);
      M5.Display.setCursor(0, M5.Display.height()-10);
      M5.Display.printf("HTTP Err:%d", httpCode);
    }
    http.end();
  } else {
    Serial.printf("[HTTP] Unable to connect to: %s\n", fullUrl.c_str());
    M5.Display.fillRect(0, M5.Display.height()-10, M5.Display.width(), 10, BLACK);
    M5.Display.setCursor(0, M5.Display.height()-10); M5.Display.print("Connect Failed");
  }
}


// 初期設定
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Speaker.begin();

  M5.Display.setRotation(1);
  M5.Display.setTextSize(1);
  M5.Display.println("Setup Start");
  Serial.begin(115200);
  Serial.println("M5StickC Plus2 Start");

  M5.Speaker.setVolume(255); // 音量最大

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    syncNtpTime(); // Wi-Fi接続後に時刻同期
  }
}

// メインループ
void loop() {
  M5.update();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= pollingInterval) {
    previousMillis = currentMillis;

    if(WiFi.status() == WL_CONNECTED) {
      m5::rtc_time_t now_time;
      m5::rtc_date_t now_date;
      M5.Rtc.getTime(&now_time);
      M5.Rtc.getDate(&now_date);

      int currentHour = now_time.hours;
      Serial.printf("Current time: %d-%02d-%02d %02d:%02d:%02d\n", now_date.year, now_date.month, now_date.date, now_time.hours, now_time.minutes, now_time.seconds);

      // ポーリング時間内かチェック
      if (currentHour >= START_HOUR && currentHour < END_HOUR) {
        Serial.printf("Hour %d is within polling window (%d-%d). Checking API...\n", currentHour, START_HOUR, END_HOUR);
        checkWebApp();
      } else {
        Serial.printf("Hour %d is outside polling window (%d-%d). Skipping API check.\n", currentHour, START_HOUR, END_HOUR);
        M5.Display.fillRect(0, M5.Display.height() - 10, M5.Display.width(), 10, BLACK);
        M5.Display.setCursor(0, M5.Display.height() - 10);
        M5.Display.print("Sleeping...");
      }
    } else {
        Serial.println("WiFi disconnected. Trying to reconnect...");
        M5.Display.fillRect(0, M5.Display.height() - 10, M5.Display.width(), 10, BLACK);
        M5.Display.setCursor(0, M5.Display.height() - 10);
        M5.Display.print("WiFi Lost");
        connectWiFi();
    }
  }

  // ボタンAで強制チェック
  if (M5.BtnA.wasPressed()) {
      Serial.println("Button A pressed, checking API (regardless of time)...");
      if(WiFi.status() == WL_CONNECTED) {
          checkWebApp();
          previousMillis = millis();
      } else {
          Serial.println("WiFi disconnected.");
          M5.Display.fillRect(0, M5.Display.height() - 10, M5.Display.width(), 10, BLACK);
          M5.Display.setCursor(0, M5.Display.height() - 10);
          M5.Display.print("WiFi Lost");
      }
  }

  delay(10);
}
