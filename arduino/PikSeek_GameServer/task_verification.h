/*
 *  task_verification.h - 統一推論 Task
 *  =============================================================
 *  【A 負責】此檔的開發責任(配合 camera_yolo.h)
 *
 *  關鍵設計理念:
 *    ❌ 不要每個學院開一個 task(NPU 衝突、RAM 浪費)
 *    ✅ 一個 verificationTask,根據當前學院跑不同函式
 *
 *  運作流程:
 *    玩家按「捕獲」→ requestVerification() 設定狀態為 VERIFYING
 *                 → xSemaphoreGive(verifySignal) 喚醒此 task
 *
 *    此 task wakeup → 看當前學院 → 跑對應的 verify_XXX()
 *                  → 結果寫回 g_verifyResult
 *                  → 若通過,狀態切到 CAPTURED
 *                  → 若失敗,狀態回到 HINTING(讓玩家再試)
 */

#ifndef TASK_VERIFICATION_H
#define TASK_VERIFICATION_H

#include <Arduino.h>
#include "game_state.h"
#include "colleges.h"
#include "camera_yolo.h"
#include "107_detect.h" // 加上你的分類器

// ====================================================
// 驗證函式宣告
// ====================================================
bool verifyMGT(); // 管院:YOLO person + LoRa RSSI > -50
bool verifySCI(); // 理院:YOLO potted plant
bool verifyHUM(); // 人文院:YOLO book
bool verifyENG(); // 工院:濕度 >= 80%(純感測器)
bool verifyLAW(); // 法院:Template Matching → 自訓二分類

// ====================================================
// 管院:YOLO person + LoRa RSSI 雙重驗證
// ====================================================
bool verifyMGT()
{
    // 條件 1:LoRa RSSI 夠強(代表玩家真的在節點旁)
    int rssi = -120;
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE)
    {
        rssi = g_lastRssi;
        xSemaphoreGive(stateMutex);
    }
    if (rssi < -50)
    {
        Serial.print("[VERIFY MGT] ❌ RSSI too weak: ");
        Serial.println(rssi);
        return false;
    }

    // 條件 2:YOLO 偵測到 person
    YoloResults r = yoloDetect();
    if (!yoloHasClass(r, YOLO_PERSON, 0.6))
    {
        Serial.println("[VERIFY MGT] ❌ No person detected");
        return false;
    }

    Serial.println("[VERIFY MGT] ✅ RSSI strong + person found");
    return true;
}

// ====================================================
// 理院:YOLO potted plant
// ====================================================
bool verifySCI()
{
    YoloResults r = yoloDetect();
    if (yoloHasClass(r, YOLO_POTTED_PLANT, 0.5))
    {
        Serial.println("[VERIFY SCI] ✅ Plant detected");
        return true;
    }
    Serial.println("[VERIFY SCI] ❌ No plant detected");
    return false;
}

// ====================================================
// 人文院:YOLO book
// ====================================================
bool verifyHUM()
{
    YoloResults r = yoloDetect();
    if (yoloHasClass(r, YOLO_BOOK, 0.5))
    {
        Serial.println("[VERIFY HUM] ✅ Book detected");
        return true;
    }
    Serial.println("[VERIFY HUM] ❌ No book detected");
    return false;
}

// ====================================================
// 工院:濕度 >= 80%(LoRa 傳來的 DHT22 數值)
// ====================================================
bool verifyENG()
{
    int humidity = 0;
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE)
    {
        humidity = g_engHumidity;
        xSemaphoreGive(stateMutex);
    }

    Serial.print("[VERIFY ENG] Humidity = ");
    Serial.print(humidity);
    Serial.println("%");

    if (humidity >= 80)
    {
        Serial.println("[VERIFY ENG] ✅ Humidity threshold met");
        return true;
    }
    Serial.println("[VERIFY ENG] ❌ Need to blow more");
    return false;
}

// ====================================================
// 法院:Template Matching(MVP)→ 自訓二分類(進階)
// ====================================================
bool verifyLAW()
{
    Serial.println("[VERIFY LAW] Checking 107 doorplate...");

    // 呼叫你的 107_detect 模組
    if (getDoorplateStatus() == IS_107)
    {
        Serial.println("[VERIFY LAW] ✅ 107 門牌辨識通過！");
        return true;
    }

    Serial.println("[VERIFY LAW] ❌ 非 107 門牌");
    return false; // 補上這個預設回傳
}

// ====================================================
// Verification Task 主體
// ====================================================
void verificationTask(void *parameter)
{
    Serial.println("[Task] verificationTask started, waiting for signals...");

    while (true)
    {
        // 等待玩家觸發(由 web_server 呼叫 requestVerification() 喚醒)
        xSemaphoreTake(verifySignal, portMAX_DELAY);

        // 取得當前學院
        String college;
        if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE)
        {
            college = g_currentCollege;
            xSemaphoreGive(stateMutex);
        }

        if (college.length() == 0)
        {
            Serial.println("[VERIFY] No active college, ignoring");
            continue;
        }

        Serial.print("\n========== 開始驗證: ");
        Serial.print(college);
        Serial.println(" ==========");

        // 根據學院跑對應的 verify 函式
        bool passed = false;
        if (college == "MGT")
            passed = verifyMGT();
        else if (college == "SCI")
            passed = verifySCI();
        else if (college == "HUM")
            passed = verifyHUM();
        else if (college == "ENG")
            passed = verifyENG();
        else if (college == "LAW")
            passed = verifyLAW();

        // 更新結果
        const College *col = findCollege(college);
        if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE)
        {
            if (passed)
            {
                // 加入已捕獲清單
                if (g_collegesGot.indexOf(college) < 0)
                {
                    if (g_collegesGot.length() > 0)
                        g_collegesGot += ",";
                    g_collegesGot += college;
                }
                g_lastMessage = col ? col->successMsg : "";
                g_verifyResult = "✨ 捕獲成功!";
                g_state = STATE_CAPTURED;
                g_stateChangedAt = millis();
                Serial.print("[GAME] → CAPTURED: ");
                Serial.println(college);
            }
            else
            {
                g_verifyResult = "❌ 任務未通過,再試一次!";
                g_state = STATE_HINTING; // 回到 HINTING 讓玩家再試
                g_stateChangedAt = millis();
            }
            g_verifyResultReady = true;
            xSemaphoreGive(stateMutex);
        }

        Serial.print("========== ");
        Serial.print(passed ? "✅ PASS" : "❌ FAIL");
        Serial.println(" ==========\n");
    }
}

#endif // TASK_VERIFICATION_H
