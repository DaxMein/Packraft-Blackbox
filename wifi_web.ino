/*
  wifi_web.ino – ESP32 mit interaktiven Charts (korrigiert)
*/

#include <WiFi.h>
#include <WebServer.h>

// ===== externe Sensor‑Variablen =====
extern float temp, hum, press;
extern bool gpsActive;
extern bool sdAvailable;
extern float gpsLat, gpsLon;

// ===== globale Objekte =========================================
int connectedClients = 0;
static WebServer server(80);

const char *ssid     = "Packraft Blackbox";
const char *password = "}}Packraft_Blackbox25";

// =====================  CSS Styles =============================
const char CSS_STYLES[] PROGMEM = R"rawliteral(
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:Arial,sans-serif;background:#0f0f23;color:#ccc;line-height:1.6}
.container{max-width:1200px;margin:0 auto;padding:20px}
.header{text-align:center;margin-bottom:30px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);padding:20px;border-radius:12px}
.header h1{color:white;font-size:2.5em;margin-bottom:10px}
.status-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;margin-bottom:30px}
.status-card{background:#1a1a2e;border:2px solid #16213e;border-radius:8px;padding:15px;text-align:center}
.status-card.ok{border-color:#4CAF50;background:#0f2027}
.status-card.error{border-color:#f44336;background:#2d0f0f}
.status-label{font-size:12px;color:#888;margin-bottom:5px}
.status-value{font-size:18px;font-weight:bold}
.sensor-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(400px,1fr));gap:20px}
.sensor-card{background:#1a1a2e;border:1px solid #16213e;border-radius:12px;padding:25px;position:relative}
.sensor-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px}
.sensor-title{font-size:18px;font-weight:600;color:#64b5f6}
.sensor-value{font-size:36px;font-weight:bold;color:white}
.temp-value{color:#ff6b6b}.hum-value{color:#4ecdc4}.press-value{color:#45b7d1}
.chart-container{height:250px;background:#0f0f1e;border-radius:8px;position:relative;margin-top:15px;border:1px solid #333;cursor:crosshair}
.chart-svg{width:100%;height:100%;position:absolute}
.chart-line{fill:none;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.temp-line{stroke:#ff6b6b}.hum-line{stroke:#4ecdc4}.press-line{stroke:#45b7d1}
.chart-grid{stroke:#333;stroke-width:0.5;opacity:0.5}
.chart-axis{stroke:#555;stroke-width:1}
.y-label{fill:#aaa;font-size:11px}.x-label{fill:#aaa;font-size:10px}
.data-point{r:4;opacity:0;cursor:pointer;transition:all 0.2s ease}
.data-point:hover{opacity:1;r:6}
.data-point.temp{fill:#ff6b6b}.data-point.hum{fill:#4ecdc4}.data-point.press{fill:#45b7d1}
.data-point.selected{opacity:1;r:8;stroke:white;stroke-width:2}
.tooltip{position:absolute;background:rgba(0,0,0,0.9);border:2px solid #64b5f6;border-radius:8px;padding:12px;font-size:14px;pointer-events:none;z-index:1000;opacity:0;transition:opacity 0.2s ease}
.tooltip.visible{opacity:1}
.tooltip-header{color:#64b5f6;font-weight:bold;margin-bottom:8px}
.tooltip-value{font-size:18px;font-weight:bold;margin:4px 0}
.tooltip-time{color:#888;font-size:12px;margin-top:8px;font-family:monospace}
.detail-panel{position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);background:#1a1a2e;border:2px solid #64b5f6;border-radius:12px;padding:25px;min-width:300px;z-index:2000;display:none}
.detail-panel.visible{display:block}
.detail-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;border-bottom:1px solid #333;padding-bottom:10px}
.detail-title{color:#64b5f6;font-size:20px;font-weight:bold}
.close-btn{background:#ff4757;color:white;border:none;border-radius:6px;padding:8px 12px;cursor:pointer;font-weight:bold}
.detail-row{display:flex;justify-content:space-between;margin:10px 0;padding:8px 0;border-bottom:1px solid #333}
.detail-label{color:#aaa}.detail-value{font-weight:bold;font-family:monospace}

.range-selector{text-align:center;margin:30px 0}
.range-selector select{background:#1a1a2e;color:#ccc;border:2px solid #16213e;padding:10px 15px;border-radius:8px;font-size:16px;cursor:pointer}
@media (max-width:768px){.sensor-grid{grid-template-columns:1fr}.header h1{font-size:2em}.sensor-value{font-size:28px}.detail-panel{margin:20px;width:calc(100% - 40px)}}
</style>
)rawliteral";

// =====================  HTML Content =============================
const char HTML_CONTENT[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<title>Packraft Blackbox - Interaktiv</title>
)rawliteral";

const char HTML_BODY[] PROGMEM = R"rawliteral(
</head><body>
<div class="container">
  <div class="header">
    <h1>🚣‍♂️ Packraft Blackbox</h1>
    <div>Interaktive Umwelt-Überwachung</div>
  </div>
  
  <div class="status-grid">
    <div class="status-card" id="gps-card">
      <div class="status-label">GPS Signal</div>
      <div class="status-value" id="gps">--</div>
    </div>
    <div class="status-card" id="sd-card">
      <div class="status-label">SD Card</div>
      <div class="status-value" id="sd">--</div>
    </div>
    <div class="status-card">
      <div class="status-label">Connected</div>
      <div class="status-value" id="clients">0</div>
    </div>
    <div class="status-card">
      <div class="status-label">Last Update</div>
      <div class="status-value" id="last-update">--:--</div>
    </div>
  </div>

  <div class="sensor-grid">
    <div class="sensor-card">
      <div class="sensor-header">
        <span class="sensor-title">🌡️ Temperatur</span>
        <span class="sensor-value temp-value" id="temp-value">--.- °C</span>
      </div>
      <div class="chart-container" id="temp-container">
        <svg class="chart-svg" id="temp-chart" viewBox="0 0 400 200">
          <defs>
            <linearGradient id="tempGradient" x1="0%" y1="0%" x2="0%" y2="100%">
              <stop offset="0%" style="stop-color:#ff6b6b;stop-opacity:0.3" />
              <stop offset="100%" style="stop-color:#ff6b6b;stop-opacity:0" />
            </linearGradient>
          </defs>
        </svg>
      </div>
    </div>

    <div class="sensor-card">
      <div class="sensor-header">
        <span class="sensor-title">💧 Luftfeuchtigkeit</span>
        <span class="sensor-value hum-value" id="hum-value">--.- %</span>
      </div>
      <div class="chart-container" id="hum-container">
        <svg class="chart-svg" id="hum-chart" viewBox="0 0 400 200">
          <defs>
            <linearGradient id="humGradient" x1="0%" y1="0%" x2="0%" y2="100%">
              <stop offset="0%" style="stop-color:#4ecdc4;stop-opacity:0.3" />
              <stop offset="100%" style="stop-color:#4ecdc4;stop-opacity:0" />
            </linearGradient>
          </defs>
        </svg>
      </div>
    </div>

    <div class="sensor-card">
      <div class="sensor-header">
        <span class="sensor-title">🌪️ Luftdruck</span>
        <span class="sensor-value press-value" id="press-value">---- hPa</span>
      </div>
      <div class="chart-container" id="press-container">
        <svg class="chart-svg" id="press-chart" viewBox="0 0 400 200">
          <defs>
            <linearGradient id="pressGradient" x1="0%" y1="0%" x2="0%" y2="100%">
              <stop offset="0%" style="stop-color:#45b7d1;stop-opacity:0.3" />
              <stop offset="100%" style="stop-color:#45b7d1;stop-opacity:0" />
            </linearGradient>
          </defs>
        </svg>
      </div>
    </div>
  </div>

  <div class="range-selector">
    <select id="range">
      <option value="1">Letzte 1 Minute</option>
      <option value="5" selected>Letzte 5 Minuten</option>
      <option value="30">Letzte 30 Minuten</option>
      <option value="60">Letzte 1 Stunde</option>
      <option value="300">Letzte 5 Stunden</option>
    </select>
  </div>
</div>

<div class="tooltip" id="tooltip">
  <div class="tooltip-header" id="tooltip-header">Datenpunkt</div>
  <div>
    <div class="tooltip-value" id="tooltip-value">--</div>
    <div class="tooltip-time" id="tooltip-time">--:--:--</div>
  </div>
</div>

<div class="detail-panel" id="detail-panel">
  <div class="detail-header">
    <div class="detail-title" id="detail-title">Details</div>
    <button class="close-btn" onclick="closeDetail()">×</button>
  </div>
  <div id="detail-content"></div>
</div>
)rawliteral";

// =====================  JavaScript =============================
const char JAVASCRIPT_CODE[] PROGMEM = R"rawliteral(
<script>
let rangeMinutes=5,maxDataPoints=100;
let data={temp:[],hum:[],press:[]};
let selectedPoints={temp:null,hum:null,press:null};
const chartConfig={
  temp:{min:0,max:50,color:'#ff6b6b',unit:'°C',name:'Temperatur'},
  hum:{min:0,max:100,color:'#4ecdc4',unit:'%',name:'Luftfeuchtigkeit'},
  press:{min:0,max:5000,color:'#45b7d1',unit:'hPa',name:'Luftdruck'}
};

function addDataPoint(type,value){
  const now=Date.now();
  data[type].push({time:now,value:value});
  const cutoff=now-(rangeMinutes*60*1000);
  data[type]=data[type].filter(point=>point.time>cutoff);
  if(data[type].length>maxDataPoints)data[type]=data[type].slice(-maxDataPoints);
}

function formatTime(timestamp){
  const date=new Date(timestamp);
  return date.getHours().toString().padStart(2,'0')+':'+date.getMinutes().toString().padStart(2,'0');
}

function formatTimeDetailed(timestamp){
  const date=new Date(timestamp);
  return date.getHours().toString().padStart(2,'0')+':'+date.getMinutes().toString().padStart(2,'0')+':'+date.getSeconds().toString().padStart(2,'0');
}

function showTooltip(event,type,point,index){
  const tooltip=document.getElementById('tooltip');
  const config=chartConfig[type];
  document.getElementById('tooltip-header').textContent=config.name;
  document.getElementById('tooltip-value').textContent=point.value.toFixed(1)+' '+config.unit;
  document.getElementById('tooltip-time').textContent=formatTimeDetailed(point.time);
  tooltip.style.left=event.pageX+10+'px';
  tooltip.style.top=event.pageY-10+'px';
  tooltip.classList.add('visible');
}

function hideTooltip(){
  document.getElementById('tooltip').classList.remove('visible');
}

function selectPoint(type,point,index){
  selectedPoints[type]={point,index};
  createChart(type);
  showDetailPanel(type,point,index);
}

function showDetailPanel(type,point,index){
  const config=chartConfig[type];
  const panel=document.getElementById('detail-panel');
  const content=document.getElementById('detail-content');
  document.getElementById('detail-title').textContent=config.name+' - Punkt #'+(index+1);
  
  const date=new Date(point.time);
  const allData=data[type];
  const values=allData.map(p=>p.value);
  const avg=values.reduce((a,b)=>a+b,0)/values.length;
  const min=Math.min(...values);
  const max=Math.max(...values);
  
  content.innerHTML=`
    <div class="detail-row">
      <span class="detail-label">Wert:</span>
      <span class="detail-value" style="color:${config.color}">${point.value.toFixed(2)} ${config.unit}</span>
    </div>
    <div class="detail-row">
      <span class="detail-label">Zeit:</span>
      <span class="detail-value">${formatTimeDetailed(point.time)}</span>
    </div>
    <div class="detail-row">
      <span class="detail-label">Position:</span>
      <span class="detail-value">${index+1} von ${allData.length}</span>
    </div>
    <div class="detail-row">
      <span class="detail-label">Durchschnitt:</span>
      <span class="detail-value">${avg.toFixed(2)} ${config.unit}</span>
    </div>
    <div class="detail-row">
      <span class="detail-label">Min/Max:</span>
      <span class="detail-value">${min.toFixed(1)} / ${max.toFixed(1)} ${config.unit}</span>
    </div>
  `;
  panel.classList.add('visible');
}

function closeDetail(){
  document.getElementById('detail-panel').classList.remove('visible');
}

function clearSelection(type){
  selectedPoints[type]=null;
  createChart(type);
}

function createChart(type){
  const svg=document.getElementById(`${type}-chart`);
  const config=chartConfig[type];
  const points=data[type];
  if(!svg||points.length===0)return;
  
  const width=400,height=200;
  const margin={top:10,right:30,bottom:30,left:50};
  const chartWidth=width-margin.left-margin.right;
  const chartHeight=height-margin.top-margin.bottom;
  
  const defs=svg.querySelector('defs');
  svg.innerHTML='';
  if(defs)svg.appendChild(defs);
  
  const g=document.createElementNS('http://www.w3.org/2000/svg','g');
  g.setAttribute('transform',`translate(${margin.left}, ${margin.top})`);
  svg.appendChild(g);
  
  // Grid and labels
  for(let i=0;i<=5;i++){
    const y=(chartHeight/5)*i;
    const value=config.max-(config.max-config.min)*(i/5);
    
    const line=document.createElementNS('http://www.w3.org/2000/svg','line');
    line.setAttribute('x1','0');
    line.setAttribute('y1',y.toString());
    line.setAttribute('x2',chartWidth.toString());
    line.setAttribute('y2',y.toString());
    line.setAttribute('class','chart-grid');
    g.appendChild(line);
    
    const label=document.createElementNS('http://www.w3.org/2000/svg','text');
    label.setAttribute('x','-5');
    label.setAttribute('y',(y+3).toString());
    label.setAttribute('text-anchor','end');
    label.setAttribute('class','y-label');
    label.textContent=Math.round(value)+config.unit;
    g.appendChild(label);
  }
  
  // Time labels
  if(points.length>1){
    const timeSteps=Math.max(1,Math.floor(points.length/5));
    for(let i=0;i<points.length;i+=timeSteps){
      const x=(i/(points.length-1))*chartWidth;
      const time=formatTime(points[i].time);
      const label=document.createElementNS('http://www.w3.org/2000/svg','text');
      label.setAttribute('x',x.toString());
      label.setAttribute('y',(chartHeight+20).toString());
      label.setAttribute('text-anchor','middle');
      label.setAttribute('class','x-label');
      label.textContent=time;
      g.appendChild(label);
    }
  }
  
  // Axes
  const xAxis=document.createElementNS('http://www.w3.org/2000/svg','line');
  xAxis.setAttribute('x1','0');
  xAxis.setAttribute('y1',chartHeight.toString());
  xAxis.setAttribute('x2',chartWidth.toString());
  xAxis.setAttribute('y2',chartHeight.toString());
  xAxis.setAttribute('class','chart-axis');
  g.appendChild(xAxis);
  
  const yAxis=document.createElementNS('http://www.w3.org/2000/svg','line');
  yAxis.setAttribute('x1','0');
  yAxis.setAttribute('y1','0');
  yAxis.setAttribute('x2','0');
  yAxis.setAttribute('y2',chartHeight.toString());
  yAxis.setAttribute('class','chart-axis');
  g.appendChild(yAxis);
  
  // Data visualization
  if(points.length>1){
    const yRange=config.max-config.min;
    let linePath='';
    let areaPath=`M 0 ${chartHeight}`;
    
    points.forEach((point,index)=>{
      const x=(index/(points.length-1))*chartWidth;
      const y=chartHeight-((point.value-config.min)/yRange)*chartHeight;
      if(index===0){
        linePath+=`M ${x} ${y}`;
        areaPath+=` L ${x} ${y}`;
      }else{
        linePath+=` L ${x} ${y}`;
        areaPath+=` L ${x} ${y}`;
      }
    });
    
    areaPath+=` L ${chartWidth} ${chartHeight} Z`;
    
    const area=document.createElementNS('http://www.w3.org/2000/svg','path');
    area.setAttribute('d',areaPath);
    area.setAttribute('fill',`url(#${type}Gradient)`);
    g.appendChild(area);
    
    const line=document.createElementNS('http://www.w3.org/2000/svg','path');
    line.setAttribute('d',linePath);
    line.setAttribute('class',`chart-line ${type}-line`);
    g.appendChild(line);
    
    // Interactive points - always active
    points.forEach((point,index)=>{
      const x=(index/(points.length-1))*chartWidth;
      const y=chartHeight-((point.value-config.min)/yRange)*chartHeight;
      const circle=document.createElementNS('http://www.w3.org/2000/svg','circle');
      circle.setAttribute('cx',x.toString());
      circle.setAttribute('cy',y.toString());
      circle.setAttribute('class',`data-point ${type}`);
      
      if(selectedPoints[type]&&selectedPoints[type].index===index){
        circle.classList.add('selected');
      }
      
      circle.addEventListener('mouseenter',(e)=>showTooltip(e,type,point,index));
      circle.addEventListener('mouseleave',hideTooltip);
      circle.addEventListener('click',(e)=>{e.stopPropagation();selectPoint(type,point,index);});
      g.appendChild(circle);
    });
  }
}

async function poll(){
  try{
    const response=await fetch('/data.json');
    if(!response.ok)throw new Error(`HTTP ${response.status}`);
    const result=await response.json();
    
    addDataPoint('temp',result.temp);
    addDataPoint('hum',result.hum);
    addDataPoint('press',result.press);
    
    document.getElementById('temp-value').textContent=result.temp.toFixed(1)+' °C';
    document.getElementById('hum-value').textContent=result.hum.toFixed(1)+' %';
    document.getElementById('press-value').textContent=result.press.toFixed(1)+' hPa';
    
    const gpsCard=document.getElementById('gps-card');
    const gpsEl=document.getElementById('gps');
    if(result.gps){
      gpsEl.textContent='OK';
      gpsCard.className='status-card ok';
    }else{
      gpsEl.textContent='NO';
      gpsCard.className='status-card error';
    }
    
    const sdCard=document.getElementById('sd-card');
    const sdEl=document.getElementById('sd');
    if(result.sd){
      sdEl.textContent='OK';
      sdCard.className='status-card ok';
    }else{
      sdEl.textContent='NO';
      sdCard.className='status-card error';
    }
    
    document.getElementById('clients').textContent=result.clients;
    document.getElementById('last-update').textContent=new Date().toLocaleTimeString();
    
    createChart('temp');
    createChart('hum');
    createChart('press');
    
  }catch(error){
    console.error('Error:',error);
    document.getElementById('last-update').textContent='ERROR';
  }
}

document.getElementById('range').addEventListener('change',function(e){
  rangeMinutes=parseInt(e.target.value);
  data={temp:[],hum:[],press:[]};
});

document.addEventListener('DOMContentLoaded',function(){
  poll();
  setInterval(poll,2000);
});
</script>
</body></html>
)rawliteral";

// =======================  HANDLER ===============================
void handleRoot() {
  // Send HTML in parts to avoid memory issues
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  
  server.sendContent_P(HTML_CONTENT);
  server.sendContent_P(CSS_STYLES);
  server.sendContent_P(HTML_BODY);
  server.sendContent_P(JAVASCRIPT_CODE);
  
  server.sendContent(""); // End response
}

void handleDataJSON(){
  char json[200];
  snprintf(json, sizeof(json),
    "{\"temp\":%.1f,\"hum\":%.1f,\"press\":%.1f,\"gps\":%s,\"sd\":%s,\"clients\":%d}",
    temp, hum, press, 
    gpsActive ? "true" : "false",
    sdAvailable ? "true" : "false",
    connectedClients
  );
  server.send(200, "application/json", json);
}

void handleNotFound(){
  server.send(404, "text/plain", "File Not Found");
}

// =======================  WIFI / SERVER =========================
void initWiFiWeb(){
  Serial.println("Starting WiFi AP...");
  
  WiFi.mode(WIFI_AP);
  bool result = WiFi.softAP(ssid, password);
  
  if (result) {
    Serial.print("AP started successfully. IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Failed to start AP");
    return;
  }

  server.on("/", handleRoot);
  server.on("/data.json", handleDataJSON);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web server started");
}

void updateWiFiWeb(){
  server.handleClient();
  connectedClients = WiFi.softAPgetStationNum();
}

// ===== Legacy‑Wrapper für alte .inos ============================
void updateConnectedClients(){ 
  connectedClients = WiFi.softAPgetStationNum(); 
}

void initWiFi(){ 
  initWiFiWeb(); 
}

void updateWebServer(){ 
  updateWiFiWeb(); 
}