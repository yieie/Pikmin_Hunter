/*
 *  lora_handler.h - LoRa 接收
 *  =============================================================
 *  【B 負責】此檔的開發責任
 *
 *  功能:
 *    - 設定 S76S(Serial3, AS923, SF7, BW125)
 *    - 持續監聽 Serial3 收 radio_rx 封包
 *    - 過濾 PIK 開頭封包
 *    - 解析學院碼 + RSSI/SNR
 *    - 工院特殊封包(含濕度):PIK1ENG_H85_T28
 *    - 觸發 game_state.h 的 triggerCollegeDetection()
 *
 *  ⚠️ LoRa 參數必須與節點端的 Sender 一致
 */

#ifndef LORA_HANDLER_H
#define LORA_HANDLER_H

#include <Arduino.h>
#include "game_state.h"
#include "colleges.h"

// ====================================================
// LoRa 參數(與 Sender 一致!)
// ====================================================
#define LORA_FREQ      "923000000"
#define LORA_SF        "7"
#define LORA_BW        "125"
#define LORA_CR        "4/5"
#define LORA_SYNC      "12"
#define LORA_CRC       "on"

// 觸發冷卻:同一學院 30 秒內不重複觸發
#define LORA_TRIGGER_COOLDOWN_MS 30000

// ====================================================
// 內部狀態
// ====================================================
unsigned long g_lastTriggerTime[5] = {0, 0, 0, 0, 0};

// ====================================================
// 發送 AT 指令給 S76S
// ====================================================
void sendLoraCmd(const char* cmd) {
    Serial.print("[LoRa] >> ");
    Serial.println(cmd);
    Serial3.write(cmd);
    delay(500);
    String resp = "";
    while (Serial3.available()) {
        resp += (char)Serial3.read();
    }
    if (resp.length() > 0) {
        Serial.print("[LoRa] << ");
        Serial.println(resp);
    }
}

// ====================================================
// 設定 LoRa P2P
// ====================================================
void setupLoRa() {
    Serial.println("\n[LoRa] Initializing S76S...");
    Serial3.begin(115200);
    delay(1000);
    while (Serial3.available()) Serial3.read();

    String cmd;
    cmd = "rf set_freq "; cmd += LORA_FREQ;  sendLoraCmd(cmd.c_str());
    cmd = "rf set_sf ";   cmd += LORA_SF;    sendLoraCmd(cmd.c_str());
    cmd = "rf set_bw ";   cmd += LORA_BW;    sendLoraCmd(cmd.c_str());
    cmd = "rf set_cr ";   cmd += LORA_CR;    sendLoraCmd(cmd.c_str());
    cmd = "rf set_crc ";  cmd += LORA_CRC;   sendLoraCmd(cmd.c_str());
    cmd = "rf set_sync "; cmd += LORA_SYNC;  sendLoraCmd(cmd.c_str());

    sendLoraCmd("rf rx_con on");
    sendLoraCmd("rf rx 0");
    Serial.println("[LoRa] ✅ S76S ready, listening...\n");
}

// ====================================================
// hex → ASCII
// ====================================================
String hexToAscii(const String& hex) {
    String result = "";
    for (int i = 0; i + 1 < (int)hex.length(); i += 2) {
        char byteStr[3] = {hex[i], hex[i+1], 0};
        char c = (char)strtol(byteStr, NULL, 16);
        if (c >= 32 && c < 127) result += c;
        else result += '?';
    }
    return result;
}

// ====================================================
// 解析工院封包濕度
// 範例:PIK1ENG_H85_T28_0001
// ====================================================
void parseEngHumidity(const String& decoded) {
    int hIdx = decoded.indexOf("_H");
    int tIdx = decoded.indexOf("_T");
    if (hIdx < 0 || tIdx < 0) return;

    int humidity = decoded.substring(hIdx + 2, tIdx).toInt();
    int temperature = 0;
    int t_end = decoded.indexOf("_", tIdx + 1);
    if (t_end < 0) t_end = decoded.length();
    temperature = decoded.substring(tIdx + 2, t_end).toInt();

    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        g_engHumidity    = humidity;
        g_engTemperature = temperature;
        xSemaphoreGive(stateMutex);
    }

    Serial.print("[LoRa] ENG sensor: humidity=");
    Serial.print(humidity);
    Serial.print("%, temp=");
    Serial.print(temperature);
    Serial.println("°C");
}

// ====================================================
// 解析 radio_rx 一行
// 格式:radio_rx <hex_payload> <rssi> <snr>
// ====================================================
void parseLoraRx(const String& line) {
    int idx = line.indexOf("radio_rx");
    if (idx < 0) return;

    int dataStart = idx + 9;
    int dataEnd = line.indexOf(' ', dataStart);
    if (dataEnd < 0) return;
    String hexData = line.substring(dataStart, dataEnd);

    int rssiStart = dataEnd + 1;
    int rssiEnd = line.indexOf(' ', rssiStart);
    if (rssiEnd < 0) return;
    int rssi = line.substring(rssiStart, rssiEnd).toInt();

    int snrStart = rssiEnd + 1;
    String snrStr = line.substring(snrStart);
    snrStr.trim();
    int snr = snrStr.toInt();

    String decoded = hexToAscii(hexData);

    // 過濾:必須 PIK1 開頭
    if (!decoded.startsWith("PIK1")) {
        Serial.print("[LoRa] ⚠️ Non-PIK ignored: ");
        Serial.println(decoded);
        return;
    }

    if (decoded.length() < 7) {
        Serial.println("[LoRa] ⚠️ Malformed");
        return;
    }

    String college = decoded.substring(4, 7);
    int collegeIdx = collegeCodeToIdx(college);
    if (collegeIdx < 0) {
        Serial.print("[LoRa] ⚠️ Unknown college: ");
        Serial.println(college);
        return;
    }

    Serial.print("[LoRa] 📥 ");
    Serial.print(decoded);
    Serial.print(" | RSSI=");
    Serial.print(rssi);
    Serial.print(" | SNR=");
    Serial.println(snr);

    // 更新 RSSI/SNR/sender
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        g_lastRssi     = rssi;
        g_lastSnr      = snr;
        g_lastSender   = college;
        g_lastLoraTime = millis();
        xSemaphoreGive(stateMutex);
    }

    // 工院的特殊封包解析(含濕度)
    if (college == "ENG" && decoded.indexOf("_H") >= 0) {
        parseEngHumidity(decoded);
    }

    // 冷卻檢查
    unsigned long now = millis();
    if (now - g_lastTriggerTime[collegeIdx] < LORA_TRIGGER_COOLDOWN_MS) {
        return;  // 還在冷卻
    }

    // 觸發學院偵測
    triggerCollegeDetection(college, rssi);
    g_lastTriggerTime[collegeIdx] = now;

    // 重新進入接收模式
    Serial3.write("rf rx 0");
}

// ====================================================
// LoRa Task
// ====================================================
void loraTask(void *parameter) {
    setupLoRa();

    String lineBuf = "";
    lineBuf.reserve(256);

    while (true) {
        while (Serial3.available()) {
            char c = Serial3.read();
            if (c == '\n') {
                if (lineBuf.length() > 0) {
                    parseLoraRx(lineBuf);
                    lineBuf = "";
                }
            } else if (c != '\r') {
                lineBuf += c;
                if (lineBuf.length() > 240) lineBuf = "";
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

#endif // LORA_HANDLER_H
