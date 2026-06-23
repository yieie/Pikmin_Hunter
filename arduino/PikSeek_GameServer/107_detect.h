#ifndef DETECT_107_H
#define DETECT_107_H

/*
 * 107 教室門牌辨識
 * =============================================================
 * 對齊 RTSPImageClassification 官方範例的成功設定
 *
 * 關鍵設定(跟官方範例一致):
 *   - DEFAULT_IMGCLASS  (custom CNN sequential model)
 *   - ICPostProcess     (custom CNN callback)
 *   - configInputImageColor(0) = IMAGERGB
 *   - useModelMetaData(1) = 開啟 metadata
 *   - 需要 StreamIO 把 channel 1 連到 imgclass
 *
 * 部署:把 img_class_cnn.nb 放到
 *   C:\Users\...\AppData\Local\Arduino15\packages\ideasHatch\
 *     hardware\AmebaPro2\4.1.1-Release\variants\common_nn_models\
 */

#include "VideoStream.h"
#include "StreamIO.h"
#include "NNImageClassification.h"
#include "ClassificationClassList.h"

// ============================================================
// 設定常數(跟官方範例一致)
// ============================================================
#define USE_MODEL_META_DATA_EN 1
#define IMAGERGB               0

// AI 模型專用 channel(channel 0 給網頁串流用)
#define CHANNEL_NN  3

// 模型輸入大小(跟訓練 input_shape=(128,128,3) 一致)
#define NN_WIDTH   128
#define NN_HEIGHT  128

// ============================================================
// 全域物件
// ============================================================
static NNImageClassification imgclass;
static StreamIO videoStreamerNN(1, 1);

// ============================================================
// 結果列舉
// ============================================================
enum DoorplateResult
{
    IS_107,
    NOT_107
};

// 內部狀態:儲存最新的辨識結果(由 callback 更新)
static volatile int g_latestClassId = -1;
static volatile int g_latestScore   = 0;

static volatile bool g_isLawActive = false;

inline void enableLawDetection() {
    g_latestClassId = -1; // 開啟前先清空舊的結果
    g_isLawActive = true;
}

inline void disableLawDetection() {
    g_isLawActive = false;
}

// ============================================================
// Callback(對應 custom CNN sequential model)
// 用 ICPostProcess 不是 ICPostProcess_MobileNetV2
// ============================================================

void ICPostProcess(std::vector<ImageClassificationResult> results)
{
    if(!g_isLawActive){
      return;
     }
    if (imgclass.getResultCount() > 0) {
        ImageClassificationResult top = results[0];
        int classId = (int)top.classID();
        int score   = top.score();

        // 更新全域狀態,讓主程式可以讀取
        g_latestClassId = classId;
        g_latestScore   = score;

        // Debug 印出
        if (classId >= 0 && classId < 2  && imgclassItemList[classId].filter) {
            Serial.print("[107 Model] class=");
            Serial.print(imgclassItemList[classId].imgclassName);
            Serial.print(" score=");
            Serial.println(score);
        }
    }
}

// ============================================================
// 初始化(在 setup() 呼叫)
// ============================================================
inline void init107Detect(VideoSetting &configNN)
{
    Serial.println("[107 Detect] Initializing custom CNN model...");

    // 設定 imgclass(對齊官方範例)
    imgclass.configVideo(configNN);
    imgclass.configInputImageColor(IMAGERGB);             // 0 = RGB
    imgclass.useModelMetaData(USE_MODEL_META_DATA_EN);    // 1 = 啟用 metadata
    imgclass.setResultCallback(ICPostProcess);            // custom CNN callback
    imgclass.modelSelect(IMAGE_CLASSIFICATION,
                         NA_MODEL, NA_MODEL, NA_MODEL, NA_MODEL,
                         DEFAULT_IMGCLASS);                // 對應 sequential CNN
    imgclass.begin();

    // 關鍵!用 StreamIO 把 channel 1 連到 imgclass
    // (沒這段的話,模型載入了但畫面沒餵進去,不會推論)
    videoStreamerNN.registerInput(Camera.getStream(CHANNEL_NN));
    videoStreamerNN.setStackSize();
    videoStreamerNN.setTaskPriority();
    videoStreamerNN.registerOutput(imgclass);
    if (videoStreamerNN.begin() != 0) {
        Serial.println("[107 Detect] StreamIO link failed");
        return;
    }

    Serial.println("[107 Detect] Model + StreamIO ready");
}

// ============================================================
// 取得門牌狀態(由 verifyLAW 呼叫)
// ============================================================
inline DoorplateResult getDoorplateStatus()
{
    if (g_latestClassId < 0) {
        // 還沒有任何推論結果
        return NOT_107;
    }

    // 確認你訓練時的 class 對應!
    // ImageDataGenerator 預設用字母順序:
    //   {'IS_107': 0, 'NOT_107': 1} → class 0 = IS_107
    //
    // 訓練時可以用 print(train_generator.class_indices) 確認
    // 如果順序反了,把下面的 == 0 改成 == 1
    if (g_latestClassId == 0 && g_latestScore > 80) {
        return IS_107;
    }
    return NOT_107;
}

#endif
