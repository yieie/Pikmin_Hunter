/*
 *  game_state.h - 遊戲共享狀態
 *  =============================================================
 *  ⚠️ 共用檔案:A、B 都會讀寫此檔的變數,**所有存取要透過 mutex**
 *
 *  使用範例:
 *      if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
 *          int rssi = g_lastRssi;
 *          xSemaphoreGive(stateMutex);
 *      }
 *
 *  Helper 函式:
 *      triggerCollegeDetection() - 由 LoRa task 呼叫
 *      addCaptured()             - 由 verify task 呼叫
 *      requestVerification()     - 由 web task 呼叫(玩家按按鈕)
 */

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <Arduino.h>

// ====================================================
// 通用常數
// ====================================================
#define CHANNEL       0
#define STREAM_FPS    15
#define FRAME_INTERVAL (1000 / STREAM_FPS / portTICK_PERIOD_MS)
#define PART_BOUNDARY "PIKSEEK_FRAME_BOUNDARY"

// ====================================================
// 遊戲狀態列舉
// ====================================================
enum GameState {
    STATE_WAITING   = 0,  // 等待 LoRa 觸發
    STATE_DETECTED  = 1,  // 偵測到訊號(2.5 秒提示)
    STATE_HINTING   = 2,  // 顯示謎題(玩家解謎)
    STATE_VERIFYING = 3,  // AI 推論中
    STATE_CAPTURED  = 4   // 捕獲成功(5 秒慶祝)
};

// ====================================================
// 共享變數(所有存取需透過 mutex)
// ====================================================
extern GameState     g_state;
extern String        g_currentCollege;
extern String        g_collegesGot;
extern int           g_lastRssi;
extern int           g_lastSnr;
extern int           g_engHumidity;          // 工院 DHT22 濕度(LoRa 傳過來)
extern int           g_engTemperature;       // 工院 DHT22 溫度
extern String        g_lastSender;
extern unsigned long g_lastLoraTime;
extern unsigned long g_stateChangedAt;
extern String        g_lastMessage;
extern bool          g_verifyRequested;      // 玩家按了「驗證」按鈕
extern String        g_verifyResult;         // 驗證結果文字(給網頁顯示)
extern bool          g_verifyResultReady;    // 結果可顯示了

// Mutex 保護所有 g_ 變數
extern SemaphoreHandle_t stateMutex;

// 用來喚醒 verificationTask 的訊號
extern SemaphoreHandle_t verifySignal;

// ====================================================
// 初始化(在 setup 呼叫一次)
// ====================================================
inline void initGameState() {
    stateMutex   = xSemaphoreCreateMutex();
    verifySignal = xSemaphoreCreateBinary();
    g_stateChangedAt = millis();
}

// ====================================================
// Helper:由 LoRa 收到合法封包後呼叫
// ====================================================
inline void triggerCollegeDetection(const String& code, int rssi) {
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        // 已經在處理其他學院 → 忽略
        if (g_state != STATE_WAITING) {
            xSemaphoreGive(stateMutex);
            return;
        }
        // 已捕獲過 → 忽略
        if (g_collegesGot.indexOf(code) >= 0) {
            xSemaphoreGive(stateMutex);
            return;
        }

        g_currentCollege = code;
        g_lastRssi = rssi;
        g_state = STATE_DETECTED;
        g_stateChangedAt = millis();
        xSemaphoreGive(stateMutex);

        Serial.print("[GAME] → DETECTED: ");
        Serial.println(code);
    }
}

// ====================================================
// Helper:加入已捕獲清單
// ====================================================
inline void addCaptured(const String& code) {
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        if (g_collegesGot.indexOf(code) < 0) {
            if (g_collegesGot.length() > 0) g_collegesGot += ",";
            g_collegesGot += code;
        }
        xSemaphoreGive(stateMutex);
    }
}

// ====================================================
// Helper:玩家按下「捕獲驗證」按鈕
// ====================================================
inline void requestVerification() {
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        // 必須在 HINTING 狀態才能驗證
        if (g_state == STATE_HINTING) {
            g_state = STATE_VERIFYING;
            g_stateChangedAt = millis();
            g_verifyResultReady = false;
        }
        xSemaphoreGive(stateMutex);
    }
    // 喚醒 verificationTask(由它去跑 AI)
    xSemaphoreGive(verifySignal);
}

// ====================================================
// 變數定義(在主程式 .ino 不需要,放這裡集中)
// ====================================================
GameState     g_state              = STATE_WAITING;
String        g_currentCollege     = "";
String        g_collegesGot        = "";
int           g_lastRssi           = -120;
int           g_lastSnr            = 0;
int           g_engHumidity        = 0;
int           g_engTemperature     = 0;
String        g_lastSender         = "";
unsigned long g_lastLoraTime       = 0;
unsigned long g_stateChangedAt     = 0;
String        g_lastMessage        = "";
bool          g_verifyRequested    = false;
String        g_verifyResult       = "";
bool          g_verifyResultReady  = false;

SemaphoreHandle_t stateMutex   = NULL;
SemaphoreHandle_t verifySignal = NULL;

// ====================================================
// 時間驅動狀態推進 Task
//   - DETECTED 2.5 秒後 → HINTING
//   - CAPTURED 5 秒後 → WAITING
// ====================================================
void stateTask(void *parameter) {
    while (true) {
        vTaskDelay(500 / portTICK_PERIOD_MS);

        if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
            unsigned long elapsed = millis() - g_stateChangedAt;

            if (g_state == STATE_DETECTED && elapsed > 2500) {
                g_state = STATE_HINTING;
                g_stateChangedAt = millis();
                Serial.println("[GAME] → HINTING");
            }
            else if (g_state == STATE_CAPTURED && elapsed > 5000) {
                g_state = STATE_WAITING;
                g_currentCollege = "";
                g_lastMessage = "";
                g_stateChangedAt = millis();
                Serial.println("[GAME] → WAITING");
            }
            xSemaphoreGive(stateMutex);
        }
    }
}

#endif // GAME_STATE_H
