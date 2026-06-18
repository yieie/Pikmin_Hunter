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

#include "C:\Users\Brian\Desktop\107\secrets.h"
#include "game_state.h"
#include "colleges.h"
#include "lora_handler.h"
#include "camera_yolo.h"
#include "task_verification.h"
#include "web_server.h"
#include "107_detect.h"

// ====================================================
// Wi-Fi
// ====================================================
char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;
int wifiStatus = WL_IDLE_STATUS;

// ====================================================
// 串流影像設定
// ====================================================
VideoSetting config(VIDEO_VGA, STREAM_FPS, VIDEO_JPEG, 1);

// ====================================================
// 連 Wi-Fi
// ====================================================
void connectWiFi()
{
    Serial.print("[WiFi] Connecting to ");
    Serial.println(ssid);
    while (wifiStatus != WL_CONNECTED)
    {
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
void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================================");
    Serial.println("    皮探隊 PikSeek - GameServer v4.0");
    Serial.println("    模組化版本(LoRa + YOLO + Verification)");
    Serial.println("================================================");

    // 初始化遊戲狀態(建立 mutex)
    // 初始化遊戲狀態(建立 mutex)
    initGameState();

    // 連 Wi-Fi
    connectWiFi();

    // ====================================================
    // 啟動相機雙頻道 (直接覆蓋原本 Camera 的地方)
    // ====================================================
    // 頻道 0: 專門給網頁 MJPEG 串流看畫面 (VGA, JPEG 格式)
    VideoSetting configWidget(VIDEO_VGA, STREAM_FPS, VIDEO_JPEG, 1);
    Camera.configVideoChannel(0, configWidget);

    // 頻道 1: 專門餵給你的 107 分類模型 (128x128, RGB 格式)
    VideoSetting configAI(128, 128, 10, VIDEO_RGB, 0);
    Camera.configVideoChannel(1, configAI);

    Camera.videoInit();

    // 雙通道各自開啟，各走各的路，互不卡死
    Camera.channelBegin(0);
    Camera.channelBegin(1);
    Serial.println("[Camera] Dual Channels (0: Web, 1: AI) Initialized");

    // 初始化模型 (注意：先不要初始化 YOLO，測試時我們先專注在 107 門牌)
    initYOLO(); //
    init107Detect();
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
    xTaskCreate(webTask, "WebTask", (4 * 1024), NULL, 1, NULL);
    xTaskCreate(streamTask, "StreamTask", (8 * 1024), NULL, 1, NULL);
    xTaskCreate(stateTask, "StateTask", (2 * 1024), NULL, 1, NULL);
    xTaskCreate(loraTask, "LoraTask", (4 * 1024), NULL, 1, NULL);
    xTaskCreate(verificationTask, "VerifyTask", (6 * 1024), NULL, 1, NULL);

    Serial.println("[Setup] All 5 tasks started.");
}

// ====================================================
// Loop(主程式空,所有工作交給 task)
// ====================================================
void loop()
{
    delay(30000);
    Serial.print("[Heartbeat] uptime: ");
    Serial.print(millis() / 1000);
    Serial.println("s");
}
