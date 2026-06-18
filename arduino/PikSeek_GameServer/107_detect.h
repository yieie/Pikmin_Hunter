#ifndef DETECT_107_H
#define DETECT_107_H

#include "NNImageClassification.h"

// 宣告分類器物件
static NNImageClassification detect107;

enum DoorplateResult
{
    IS_107,
    NOT_107
};

// 初始化模型
inline void init107Detect()
{
    Serial.println("[107 Detect] Initializing model on Channel 1...");

    // 💡 建立一個只給 Channel 1 用的影像設定
    VideoSetting myAIConfig(128, 128, 10, VIDEO_RGB, 0);

    detect107.configVideo(myAIConfig);
    detect107.configInputImageColor(1); // 1 代表 IMAGERGB
    detect107.useModelMetaData(0);      // 0 代表 DISABLE_MODEL_META_DATA

    // 掛載你的 img_class_cnn.nb 模型到 MobileNetV2 插槽
    detect107.modelSelect(IMAGE_CLASSIFICATION, NA_MODEL, NA_MODEL, NA_MODEL, NA_MODEL, DEFAULT_IMGCLASS_MOBILENETV2);
    detect107.begin();

    Serial.println("[107 Detect] Model ready on Channel 1.");
}

// 獲取目前門牌偵測狀態
inline DoorplateResult getDoorplateStatus()
{
    // 讀取最新的背景推論結果
    if (detect107.getResultCount() > 0)
    {
        ImageClassificationResult res = detect107.getResult(0);

        // 幫你在 Serial 視窗印出你模型的即時分數
        Serial.print("[107 Model Debug] Class ID: ");
        Serial.print(res.classID());
        Serial.print(" | Score: ");
        Serial.println(res.score());

        // 假設 ID 1 是 107 門牌，分數大於 80 判定成功
        if (res.classID() == 1 && res.score() > 80)
        {
            return IS_107;
        }
    }
    return NOT_107;
}

#endif