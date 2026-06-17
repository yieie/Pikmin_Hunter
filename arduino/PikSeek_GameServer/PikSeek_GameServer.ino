/*
 *  皮探隊 PikSeek - GameServer v4.0(模組化版本)
 *  =============================================================
 *
 *  專案資料夾結構:
 *      PikSeek_GameServer/
 *      ├── PikSeek_GameServer.ino     ← 本檔(主程式,只做 setup)
 *      ├── secrets.h                  ← Wi-Fi、API key 等私密設定
 *      ├── game_state.h               ← 遊戲狀態變數 + Mutex(共用)
 *      ├── colleges.h                 ← 五院關卡資料(謎題、答案、技術)
 *      ├── lora_handler.h             ← LoRa Task + 封包解析    【B 負責】
 *      ├── camera_yolo.h              ← YOLO 推論介面            【A 負責】
 *      ├── task_verification.h        ← 五院驗證函式 + 推論 Task 【A 負責】
 *      └── web_server.h               ← Web Task + 路由 + 網頁HTML【A 負責】
 *
 *  五個 FreeRTOS Task:
 *      1. webTask           - 處理網頁請求 (port 80)
 *      2. streamTask        - MJPEG 串流 (port 8080)
 *      3. loraTask          - 持續監聽 LoRa,觸發學院偵測
 *      4. stateTask         - 時間驅動狀態推進(DETECTED→HINTING 等)
 *      5. verificationTask  - 統一推論 Task,根據學院跑不同模型
 *
 *  ⚠️ 把所有 .h 檔放在跟 .ino 同一個資料夾
 */

#include <WiFi.h>
#include "VideoStream.h"

#include "../secrets.h"
#include "game_state.h"
#include "colleges.h"
#include "lora_handler.h"
#include "camera_yolo.h"
#include "task_verification.h"
#include "web_server.h"

// ====================================================
// Wi-Fi
// ====================================================
char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;
int wifiStatus = WL_IDLE_STATUS;

// ====================================================
// 串流影像與 NN 設定
// ====================================================
#define CHANNELNN 3
#define NNWIDTH   576
#define NNHEIGHT  320

VideoSetting config(VIDEO_VGA, STREAM_FPS, VIDEO_JPEG, 1);
// 建立給 NN 使用的低解析度 RGB 影片設定
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);

// ====================================================
// 連 Wi-Fi
// ====================================================
void connectWiFi() {
    Serial.print("[WiFi] Connecting to ");
    Serial.println(ssid);
    while (wifiStatus != WL_CONNECTED) {
        wifiStatus = WiFi.begin(ssid, pass);
        Serial.print(".");
        delay(3000);
    }
    Serial.println();
    Serial.print("[WiFi] Connected! IP: ");
    Serial.println(WiFi.localIP());
}

// ====================================================
// Setup
// ====================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================================");
    Serial.println("    皮探隊 PikSeek - GameServer v4.0");
    Serial.println("    模組化版本(LoRa + YOLO + Verification)");
    Serial.println("================================================");

    // 初始化遊戲狀態(建立 mutex)
    initGameState();

    // 連 Wi-Fi
    connectWiFi();

    // 啟動主相機通道 (網頁串流用)
    Camera.configVideoChannel(CHANNEL, config);
    
    // 啟動 NN 相機通道 (NPU 辨識用)
    Camera.configVideoChannel(CHANNELNN, configNN);
    Camera.videoInit();
    
    Camera.channelBegin(CHANNEL);
    Serial.println("[Camera] Channel 0 Initialized");

    // 初始化 YOLO (將設定與通道帶入)
    initYOLO(configNN, CHANNELNN);

    // 印出網址
    Serial.println();
    Serial.println("================================================");
    Serial.print("🎮 遊戲入口:   http://");
    Serial.println(WiFi.localIP());
    Serial.print("📺 純串流:     http://");
    Serial.print(WiFi.localIP());
    Serial.println(":8080");
    Serial.print("📊 狀態 API:   http://");
    Serial.print(WiFi.localIP());
    Serial.println("/api/state");
    Serial.println("================================================");

    // 啟動五個 Task
    xTaskCreate(webTask,          "WebTask",         (4 * 1024), NULL, 1, NULL);
    xTaskCreate(streamTask,       "StreamTask",      (8 * 1024), NULL, 1, NULL);
    xTaskCreate(stateTask,        "StateTask",       (2 * 1024), NULL, 1, NULL);
    xTaskCreate(loraTask,         "LoraTask",        (4 * 1024), NULL, 1, NULL);
    xTaskCreate(verificationTask, "VerifyTask",      (6 * 1024), NULL, 1, NULL);

    Serial.println("[Setup] All 5 tasks started.");
}

// ====================================================
// Loop(主程式空,所有工作交給 task)
// ====================================================
void loop() {
    delay(30000);
    Serial.print("[Heartbeat] uptime: ");
    Serial.print(millis() / 1000);
    Serial.println("s");
}
