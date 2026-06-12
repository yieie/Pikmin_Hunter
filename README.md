# Pikmin_Hunter
物聯網期末專題

## 遊戲流程

1. LoRa 偵測到節點(玩家進入學院範圍)

2. 網頁跳「偵測到 XX 學院」

3. 網頁顯示「謎題提示」 ← 這個謎題是「告訴你 LoRa 裝置在哪」
       例:「我躲在 360 度的樓梯」→ 玩家自己去找旋轉樓梯

4. 玩家走到旋轉樓梯,找到 LoRa 裝置實體

5. 看到節點盒子上的「**通關任務**」  ← 例如 QR Code、或寫在盒子上的指示
       
    例:「拍一張畫面中有 3 台電腦的照片」
    
    例:「解這道數學題:_____」
    
    例:「找一個圓形物體放畫面中央」

6. 玩家完成任務,用 8735 進行辨識/驗證

7. 8735 透過 AI 模型驗證通過
8. 捕獲皮克敏成功

## 執行說明

1. 設定secrets.h

    參考secrets.h.example設定8735連接的wifi及密碼
手機要與8735在同一區網下

2. 燒錄arduino/Pikseek_GameServer/Pikseek_GameServer.ino至8735

    執行後在手機上輸入Serial Port就可以看到遊戲介面

3. 燒錄lora_node/PikSeek_Sender_ESP32/PikSeek_Sender_ESP32.ino至ESP32
    
    執行後8735即能收到ESP32傳送之各學院訊號

## 檔案架構
```
│
└ arduino
│   └ secrets.h.example #Wifi連接example
│   └ HTTPDisplayJPEGContinuous  #可以用來測試單一模型是否能執行
│   └ PikSeek_GameServer    #完整遊戲架構
│       └ PikSeek_GameServer.ino     #主程式
│       └ camera_yolo.h     # YOLO辨識
│       └ colleges.h        # 各學院提示文字
│       └ game_state.h      # 紀錄遊戲狀態
│       └ lora_handler.h    # 接收遊戲節點訊號及資訊
│       └ task_verification.h    #依據學院辨識不同內容
│       └ web_server.h    #存放遊戲網頁HTML
└ lora_node
    └ PikSeek_Sender_ESP32  #各遊戲節點傳送訊號
```

## 工作說明

### YOLO辨識
關聯檔案
- camera_yolo.h (辨識)
- task_verification.h (驗證)

可以參考Moodle 11_YOLOv7模型訓練_darknet.pdf

---

### DHT溫濕度感測完成
關聯檔案
- PikSeek_Sender_ESP32.ino (發送方)
- lora_handler.h (接收方)
- task_verification.h (驗證)

> [!NOTE] 
> 須注意ESP32發送方 其他學院也需要共用此程式
> 也就是只有工院會需要回傳溫溼度，其他只需要回傳距離信號

可以參考Moodle 08_LoRa應用

---

### 數字辨識
關聯檔案
- task_verification.h (驗證)