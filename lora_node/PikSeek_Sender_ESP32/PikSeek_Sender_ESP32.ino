/*
 *  皮探隊 PikSeek - 節點 Sender(ESP32 版)
 *  =============================================================
 *  搭配:PikSeek_GameServer v3.5+
 *  目標板:ESP32 NodeMCU-32S(原版 ESP32)
 *
 *  接線(只要 4 條):
 *      ESP32 GPIO 17 (TX2) ──→ S76S RX
 *      ESP32 GPIO 16 (RX2) ←── S76S TX
 *      ESP32 3.3V          ──→ S76S VCC  ⚠️ 不能用 5V!
 *      ESP32 GND           ──→ S76S GND
 *
 *  ⚠️ 千萬不要用 GPIO 1 (TX0) / GPIO 3 (RX0)
 *     那會跟 USB 序列衝突,Serial Monitor 會看不到東西
 *
 *  ============================================================
 *  操作步驟:
 *  ============================================================
 *  1. Arduino IDE 安裝 ESP32 板子支援
 *     檔案 → 偏好設定 → 額外的開發板管理員網址,加入:
 *     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
 *
 *  2. 工具 → 開發板 → ESP32 Arduino → "NodeMCU-32S" 或 "ESP32 Dev Module"
 *
 *  3. 修改下方 COLLEGE_CODE 為這節點扮演的學院
 *
 *  4. 燒錄(按住 BOOT 鍵不放,點 Upload,看到「Connecting...」再放開)
 *
 *  5. 開 Serial Monitor (115200 baud)
 *  =============================================================
 */

// ╔══════════════════════════════════════════╗
// ║  ⚠️ 請修改這個學院碼!                     ║
// ╚══════════════════════════════════════════╝
const char* COLLEGE_CODE = "ENG";  // MGT / SCI / HUM / ENG / LAW

// 發送間隔(毫秒)
const unsigned long TX_INTERVAL_MS = 1500;

// ====================================================
// LoRa 參數(必須與 GameServer v3.5 完全一致!)
// ====================================================
#define LORA_FREQ      "923000000"   // AS923 台灣
#define LORA_SF        "7"
#define LORA_BW        "125"
#define LORA_CR        "4/5"
#define LORA_SYNC      "12"
#define LORA_CRC       "on"
#define LORA_PWR       "14"

// ====================================================
// ESP32 Serial 設定
// Serial0 = USB(印 log 用),不要用
// Serial1 = ESP32 內建,但預設腳位不便,跳過
// Serial2 = GPIO 16 (RX) / GPIO 17 (TX) ← 用這個
// ====================================================
#define LORA_SERIAL Serial2

#include "DHT.h"
#define DHTPIN 4       // 假設接在 GPIO 4
#define DHTTYPE DHT11  // 指定使用 DHT11
DHT dht(DHTPIN, DHTTYPE);

// 如果你想用其他腳位(進階),可以這樣明確指定:
// Serial2.begin(115200, SERIAL_8N1, 16, 17);
//                                   ^^  ^^
//                                   RX  TX
// 然後上面的 LORA_SERIAL 仍指向 Serial2 即可

// ====================================================
// 內部狀態
// ====================================================
unsigned int counter = 0;
unsigned long lastTxTime = 0;




// ====================================================
// 發送 AT 指令給 S76S 並印出回應
// ====================================================
void sendLoraCmd(const char* cmd) {
    Serial.print(">> ");
    Serial.println(cmd);
    LORA_SERIAL.write(cmd);
    delay(500);

    String resp = "";
    while (LORA_SERIAL.available()) {
        resp += (char)LORA_SERIAL.read();
    }
    if (resp.length() > 0) {
        Serial.print("<< ");
        Serial.println(resp);
    }
}

// ====================================================
// 設定 LoRa P2P
// ====================================================
void setupLoRa() {
    Serial.println("[LoRa] Initializing S76S...");

    // ESP32 Serial2 需要指定腳位
    LORA_SERIAL.begin(115200, SERIAL_8N1, 16, 17);  // RX=16, TX=17
    delay(1000);

    // 清空 buffer
    while (LORA_SERIAL.available()) LORA_SERIAL.read();

    String cmd;

    cmd = "rf set_freq "; cmd += LORA_FREQ;
    sendLoraCmd(cmd.c_str());

    cmd = "rf set_sf "; cmd += LORA_SF;
    sendLoraCmd(cmd.c_str());

    cmd = "rf set_bw "; cmd += LORA_BW;
    sendLoraCmd(cmd.c_str());

    cmd = "rf set_cr "; cmd += LORA_CR;
    sendLoraCmd(cmd.c_str());

    cmd = "rf set_crc "; cmd += LORA_CRC;
    sendLoraCmd(cmd.c_str());

    cmd = "rf set_sync "; cmd += LORA_SYNC;
    sendLoraCmd(cmd.c_str());

    cmd = "rf set_pwr "; cmd += LORA_PWR;
    sendLoraCmd(cmd.c_str());

    Serial.println("[LoRa] ✅ S76S configured. Ready to transmit.\n");
}

// ====================================================
// 把 ASCII 字串轉成 hex 字串
// ====================================================
String asciiToHex(const String& s) {
    String hex = "";
    for (int i = 0; i < (int)s.length(); i++) {
        char buf[3];
        sprintf(buf, "%02X", (unsigned char)s[i]);
        hex += buf;
    }
    return hex;
}

// ====================================================
// 發送一次封包（已相容工院 DHT11 與其他學院基本功能）
// ====================================================
void sendPikPacket() {
    char packetStr[32]; // 加大陣列長度以安全容納較長的字串

    // 判斷當前節點是否為工院 (ENG)
    if (strcmp(COLLEGE_CODE, "ENG") == 0) {
        // 1. 僅在工院時讀取溫濕度數值
        float h = dht.readHumidity();
        float t = dht.readTemperature();

        // 檢查是否讀取失敗（例如沒接硬體或斷線）
        if (isnan(h) || isnan(t)) {
            Serial.println("❌ [DHT] 讀取失敗！請檢查接線。發送預設數值測試。");
            h = 50; // 失敗時的預設替代值
            t = 25;
        }

        // 2. 組裝成 8735 (lora_handler.h) 期待的工院特殊格式
        // 範例：PIK1ENG_H85_T28
        snprintf(packetStr, sizeof(packetStr), "PIK1%s_H%f_T%f", COLLEGE_CODE, h, t);
    } 
    else {
        // 其他學院（MGT, SCI, HUM, LAW）不讀取 DHT，維持原有的純訊號計數器格式
        // 範例：PIK1MGT0001
        snprintf(packetStr, sizeof(packetStr), "PIK1%s%04d", COLLEGE_CODE, counter % 10000);
    }

    // 3. 將 ASCII 字串轉換為 Hex payload
    String hexPayload = asciiToHex(String(packetStr));
    String txCmd = "rf tx " + hexPayload;

    Serial.print("\n[TX #");
    Serial.print(counter);
    Serial.print("] Broadcasting ");
    Serial.print(packetStr);
    Serial.print("  →  ");
    Serial.println(txCmd);

    LORA_SERIAL.write(txCmd.c_str());

    // 等待回應 (radio_tx_ok)
    unsigned long timeout = millis() + 2500;
    String resp = "";
    while (millis() < timeout) {
        while (LORA_SERIAL.available()) {
            char c = LORA_SERIAL.read();
            resp += c;
            if (resp.indexOf("radio_tx_ok") >= 0 ||
                resp.indexOf("ok") >= 0) {
                goto txDone;
            }
        }
        delay(10);
    }

txDone:
    if (resp.length() > 0) {
        Serial.print("[Response] ");
        Serial.println(resp);
    } else {
        Serial.println("[Response] (no response, timeout)");
    }

    counter++;
}

// ====================================================
// Setup
// ====================================================
void setup() {
    Serial.begin(115200);  // USB 序列(印 log 用)
    delay(1500);

    Serial.println();
    Serial.println("================================================");
    Serial.println("    皮探隊 PikSeek - 節點 Sender (ESP32)");
    Serial.println("================================================");
    Serial.print("    板子:        ESP32 NodeMCU-32S");
    Serial.println();
    Serial.print("    學院碼:      ");
    Serial.println(COLLEGE_CODE);
    Serial.print("    發送間隔:    ");
    Serial.print(TX_INTERVAL_MS);
    Serial.println(" ms");
    Serial.print("    LoRa 頻率:   ");
    Serial.println(LORA_FREQ);
    Serial.println("    Serial2 腳位:  RX=GPIO16, TX=GPIO17");
    Serial.println("================================================");
    Serial.println();

    setupLoRa();

    dht.begin();

    Serial.println("[Setup] Starting broadcast...\n");
}

// ====================================================
// Main Loop
// ====================================================
void loop() {
    if (millis() - lastTxTime >= TX_INTERVAL_MS) {
        sendPikPacket();
        lastTxTime = millis();
    }
}
