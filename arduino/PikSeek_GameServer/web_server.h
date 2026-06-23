/*
 *  web_server.h - Web 伺服器 + MJPEG 串流 + 遊戲網頁
 *  =============================================================
 *  【A 負責】此檔的開發責任
 *
 *  包含:
 *    - WiFiServer 物件(port 80 網頁 / port 8080 串流)
 *    - webTask:處理 HTTP 請求,路由分發
 *    - streamTask:MJPEG 串流(沿用 multitask 範例邏輯)
 *    - API 端點:/api/state、/api/trigger、/api/verify、/api/reset
 *    - 遊戲網頁 HTML(內嵌)
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include "VideoStream.h"
#include "game_state.h"
#include "colleges.h"

// ====================================================
// Server 物件
// ====================================================
WiFiServer webServer(80, TCP_MODE, NON_BLOCKING_MODE);
WiFiServer streamServer(8080, TCP_MODE, NON_BLOCKING_MODE);

// ====================================================
// HTTP 工具
// ====================================================
void sendHttpHeader(WiFiClient &c, const char* contentType) {
    c.print("HTTP/1.1 200 OK\r\n");
    c.print("Content-Type: ");
    c.print(contentType);
    c.print("\r\n");
    c.print("Cache-Control: no-cache\r\n");
    c.print("Access-Control-Allow-Origin: *\r\n");
    c.print("Connection: close\r\n\r\n");
}

String escapeJson(const String& s) {
    String out = "";
    for (int i = 0; i < (int)s.length(); i++) {
        char c = s[i];
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

String urlDecode(const String& s) {
    String out = "";
    for (int i = 0; i < (int)s.length(); i++) {
        char c = s[i];
        if (c == '+') out += ' ';
        else if (c == '%' && i + 2 < (int)s.length()) {
            char hex[3] = {s[i+1], s[i+2], 0};
            out += (char)strtol(hex, NULL, 16);
            i += 2;
        } else out += c;
    }
    return out;
}

String getQueryParam(const String& req, const String& key) {
    int qIdx = req.indexOf('?');
    if (qIdx < 0) return "";
    int spaceIdx = req.indexOf(' ', qIdx);
    String query = req.substring(qIdx + 1, spaceIdx > 0 ? spaceIdx : req.length());
    String search = key + "=";
    int start = query.indexOf(search);
    if (start < 0) return "";
    start += search.length();
    int end = query.indexOf('&', start);
    if (end < 0) end = query.length();
    return urlDecode(query.substring(start, end));
}

// ====================================================
// 遊戲網頁 HTML
// ====================================================
const char GAME_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="zh-TW"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0">
<title>皮探隊</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{height:100%}
body{font-family:-apple-system,"Microsoft JhengHei",sans-serif;background:#0f172a;background-image:radial-gradient(circle at 20% 0%,rgba(74,222,128,.15),transparent 50%),radial-gradient(circle at 80% 100%,rgba(168,85,247,.15),transparent 50%);color:#e2e8f0;height:100vh;height:100dvh;overflow:hidden;user-select:none}
.c{max-width:540px;margin:0 auto;height:100%;display:flex;flex-direction:column}
.top-fixed{flex-shrink:0;padding:12px 12px 0 12px;position:relative;z-index:2;box-shadow:0 10px 16px -10px rgba(0,0,0,.5)}
.scroll-area{flex:1;min-height:0;overflow-y:auto;-webkit-overflow-scrolling:touch;padding:10px 12px 28px 12px}
header{text-align:center;margin-bottom:12px}
.lg{font-size:28px}
h1{font-size:18px;color:#4ade80;letter-spacing:2px;margin:2px 0}
.s{color:#64748b;font-size:11px}
.fr{background:#000;border-radius:12px;overflow:hidden;aspect-ratio:4/3;position:relative;margin-bottom:10px;box-shadow:0 20px 40px rgba(0,0,0,.5)}
.fr img{width:100%;height:100%;object-fit:contain;display:block}
.lv{position:absolute;top:8px;left:8px;background:rgba(0,0,0,.7);padding:4px 8px;border-radius:10px;font-size:9px;font-weight:700;display:flex;align-items:center;gap:4px;z-index:2}
.dot{width:6px;height:6px;border-radius:50%;background:#4ade80;box-shadow:0 0 6px #4ade80;animation:p 1.5s infinite}
.dot.off{background:#ef4444;animation:none}
@keyframes p{0%,100%{opacity:1}50%{opacity:.3}}
.rs{position:absolute;top:8px;right:8px;background:rgba(0,0,0,.7);padding:4px 8px;border-radius:10px;font-size:9px;color:#94a3b8;font-family:monospace;z-index:2}
.cd{background:rgba(30,41,59,.6);border:1px solid rgba(148,163,184,.1);border-radius:12px;padding:14px;margin-bottom:10px}
.lb{font-size:10px;color:#94a3b8;text-transform:uppercase;letter-spacing:1px;margin-bottom:6px}
.tk{font-size:13px;line-height:1.6;color:#e2e8f0;white-space:pre-wrap}
.cd.waiting{border-color:rgba(100,116,139,.3)}
.cd.detected{border-color:rgba(74,222,128,.5);background:rgba(74,222,128,.08);animation:pulse 1s infinite}
@keyframes pulse{0%,100%{box-shadow:0 0 0 0 rgba(74,222,128,0.4)}50%{box-shadow:0 0 0 8px rgba(74,222,128,0)}}
.cd.verifying{border-color:rgba(96,165,250,.5);background:rgba(96,165,250,.08)}
.cd.captured{border-color:rgba(250,204,21,.5);background:rgba(250,204,21,.1)}
.eng-meter{margin-top:10px;height:24px;background:#1e293b;border-radius:6px;overflow:hidden;position:relative}
.eng-bar{height:100%;background:linear-gradient(90deg,#3b82f6,#4ade80);transition:width .3s}
.eng-text{position:absolute;top:0;left:0;right:0;bottom:0;display:flex;align-items:center;justify-content:center;font-size:12px;font-weight:700;color:#fff;text-shadow:0 1px 2px rgba(0,0,0,.5)}
.pg{display:flex;gap:5px;margin-top:8px}
.pk{flex:1;aspect-ratio:1;background:#1e293b;border-radius:8px;display:flex;flex-direction:column;align-items:center;justify-content:center;font-size:18px;opacity:.25;transition:.3s;border:1px solid transparent}
.pk.got{opacity:1;background:rgba(74,222,128,.15);border-color:rgba(74,222,128,.3)}
.pk .nm{font-size:9px;color:#94a3b8;margin-top:2px}
.pk.cur{border-color:#4ade80;opacity:1;animation:cur 1s infinite}
@keyframes cur{0%,100%{transform:scale(1)}50%{transform:scale(1.05)}}
.ac{display:grid;grid-template-columns:1fr;gap:8px;margin-top:8px}
button{background:#4ade80;color:#0f172a;border:none;padding:14px;border-radius:10px;font-size:14px;font-weight:700;cursor:pointer;transition:.15s}
button:active{transform:scale(.96)}
button.s2{background:#334155;color:#e2e8f0}
button.warn{background:#ef4444;color:#fff}
button:disabled{opacity:.5;cursor:not-allowed}
.ts{position:fixed;bottom:24px;left:50%;transform:translateX(-50%) translateY(80px);background:#4ade80;color:#0f172a;padding:12px 20px;border-radius:10px;font-weight:700;opacity:0;transition:.3s;z-index:99;max-width:90%;text-align:center}
.ts.sh{transform:translateX(-50%) translateY(0);opacity:1}
.ts.err{background:#ef4444;color:#fff}
.dev{margin-top:14px;padding-top:14px;border-top:1px dashed #334155}
.dev-lb{font-size:9px;color:#64748b;text-transform:uppercase;margin-bottom:6px}
.dev-row{display:grid;grid-template-columns:repeat(5,1fr);gap:4px}
.dev-btn{background:#334155;color:#94a3b8;padding:8px;font-size:11px;border-radius:6px;border:none;cursor:pointer}
.hint-btn{width:100%;background:#334155;color:#e2e8f0;border:1px dashed #64748b;padding:10px;border-radius:10px;font-size:12px;font-weight:700;cursor:pointer;margin-top:10px}
.hint-btn:active{transform:scale(.97)}
.quest-box{margin-top:8px;background:rgba(96,165,250,.08);border:1px solid rgba(96,165,250,.3);border-radius:10px;padding:12px;animation:qfade .25s ease}
@keyframes qfade{from{opacity:0;transform:translateY(-4px)}to{opacity:1;transform:translateY(0)}}
.quest-label{font-size:10px;color:#60a5fa;text-transform:uppercase;letter-spacing:1px;margin-bottom:6px;font-weight:700}
.quest-text{font-size:13px;line-height:1.6;white-space:pre-wrap;color:#e2e8f0}
.celebrate-overlay{position:fixed;inset:0;background:rgba(15,23,42,.88);backdrop-filter:blur(4px);display:flex;align-items:center;justify-content:center;z-index:200;opacity:0;pointer-events:none;transition:opacity .25s;padding:20px}
.celebrate-overlay.show{opacity:1;pointer-events:auto}
.celebrate-confetti{position:absolute;inset:0;overflow:hidden}
.confetti-piece{position:absolute;top:-10px;width:8px;height:14px;opacity:.9;animation:confetti-fall linear forwards;border-radius:2px}
@keyframes confetti-fall{to{transform:translateY(110vh) rotate(540deg);opacity:0}}
.celebrate-card{position:relative;z-index:1;background:linear-gradient(160deg,#1e293b,#0f172a);border:1px solid rgba(250,204,21,.4);border-radius:20px;padding:28px 24px;max-width:320px;text-align:center;box-shadow:0 20px 60px rgba(0,0,0,.6);animation:celpop .4s cubic-bezier(.34,1.56,.64,1)}
@keyframes celpop{from{transform:scale(.6) translateY(20px);opacity:0}to{transform:scale(1) translateY(0);opacity:1}}
.celebrate-emoji{font-size:56px;margin-bottom:8px;animation:celbounce 1.2s ease infinite}
@keyframes celbounce{0%,100%{transform:translateY(0)}50%{transform:translateY(-10px)}}
.celebrate-title{font-size:20px;font-weight:700;color:#facc15;letter-spacing:2px;margin-bottom:4px}
.celebrate-name{font-size:15px;color:#4ade80;font-weight:700;margin-bottom:12px}
.celebrate-msg{font-size:13px;line-height:1.6;color:#cbd5e1;white-space:pre-wrap;margin-bottom:18px}
.celebrate-close{background:#facc15;color:#0f172a;border:none;padding:12px 20px;border-radius:10px;font-weight:700;font-size:13px;cursor:pointer}
.celebrate-close:active{transform:scale(.96)}
</style></head><body>
<div class="c">
<div class="top-fixed">
<header><div class="lg">🌱</div><h1>皮 探 隊</h1><div class="s">PikSeek · v4.0</div></header>

<div class="fr">
<div class="lv"><span class="dot" id="dot"></span><span id="lvT">LIVE</span></div>
<div class="rs" id="rssi">--- dBm</div>
<img id="vid" alt="影像">
</div>
</div>

<div class="scroll-area">

<div class="cd" id="mainCard">
<div class="lb" id="mainLabel">📜 任務狀態</div>
<div class="tk" id="mainText">載入中...</div>
<div id="engMeter" style="display:none">
<div class="eng-meter"><div class="eng-bar" id="engBar"></div><div class="eng-text" id="engText">0%</div></div>
</div>
<div id="questBlock" style="display:none">
<button class="hint-btn" id="hintBtn" onclick="toggleHint()">💡 查看通關任務提示</button>
<div class="quest-box" id="questBox" style="display:none">
<div class="quest-label">📦 通關任務</div>
<div class="quest-text" id="questText"></div>
</div>
</div>
<div id="captureArea" style="display:none">
<div class="ac"><button onclick="doVerify()" id="capBtn">📷 捕獲!</button></div>
</div>
</div>

<div class="cd">
<div class="lb">🏛️ 皮克敏圖鑑(<span id="cnt">0</span>/5)</div>
<div class="pg">
<div class="pk" data-c="MGT"><span>💼</span><span class="nm">管院</span></div>
<div class="pk" data-c="SCI"><span>🔬</span><span class="nm">理院</span></div>
<div class="pk" data-c="HUM"><span>📚</span><span class="nm">人文</span></div>
<div class="pk" data-c="ENG"><span>🔧</span><span class="nm">工院</span></div>
<div class="pk" data-c="LAW"><span>⚖️</span><span class="nm">法院</span></div>
</div>
</div>

<div class="cd">
<div class="dev-lb">🧪 開發測試(正式版用 LoRa 觸發)</div>
<div class="dev-row">
<button class="dev-btn" onclick="trig('MGT')">💼</button>
<button class="dev-btn" onclick="trig('SCI')">🔬</button>
<button class="dev-btn" onclick="trig('HUM')">📚</button>
<button class="dev-btn" onclick="trig('ENG')">🔧</button>
<button class="dev-btn" onclick="trig('LAW')">⚖️</button>
</div>
<div class="ac" style="margin-top:8px;grid-template-columns:1fr 1fr">
<button class="s2" onclick="location.reload()">🔄 重整</button>
<button class="warn" onclick="reset()">🗑️ 清空</button>
</div>
</div>

</div>
</div>
<div class="ts" id="toast"></div>
<div class="celebrate-overlay" id="celebrateOverlay">
<div class="celebrate-confetti" id="confettiLayer"></div>
<div class="celebrate-card">
<div class="celebrate-emoji" id="celEmoji">🌱</div>
<div class="celebrate-title">捕獲成功!</div>
<div class="celebrate-name" id="celName"></div>
<div class="celebrate-msg" id="celMsg"></div>
<button class="celebrate-close" onclick="hideCelebration()">收下皮克敏 →</button>
</div>
</div>

<script>
const STREAM_URL = 'http://' + location.hostname + ':8080/';
const vid = document.getElementById('vid');
const dot = document.getElementById('dot');
const lvT = document.getElementById('lvT');
function loadStream(){ vid.src = STREAM_URL + '?t=' + Date.now(); }
vid.onload = ()=>{ dot.classList.remove('off'); lvT.textContent='LIVE'; };
vid.onerror = ()=>{
  dot.classList.add('off'); lvT.textContent='重連';
  setTimeout(loadStream, 2000);
};
loadStream();
document.addEventListener('visibilitychange', ()=>{
  if(document.visibilityState === 'visible') loadStream();
});

function show(msg, isErr){
  const t=document.getElementById('toast');
  t.textContent=msg;
  t.classList.toggle('err', !!isErr);
  t.classList.add('sh');
  setTimeout(()=>t.classList.remove('sh'),2500);
}

async function trig(c){
  try{ const r = await fetch('/api/trigger?c='+c); show(await r.text()); }
  catch(e){ show('連線失敗', true); }
}
async function reset(){
  if(!confirm('確定要清空?')) return;
  revealedHints.clear();
  await fetch('/api/reset'); show('已重置');
}
let curCollege = null;
const revealedHints = new Set();
function toggleHint(){
  if(!curCollege) return;
  if(revealedHints.has(curCollege)) revealedHints.delete(curCollege);
  else revealedHints.add(curCollege);
  renderHintState();
}
function renderHintState(){
  const btn = document.getElementById('hintBtn');
  const box = document.getElementById('questBox');
  const revealed = curCollege && revealedHints.has(curCollege);
  box.style.display = revealed ? 'block' : 'none';
  btn.textContent = revealed ? '🙈 隱藏通關任務' : '💡 查看通關任務提示';
}

let celebrateTimer = null;
function spawnConfetti(){
  const layer = document.getElementById('confettiLayer');
  layer.innerHTML = '';
  const colors = ['#4ade80','#facc15','#60a5fa','#f472b6','#a855f7','#fb923c'];
  for(let i=0;i<28;i++){
    const p = document.createElement('div');
    p.className = 'confetti-piece';
    p.style.left = (Math.random()*100) + '%';
    p.style.background = colors[i % colors.length];
    p.style.animationDuration = (1.6 + Math.random()*1.2) + 's';
    p.style.animationDelay = (Math.random()*0.4) + 's';
    layer.appendChild(p);
  }
}
function showCelebration(d){
  document.getElementById('celEmoji').textContent = d.emoji || '🌱';
  document.getElementById('celName').textContent = '✨ ' + (d.pikminName || '皮克敏');
  document.getElementById('celMsg').textContent = d.message || '';
  spawnConfetti();
  document.getElementById('celebrateOverlay').classList.add('show');
  if(navigator.vibrate) navigator.vibrate([100,50,100,50,300]);
  clearTimeout(celebrateTimer);
  celebrateTimer = setTimeout(hideCelebration, 4500);
}
function hideCelebration(){
  document.getElementById('celebrateOverlay').classList.remove('show');
  clearTimeout(celebrateTimer);
}

async function doVerify(){
  document.getElementById('capBtn').disabled = true;
  show('🧠 AI 推論中...');
  try{
    const r = await fetch('/api/verify');
    const t = await r.text();
    show(t, !t.includes('✨'));
  }catch(e){ show('連線失敗', true); }
  setTimeout(()=>{ document.getElementById('capBtn').disabled = false; }, 2500);
}

let lastState = -1;
async function syncState(){
  try{
    const r = await fetch('/api/state');
    const d = await r.json();
    document.getElementById('rssi').textContent = d.rssi + ' dBm';

    const list = d.captured.split(',').filter(x=>x);
    document.getElementById('cnt').textContent = list.length;
    document.querySelectorAll('.pk').forEach(p=>{
      p.classList.toggle('got', list.includes(p.dataset.c));
      p.classList.toggle('cur', p.dataset.c === d.college && d.state !== 0);
    });

    const card = document.getElementById('mainCard');
    const label = document.getElementById('mainLabel');
    const text = document.getElementById('mainText');
    const capArea = document.getElementById('captureArea');
    const engMeter = document.getElementById('engMeter');
    const questBlock = document.getElementById('questBlock');
    card.className = 'cd';
    capArea.style.display = 'none';
    engMeter.style.display = 'none';
    questBlock.style.display = 'none';

    if(d.state === 0){
      card.classList.add('waiting');
      label.textContent = '📜 任務狀態';
      text.textContent = '🔍 探索校園,尋找皮克敏的訊號...';
      curCollege = null;
    }
    else if(d.state === 1){
      card.classList.add('detected');
      label.textContent = '✨ 偵測到訊號!';
      text.textContent = '⚡ 偵測到 ' + (d.collegeName||'') + ' 的訊號!\n\n' + (d.pikminName||'') + ' 正在留下線索...';
    }
    else if(d.state === 2){
      // HINTING:預設只顯示謎題,通關任務需點擊「提示」才會出現
      curCollege = d.college;
      label.textContent = '🧩 ' + (d.pikminName||'') + ' 的線索';
      text.textContent = (d.locationHint||'');
      capArea.style.display = 'block';
      questBlock.style.display = 'block';
      document.getElementById('questText').textContent = d.questDesc||'';
      renderHintState();
      // 工院顯示濕度進度條
      if(d.college === 'ENG'){
        engMeter.style.display = 'block';
        const pct = Math.min(100, d.humidity||0);
        document.getElementById('engBar').style.width = pct + '%';
        document.getElementById('engText').textContent = '濕度 ' + pct + '% / 目標 80%';
      }
    }
    else if(d.state === 3){
      card.classList.add('verifying');
      label.textContent = '🧠 AI 推論中';
      text.textContent = '正在分析畫面,請稍候...';
    }
    else if(d.state === 4){
      card.classList.add('captured');
      label.textContent = '🎉 捕獲成功!';
      text.textContent = '✨ 捕獲了 ' + (d.pikminName||'') + '!\n\n' + (d.message||'');
    }

    if(d.state !== lastState){
      if(d.state === 1 && navigator.vibrate) navigator.vibrate([200,100,200]);
      if(d.state === 4) showCelebration(d);
    }
    lastState = d.state;
  }catch(e){}
}
setInterval(syncState, 1500);
syncState();
</script></body></html>)HTML";

// ====================================================
// 路由:遊戲網頁
// ====================================================
void handleGamePage(WiFiClient &c) {
    sendHttpHeader(c, "text/html; charset=UTF-8");
    const char* html = GAME_HTML;
    int len = strlen(html);
    int chunk = 512;
    for (int i = 0; i < len; i += chunk) {
        int sendLen = (i + chunk > len) ? (len - i) : chunk;
        c.write((const uint8_t*)(html + i), sendLen);
        vTaskDelay(2 / portTICK_PERIOD_MS);
    }
}

// ====================================================
// 路由:狀態 API(JSON)
// ====================================================
void handleStateApi(WiFiClient &c) {
    sendHttpHeader(c, "application/json");
    String json = "{";
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        json += "\"state\":"  + String((int)g_state) + ",";
        json += "\"college\":\"" + g_currentCollege + "\",";
        json += "\"captured\":\"" + g_collegesGot + "\",";
        json += "\"rssi\":"   + String(g_lastRssi) + ",";
        json += "\"snr\":"    + String(g_lastSnr) + ",";
        json += "\"humidity\":" + String(g_engHumidity) + ",";
        json += "\"temperature\":" + String(g_engTemperature) + ",";
        unsigned long age = g_lastLoraTime > 0 ? (millis() - g_lastLoraTime) : 99999;
        json += "\"loraAge\":" + String(age);

        const College* col = findCollege(g_currentCollege);
        if (col) {
            json += ",\"collegeName\":\"" + escapeJson(col->name) + "\"";
            json += ",\"pikminName\":\""  + escapeJson(col->pikminName) + "\"";
            json += ",\"emoji\":\""       + escapeJson(col->emoji) + "\"";
            json += ",\"locationHint\":\"" + escapeJson(col->locationHint) + "\"";
            json += ",\"questDesc\":\""    + escapeJson(col->questDesc) + "\"";
            json += ",\"message\":\""      + escapeJson(g_lastMessage) + "\"";
        }
        xSemaphoreGive(stateMutex);
    }
    json += "}";
    c.print(json);
}

// ====================================================
// 路由:🧪 手動觸發(測試用)
// ====================================================
void handleTriggerApi(WiFiClient &c, const String &req) {
    sendHttpHeader(c, "text/plain; charset=UTF-8");
    String code = getQueryParam(req, "c");
    if (code.length() == 0) { c.print("缺少參數 c"); return; }

    triggerCollegeDetection(code, -45);

    const College* col = findCollege(code);
    if (col) {
        c.print("✨ 觸發:"); c.print(col->name);
    } else {
        c.print("未知學院碼");
    }
}

// ====================================================
// 路由:捕獲驗證
// ====================================================
void handleVerifyApi(WiFiClient &c) {
    sendHttpHeader(c, "text/plain; charset=UTF-8");

    GameState s;
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        s = g_state;
        xSemaphoreGive(stateMutex);
    }

    if (s != STATE_HINTING) {
        c.print("請先觸發學院偵測");
        return;
    }

    // 喚醒 verificationTask 去跑 AI
    requestVerification();

    c.print("🧠 AI 推論中,請等候 1-3 秒...");
}

// ====================================================
// 路由:重置
// ====================================================
void handleResetApi(WiFiClient &c) {
    sendHttpHeader(c, "text/plain; charset=UTF-8");
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE) {
        g_state = STATE_WAITING;
        g_currentCollege = "";
        g_collegesGot = "";
        g_lastMessage = "";
        g_verifyResult = "";
        g_verifyResultReady = false;
        g_stateChangedAt = millis();
        xSemaphoreGive(stateMutex);
    }
    c.print("已重置");
}

// ====================================================
// HTTP 路由分發
// ====================================================
void handleHttpClient(WiFiClient &client) {
    String requestLine = "";
    char buf[256] = {0};
    int bufPos = 0;
    unsigned long timeout = millis() + 1500;

    while (client.connected() && millis() < timeout) {
        if (client.available()) {
            char c = client.read();
            if (c == '\n') { requestLine = String(buf); break; }
            else if (c != '\r' && bufPos < (int)sizeof(buf) - 1) buf[bufPos++] = c;
        } else {
            vTaskDelay(2 / portTICK_PERIOD_MS);
        }
    }
    if (requestLine.length() == 0) return;

    String log = "[WEB] " + requestLine + "\n";
    Serial.print(log);

    if      (requestLine.indexOf("GET /api/state")   >= 0) handleStateApi(client);
    else if (requestLine.indexOf("GET /api/trigger") >= 0) handleTriggerApi(client, requestLine);
    else if (requestLine.indexOf("GET /api/verify")  >= 0) handleVerifyApi(client);
    else if (requestLine.indexOf("GET /api/reset")   >= 0) handleResetApi(client);
    else if (requestLine.indexOf("GET /favicon.ico") >= 0) {
        client.print("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
    } else {
        handleGamePage(client);
    }
    client.flush();
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

// ====================================================
// Web Task
// ====================================================
void webTask(void *parameter) {
    webServer.begin();
    Serial.println("[Task] webTask started on port 80");
    while (true) {
        WiFiClient client = webServer.available();
        if (!client) { vTaskDelay(50 / portTICK_PERIOD_MS); continue; }
        handleHttpClient(client);
        client.stop();
    }
}

// ====================================================
// Stream Task(MJPEG)
// ====================================================
static char streamHeaderBuf[64];

void sendStreamHeader(WiFiClient &client) {
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Cache-Control: no-cache\r\nPragma: no-cache\r\n");
    client.print("Connection: close\r\nAccess-Control-Allow-Origin: *\r\n");
    client.print("Content-Type: multipart/x-mixed-replace; boundary=");
    client.println(PART_BOUNDARY);
    client.print("\r\n");
}

void streamTask(void *parameter) {
    streamServer.begin();
    Serial.println("[Task] streamTask started on port 8080");

    char lineBuf[128];
    int linePos = 0;

    while (true) {
        WiFiClient client = streamServer.available();
        if (!client) { vTaskDelay(50 / portTICK_PERIOD_MS); continue; }

        linePos = 0;
        memset(lineBuf, 0, sizeof(lineBuf));

        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                if (c == '\n') {
                    if (linePos == 0) {
                        sendStreamHeader(client);
                        client.print("--" PART_BOUNDARY "\r\n");
                        break;
                    } else { linePos = 0; memset(lineBuf, 0, sizeof(lineBuf)); }
                } else if (c != '\r' && linePos < (int)sizeof(lineBuf) - 1) {
                    lineBuf[linePos++] = c;
                }
            } else { vTaskDelay(1 / portTICK_PERIOD_MS); }
        }

        while (client.connected()) {
            uint32_t addr = 0, len = 0;
            Camera.getImage(CHANNEL, &addr, &len);
            if (!addr || !len) { vTaskDelay(1 / portTICK_PERIOD_MS); continue; }

            snprintf(streamHeaderBuf, sizeof(streamHeaderBuf),
                     "Content-Type: image/jpeg\r\nContent-Length: %lu\r\n\r\n", len);
            if (client.write((uint8_t*)streamHeaderBuf, strlen(streamHeaderBuf)) == 0) break;
            if (client.write((uint8_t*)addr, len) == 0) break;
            if (client.write((uint8_t*)"\r\n--" PART_BOUNDARY "\r\n",
                             strlen("\r\n--" PART_BOUNDARY "\r\n")) == 0) break;

            vTaskDelay(FRAME_INTERVAL);
        }
        client.stop();
    }
}

#endif // WEB_SERVER_H
