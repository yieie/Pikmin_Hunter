/*
 *  camera_yolo.h - YOLO 推論介面
 *  =============================================================
 *
 *  目的:把「跑 YOLO 推論」這件事的細節封裝起來,
 *        讓 task_verification.h 只要呼叫 yoloDetect(...) 就好。
 *
 *  Phase 1 (現在):提供假介面,讓整套程式可以編譯/跑通
 *  Phase 2 (W3-W4):A 整合 8735 官方 YOLO 範例,把真實邏輯填入
 *
 *  COCO 80 類中,我們關心的:
 *      ID 0 = person       (管院用)
 *      ID 73 = book         (人文院用)
 *      ID 64 = potted plant (理院用)
 *  (其他可參考 COCO classes 表)
 */

#ifndef CAMERA_YOLO_H
#define CAMERA_YOLO_H

#include <Arduino.h>

// ====================================================
// COCO 80 類中的 ID
// ====================================================
#define YOLO_PERSON         0
#define YOLO_POTTED_PLANT   58   // (查 COCO 列表確認,新舊版本可能不同)
#define YOLO_BOOK           73

// ====================================================
// 偵測結果結構
// ====================================================
struct YoloDetection {
    int   classId;
    float confidence;
    int   x, y, w, h;   // bounding box(若不需要可忽略)
};

// 最多回傳的偵測物數
#define MAX_DETECTIONS 16

struct YoloResults {
    int           count;
    YoloDetection objects[MAX_DETECTIONS];
};

// ====================================================
// 初始化 YOLO(在 setup() 呼叫)
// ====================================================
inline void initYOLO() {
    Serial.println("[YOLO] Initializing...");

    // TODO【A】:整合 8735 官方 YOLO 範例,例如:
    //
    //   #include "NNObjectDetection.h"
    //   ObjDetect.configVideo(config);
    //   ObjDetect.modelSelect(OBJECT_DETECTION, YOLOV4TINY, NA_MODEL, NA_MODEL);
    //   ObjDetect.begin();
    //   videoStreamer.registerInput(Camera.getStream(...));
    //   videoStreamer.registerOutput(ObjDetect);
    //
    // 暫時印出佔位訊息,讓系統能跑

    Serial.println("[YOLO] ⚠️ Using stub implementation (placeholder)");
    Serial.println("[YOLO] ⚠️ A needs to integrate official YOLO example");
}

// ====================================================
// 跑一次 YOLO 推論
// 輸入:當前畫面(由 verifyTask 拍下)
// 輸出:偵測到的物件清單
// ====================================================
inline YoloResults yoloDetect() {
    YoloResults r;
    r.count = 0;

    // TODO【A】:替換為真實 YOLO 推論
    //   ObjectDetectionResult* results = ObjDetect.getResult();
    //   int n = ObjDetect.getResultCount();
    //   for (int i = 0; i < n && r.count < MAX_DETECTIONS; i++) {
    //       r.objects[r.count].classId    = results[i].type();
    //       r.objects[r.count].confidence = results[i].score();
    //       r.objects[r.count].x = results[i].xMin();
    //       ...
    //       r.count++;
    //   }
    //
    // 暫時:用 random 模擬 50% 機率偵測到 person(讓測試流程跑通)

    if (random(100) < 50) {
        r.count = 1;
        r.objects[0].classId    = YOLO_PERSON;
        r.objects[0].confidence = 0.85;
        r.objects[0].x = 100; r.objects[0].y = 100;
        r.objects[0].w = 200; r.objects[0].h = 300;
        Serial.println("[YOLO stub] Simulated: person detected (confidence=0.85)");
    } else {
        Serial.println("[YOLO stub] Simulated: nothing detected");
    }

    return r;
}

// ====================================================
// Helper:檢查結果中有沒有指定類別
// ====================================================
inline bool yoloHasClass(const YoloResults& r, int classId, float minConfidence = 0.5) {
    for (int i = 0; i < r.count; i++) {
        if (r.objects[i].classId == classId &&
            r.objects[i].confidence >= minConfidence) {
            return true;
        }
    }
    return false;
}

#endif // CAMERA_YOLO_H
