/*
 * camera_yolo.h - 真實 YOLO 推論介面 (Ameba Pro2 8735)
 * =============================================================
 */

#ifndef CAMERA_YOLO_H
#define CAMERA_YOLO_H

#include <Arduino.h>
#include "NNObjectDetection.h"
#include "StreamIO.h"

// ====================================================
// COCO 80 類中的 ID (預設 DEFAULT_YOLOV4TINY 類別)
// ====================================================
#define YOLO_PERSON         0
#define YOLO_POTTED_PLANT   58   // 盆栽植物
#define YOLO_BOOK           73   // 書本

// ====================================================
// 偵測結果結構
// ====================================================
struct YoloDetection {
    int   classId;
    float confidence;
    int   x, y, w, h;   
};

#define MAX_DETECTIONS 16

struct YoloResults {
    int           count;
    YoloDetection objects[MAX_DETECTIONS];
};

// ====================================================
// 內部實體與快取變數
// ====================================================
static NNObjectDetection ObjDet;
static StreamIO videoStreamerNN(1, 1);

// 用於快取最新一次 NPU 背景辨識的結果，供 yoloDetect() 隨時調閱
static YoloResults g_latestYoloResults;
static SemaphoreHandle_t yoloCacheMutex = NULL;

// ====================================================
// NPU 物件辨識背景回呼函式 (Callback)
// ====================================================
inline void ODPostProcess(std::vector<ObjectDetectionResult> results) {
    int totalObjects = ObjDet.getResultCount();
    
    if (yoloCacheMutex == NULL) return;

    if (xSemaphoreTake(yoloCacheMutex, portMAX_DELAY) == pdTRUE) {
        g_latestYoloResults.count = 0;
        
        // 將結果寫入快取結構中
        for (int i = 0; i < totalObjects && i < MAX_DETECTIONS; i++) {
            g_latestYoloResults.objects[i].classId    = results[i].type();
            g_latestYoloResults.objects[i].confidence = (float)results[i].score() / 100.0; // 轉換為 0.0 ~ 1.0
            
            // 由於原任務主要是分類驗證，如果需要像素座標可再自行乘以解析度
            g_latestYoloResults.objects[i].x = (int)(results[i].xMin() * 100); 
            g_latestYoloResults.objects[i].y = (int)(results[i].yMin() * 100);
            g_latestYoloResults.objects[i].w = (int)((results[i].xMax() - results[i].xMin()) * 100);
            g_latestYoloResults.objects[i].h = (int)((results[i].yMax() - results[i].yMin()) * 100);
            
            g_latestYoloResults.count++;
        }
        xSemaphoreGive(yoloCacheMutex);
    }
}

// ====================================================
// 初始化 YOLO (在主 setup() 呼叫)
// ====================================================
inline void initYOLO(VideoSetting& configNN, int channelNN) {
    Serial.println("[YOLO] Real NPU Hardware Initializing...");

    // 建立快取保護鎖
    if (yoloCacheMutex == NULL) {
        yoloCacheMutex = xSemaphoreCreateMutex();
    }

    // 1. 設定物件偵測模型
    ObjDet.configVideo(configNN);
    ObjDet.setResultCallback(ODPostProcess);
    // 載入 8735 內建的 Tiny YOLOv4 模型
    ObjDet.modelSelect(OBJECT_DETECTION, DEFAULT_YOLOV4TINY, NA_MODEL, NA_MODEL);
    ObjDet.begin();

    // 2. 建立 StreamIO 管線將相機通道影像串接到 YOLO 模型中
    videoStreamerNN.registerInput(Camera.getStream(channelNN));
    videoStreamerNN.setStackSize();
    videoStreamerNN.setTaskPriority();
    videoStreamerNN.registerOutput(ObjDet);
    
    if (videoStreamerNN.begin() != 0) {
        Serial.println("[YOLO] ❌ StreamIO link start failed");
    } else {
        Serial.println("[YOLO] ✅ StreamIO link successful");
    }

    // 3. 啟用 NN 相機通道
    Camera.channelBegin(channelNN);
    Serial.println("[YOLO] ✅ NPU Model loaded. Live inference started.");
}

// ====================================================
// 跑一次 YOLO 推論 (獲取當下最新一影格的快取結果)
// ====================================================
inline YoloResults yoloDetect() {
    YoloResults currentResults;
    currentResults.count = 0;

    if (yoloCacheMutex != NULL) {
        if (xSemaphoreTake(yoloCacheMutex, portMAX_DELAY) == pdTRUE) {
            // 拷貝當前最新辨識結果
            currentResults = g_latestYoloResults;
            xSemaphoreGive(yoloCacheMutex);
        }
    }

    Serial.print("[YOLO Real] Retreived latest frame results. Object count = ");
    Serial.println(currentResults.count);
    
    // 印出目前畫面上偵測到的種類，除錯用
    // for(int i = 0; i < currentResults.count; i++) {
    // printf("   -> Obj [%d]: Class ID = %d, Confidence = %.2f\n", 
    //        i, currentResults.objects[i].classId, currentResults.objects[i].confidence);
    // }

    return currentResults;
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
