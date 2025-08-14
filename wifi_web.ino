/*
  wifi_web.ino – ESP32 mit ArduinoJson und persistenter Datenspeicherung
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ===== externe Sensor‑Variablen =====
extern float temp, hum, press;
extern bool gpsActive;
extern bool sdAvailable;
extern float gpsLat, gpsLon;

// ===== SD-Karten Zugriff =====
#include <SD.h>
#include <FS.h>

// ===== GPS Tracking =====
struct GPSPoint {
  float lat;
  float lon;
  unsigned long timestamp;
};

const int MAX_GPS_POINTS = 500; // Max GPS-Punkte im RAM
GPSPoint gpsTrack[MAX_GPS_POINTS];
int gpsTrackIndex = 0;
int gpsTrackCount = 0;
bool gpsTracking = false;
unsigned long gpsTrackStartTime = 0;

// ===== Datenhistorie =====
struct DataPoint {
  unsigned long timestamp;
  float temp;
  float hum;
  float press;
  bool gps;
  bool sd;
};

// Ringpuffer für kurzfristige Datenhistorie (RAM)
const int MAX_HISTORY_SIZE = 300; // 5 Minuten bei 1 Messung/Sekunde (für schnellen Zugriff)
DataPoint dataHistory[MAX_HISTORY_SIZE];
int historyIndex = 0;
int historyCount = 0;
unsigned long sessionStartMillis = 0;

// ===== globale Objekte =========================================
int connectedClients = 0;
static WebServer server(80);

const char *ssid     = "Packraft Blackbox";
const char *password = "}}Packraft_Blackbox25";

// Funktion zum Hinzufügen von Datenpunkten zur Historie
void addToHistory() {
  dataHistory[historyIndex].timestamp = millis();
  dataHistory[historyIndex].temp = temp;
  dataHistory[historyIndex].hum = hum;
  dataHistory[historyIndex].press = press;
  dataHistory[historyIndex].gps = gpsActive;
  dataHistory[historyIndex].sd = sdAvailable;
  
  historyIndex = (historyIndex + 1) % MAX_HISTORY_SIZE;
  if (historyCount < MAX_HISTORY_SIZE) {
    historyCount++;
  }
}

// =====================  CSS Styles =============================
const char CSS_STYLES[] PROGMEM = R"rawliteral(
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:Arial,sans-serif;background:#0f0f23;color:#ccc;line-height:1.6}
.container{max-width:1200px;margin:0 auto;padding:20px}
.header{text-align:center;margin-bottom:30px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);padding:20px;border-radius:12px}
.header h1{color:white;font-size:2.5em;margin-bottom:10px}
.status-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;margin-bottom:30px}
.status-card{background:#1a1a2e;border:2px solid #16213e;border-radius:8px;padding:15px;text-align:center;transition:all 0.3s ease}
.status-card.ok{border-color:#4CAF50;background:#0f2027;box-shadow:0 0 10px rgba(76,175,80,0.3)}
.status-card.error{border-color:#f44336;background:#2d0f0f;box-shadow:0 0 10px rgba(244,67,54,0.3)}
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
.data-point{r:3;fill-opacity:0;stroke-opacity:0;cursor:pointer;transition:all 0.2s ease}
.data-point.visible{fill-opacity:1;stroke-opacity:1;r:5}
.data-point.temp{fill:#ff6b6b;stroke:#ff6b6b}
.data-point.hum{fill:#4ecdc4;stroke:#4ecdc4}
.data-point.press{fill:#45b7d1;stroke:#45b7d1}
.tooltip{position:absolute;background:rgba(0,0,0,0.9);border:2px solid #64b5f6;border-radius:8px;padding:12px;font-size:14px;pointer-events:none;z-index:1000;opacity:0;transition:opacity 0.2s ease}
.tooltip.visible{opacity:1}
.tooltip-header{color:#64b5f6;font-weight:bold;margin-bottom:8px}
.tooltip-value{font-size:18px;font-weight:bold;margin:4px 0}
.tooltip-time{color:#888;font-size:12px;margin-top:8px;font-family:monospace}
.range-selector{text-align:center;margin:30px 0}
.range-selector select{background:#1a1a2e;color:#ccc;border:2px solid #16213e;padding:10px 15px;border-radius:8px;font-size:16px;cursor:pointer;transition:all 0.3s ease}
.range-selector select:hover{border-color:#64b5f6}
.data-info{text-align:center;color:#888;margin:10px 0;font-size:14px;padding:10px;background:#1a1a2e;border-radius:8px}
.data-info span{color:#64b5f6;font-weight:bold}
.map-card{background:#1a1a2e;border:1px solid #16213e;border-radius:12px;padding:25px;margin:20px 0}
.map-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:15px}
.map-title{font-size:18px;font-weight:600;color:#64b5f6}
.map-controls{display:flex;gap:10px}
.map-btn{background:#16213e;color:#ccc;border:2px solid #333;padding:8px 15px;border-radius:6px;cursor:pointer;font-size:14px;transition:all 0.3s}
.map-btn:hover:not(:disabled){border-color:#64b5f6;background:#1e2940}
.map-btn:disabled{opacity:0.5;cursor:not-allowed}
.map-btn.active{background:#4CAF50;border-color:#4CAF50;color:white}
.map-btn.pause-active{background:#ff9f43;border-color:#ff9f43;color:white}
.map-btn.stop{background:#16213e;border-color:#ff4757;color:#ff4757}
.map-btn.stop:hover:not(:disabled){background:#ff4757;color:white}
#map{height:400px;background:#0f0f1e;border-radius:8px;border:1px solid #333;position:relative}
#map-canvas{width:100%;height:100%;border-radius:8px}
.map-placeholder{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);color:#666;font-size:14px;text-align:center}
.map-error{color:#ff4757;padding:20px;text-align:center}
.track-info{margin-top:10px;color:#888;font-size:14px}
.track-info span{color:#64b5f6;font-weight:bold}
.stats-box{background:#1a1a2e;border:1px solid #16213e;border-radius:8px;padding:12px;margin:20px 0}
.stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:10px}
.stat-item{text-align:center}
.stat-label{color:#888;font-size:11px;margin-bottom:3px;font-family:monospace}
.stat-value{color:#64b5f6;font-size:14px;font-weight:600;font-family:'Courier New',monospace}
@media (max-width:768px){.sensor-grid{grid-template-columns:1fr}.header h1{font-size:2em}.sensor-value{font-size:28px}}
</style>
)rawliteral";

// =====================  HTML Content =============================
const char HTML_CONTENT[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Packraft Blackbox - Live Monitor</title>
)rawliteral";

const char HTML_BODY[] PROGMEM = R"rawliteral(
</head><body>
<div class="container">
  <div class="header">
    <h1>🚣‍♂️ Packraft Blackbox</h1>
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
  </div>

  <div class="range-selector">
    <label for="range">Zeitraum: </label>
    <select id="range">
      <option value="60">Letzte Minute</option>
      <option value="300" selected>Letzte 5 Minuten</option>
      <option value="1800">Letzte 30 Minuten</option>
      <option value="3600">Letzte Stunde</option>
      <option value="7200">Letzte 2 Stunden</option>
      <option value="18000">Letzte 5 Stunden</option>
      <option value="all">Alle Daten</option>
    </select>
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

    <div class="sensor-card" id="gps-card-container">
      <div class="sensor-header">
        <span class="sensor-title">📍 GPS Tracking</span>
        <div style="display:flex;gap:10px;align-items:center">
          <span class="gps-status" id="gps-status">GPS: --</span>
          <button class="expand-btn" onclick="toggleMapSize()" title="Vergrößern">⛶</button>
        </div>
      </div>
      <div class="map-controls" style="margin-bottom:10px">
        <button class="map-btn" id="track-start" onclick="startTracking()">▶ Start</button>
        <button class="map-btn" id="track-pause" onclick="pauseTracking()" disabled>⏸ Pause</button>
        <button class="map-btn stop" id="track-stop" onclick="stopTracking()" disabled>⏹ Stop</button>
      </div>
      <div id="map" style="height:250px">
        <canvas id="map-canvas"></canvas>
        <div class="map-placeholder" id="map-placeholder">
          GPS-Position wird geladen...
        </div>
      </div>
      <div class="track-info" id="track-info" style="margin-top:10px">
        <span id="track-points">0</span> Punkte | 
        Distanz: <span id="track-distance">0</span> m | 
        Zeit: <span id="track-time">00:00</span>
      </div>
    </div>
  </div>

  <div class="stats-box">
    <div class="stats-grid" id="stats-grid">
      <!-- Statistiken werden hier eingefügt -->
    </div>
  </div>

  <div class="data-info" id="data-info">
    <span class="loading">Lade Daten...</span>
  </div>
</div>

<div class="tooltip" id="tooltip">
  <div class="tooltip-header" id="tooltip-header">Datenpunkt</div>
  <div>
    <div class="tooltip-value" id="tooltip-value">--</div>
    <div class="tooltip-time" id="tooltip-time">--:--:--</div>
  </div>
</div>
)rawliteral";

// =====================  JavaScript =============================
const char JAVASCRIPT_CODE[] PROGMEM = R"rawliteral(
<script>
let rangeSeconds = 300;
let allData = {temp:[], hum:[], press:[]};
let sessionStartTime = null;
let lastHistoryLoad = 0;

// GPS Map Variablen
let map = null;
let trackLine = null;
let currentMarker = null;
let gpsPoints = [];
let isTracking = false;
let trackStartTime = null;
let lastGPSUpdate = 0;

const chartConfig = {
  temp: {min:-10, max:50, color:'#ff6b6b', unit:'°C', name:'Temperatur', fixedScale:false},
  hum: {min:0, max:100, color:'#4ecdc4', unit:'%', name:'Luftfeuchtigkeit', fixedScale:false},
  press: {min:900, max:2000, color:'#45b7d1', unit:'hPa', name:'Luftdruck', fixedScale:false}
};

// Initialisiere Karte
function initMap() {
  // Default Position (wird überschrieben wenn GPS verfügbar)
  map = L.map('map').setView([53.7, 10.1], 13);
  
  // OpenStreetMap Tiles
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '© OpenStreetMap contributors',
    maxZoom: 19
  }).addTo(map);
  
  // Marker für aktuelle Position
  currentMarker = L.circleMarker([53.7, 10.1], {
    radius: 8,
    fillColor: "#ff6b6b",
    color: "#fff",
    weight: 2,
    opacity: 1,
    fillOpacity: 0.8
  }).addTo(map);
}

// GPS Tracking Funktionen
function startTracking() {
  isTracking = true;
  trackStartTime = Date.now();
  gpsPoints = [];
  
  // UI Update
  document.getElementById('track-start').disabled = true;
  document.getElementById('track-start').classList.add('active');
  document.getElementById('track-pause').disabled = false;
  document.getElementById('track-pause').classList.remove('pause-active');
  document.getElementById('track-stop').disabled = false;
  
  // Sende Start-Befehl an ESP32
  fetch('/gps/start', {method: 'POST'});
  
  // Lösche Map
  drawMap();
}

function pauseTracking() {
  isTracking = false;
  
  // UI Update
  document.getElementById('track-start').disabled = false;
  document.getElementById('track-start').classList.remove('active');
  document.getElementById('track-pause').disabled = true;
  document.getElementById('track-pause').classList.add('pause-active');
  
  // Sende Pause-Befehl
  fetch('/gps/pause', {method: 'POST'});
}

function stopTracking() {
  isTracking = false;
  trackStartTime = null;
  gpsPoints = [];
  
  // UI Update
  document.getElementById('track-start').disabled = false;
  document.getElementById('track-start').classList.remove('active');
  document.getElementById('track-pause').disabled = true;
  document.getElementById('track-pause').classList.remove('pause-active');
  document.getElementById('track-stop').disabled = true;
  
  // Reset Timer
  document.getElementById('track-time').textContent = '00:00';
  document.getElementById('track-distance').textContent = '0';
  document.getElementById('track-points').textContent = '0';
  
  // Sende Stop-Befehl
  fetch('/gps/stop', {method: 'POST'});
  
  // Lösche Karte
  drawMap();
}

// Berechne Distanz zwischen zwei GPS-Punkten (Haversine)
function calculateDistance(lat1, lon1, lat2, lon2) {
  const R = 6371000; // Radius der Erde in Metern
  const dLat = (lat2 - lat1) * Math.PI / 180;
  const dLon = (lon2 - lon1) * Math.PI / 180;
  const a = Math.sin(dLat/2) * Math.sin(dLat/2) +
            Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
            Math.sin(dLon/2) * Math.sin(dLon/2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
  return R * c;
}

// Update GPS Track
async function updateGPSTrack() {
  try {
    const response = await fetch('/gps/track');
    if (!response.ok) return;
    
    const data = await response.json();
    
    if (data.points && data.points.length > 0) {
      gpsPoints = data.points;
      
      // Berechne Gesamtdistanz
      let totalDistance = 0;
      if (gpsPoints.length > 1) {
        for (let i = 1; i < gpsPoints.length; i++) {
          totalDistance += calculateDistance(
            gpsPoints[i-1].lat, gpsPoints[i-1].lon,
            gpsPoints[i].lat, gpsPoints[i].lon
          );
        }
      }
      
      document.getElementById('track-distance').textContent = Math.round(totalDistance);
      document.getElementById('track-points').textContent = gpsPoints.length;
      
      // Update Track Time
      if (trackStartTime) {
        const elapsed = Math.floor((Date.now() - trackStartTime) / 1000);
        const minutes = Math.floor(elapsed / 60);
        const seconds = elapsed % 60;
        document.getElementById('track-time').textContent = 
          `${minutes.toString().padStart(2,'0')}:${seconds.toString().padStart(2,'0')}`;
      }
      
      drawMap();
    }
  } catch (error) {
    console.error('Error updating GPS track:', error);
  }
}

// Update aktuelle GPS Position
function updateGPSPosition(lat, lon, active) {
  const gpsStatus = document.getElementById('gps-status');
  
  console.log('GPS Update:', {lat, lon, active}); // Debug
  
  if (active && lat !== 0 && lon !== 0) {
    gpsStatus.textContent = `GPS: OK`;
    gpsStatus.className = 'gps-status active';
    
    currentGPSPos = {lat, lon};
    
    // Zentriere Karte beim ersten GPS-Fix
    if (lastGPSUpdate === 0) {
      mapCenter = {lat, lon};
      document.getElementById('map-placeholder').style.display = 'none';
      console.log('GPS Fix erhalten, zentriere Karte auf:', mapCenter);
    }
    
    // Karte neu zeichnen
    if (mapCanvas && mapCtx) {
      drawMap();
    }
    
    lastGPSUpdate = Date.now();
  } else {
    gpsStatus.textContent = 'GPS: Kein Signal';
    gpsStatus.className = 'gps-status inactive';
    
    // Debug Info
    if (!active) {
      console.log('GPS nicht aktiv');
    } else if (lat === 0 && lon === 0) {
      console.log('GPS aktiv aber keine gültigen Koordinaten');
    }
  }
}

async function loadHistory() {
  try {
    // Sende den gewünschten Zeitraum mit
    const rangeValue = document.getElementById('range').value;
    const url = rangeValue === 'all' ? 
      '/history.json?range=999999' : 
      `/history.json?range=${rangeValue}`;
    
    const response = await fetch(url);
    if (!response.ok) return;
    
    const data = await response.json();
    
    // Prüfe auf Fehler
    if (data.error) {
      console.error('History error:', data.error);
      return;
    }
    
    allData = {temp:[], hum:[], press:[]};
    
    // Konvertiere Server-Zeit zu Browser-Zeit
    const now = Date.now();
    const serverNow = data.serverTime;
    const offset = now - serverNow;
    
    data.points.forEach(point => {
      const browserTime = point.t + offset;
      allData.temp.push({time: browserTime, value: point.temp});
      allData.hum.push({time: browserTime, value: point.hum});
      allData.press.push({time: browserTime, value: point.press});
    });
    
    if (allData.temp.length > 0 && !sessionStartTime) {
      sessionStartTime = allData.temp[0].time;
    }
    
    // Zeige Datenquelle an
    const info = document.getElementById('data-info');
    if (data.source === 'SD') {
      info.innerHTML += ` | Quelle: <span>SD-Karte</span>`;
    }
    
    updateDataInfo();
    updateStats();
    updateAllCharts();
    
  } catch (error) {
    console.error('Error loading history:', error);
  }
}

function getFilteredData(type) {
  if (rangeSeconds === 'all') {
    return allData[type];
  }
  const now = Date.now();
  const cutoff = now - (rangeSeconds * 1000);
  return allData[type].filter(point => point.time > cutoff);
}

function updateDataInfo() {
  const info = document.getElementById('data-info');
  const totalPoints = allData.temp.length;
  
  if (totalPoints === 0) {
    info.innerHTML = '<span class="loading">Warte auf Daten...</span>';
    return;
  }
  
  const sessionMinutes = sessionStartTime ? 
    Math.floor((Date.now() - sessionStartTime) / 60000) : 0;
  const visiblePoints = getFilteredData('temp').length;
  
  let sdInfo = '';
  if (window.lastSDInfo) {
    const freeMB = window.lastSDInfo.free;
    const totalMB = window.lastSDInfo.total;
    const usedPercent = totalMB > 0 ? ((totalMB - freeMB) / totalMB * 100).toFixed(1) : 0;
    sdInfo = ` | SD: <span>${freeMB} MB frei</span> (${usedPercent}% belegt)`;
  }
  
  info.innerHTML = `<span>${totalPoints}</span> Datenpunkte | ` +
                   `<span>${visiblePoints}</span> sichtbar | ` +
                   `Session: <span>${sessionMinutes}</span> Min` + 
                   sdInfo;
}

function updateStats() {
  const statsGrid = document.getElementById('stats-grid');
  const types = ['temp', 'hum', 'press'];
  let html = '';
  
  types.forEach(type => {
    const data = getFilteredData(type);
    if (data.length > 0) {
      const values = data.map(p => p.value);
      const min = Math.min(...values);
      const max = Math.max(...values);
      const avg = values.reduce((a, b) => a + b, 0) / values.length;
      const config = chartConfig[type];
      
      html += `
        <div class="stat-item">
          <div class="stat-label">${config.name}</div>
          <div class="stat-value">
            Min: ${min.toFixed(1)} | Avg: ${avg.toFixed(1)} | Max: ${max.toFixed(1)} ${config.unit}
          </div>
        </div>
      `;
    }
  });
  
  statsGrid.innerHTML = html;
}

function formatTime(timestamp) {
  const date = new Date(timestamp);
  return date.getHours().toString().padStart(2,'0') + ':' + 
         date.getMinutes().toString().padStart(2,'0');
}

function formatTimeDetailed(timestamp) {
  const date = new Date(timestamp);
  return date.getHours().toString().padStart(2,'0') + ':' + 
         date.getMinutes().toString().padStart(2,'0') + ':' +
         date.getSeconds().toString().padStart(2,'0');
}

function showTooltip(event, type, point) {
  const tooltip = document.getElementById('tooltip');
  const config = chartConfig[type];
  document.getElementById('tooltip-header').textContent = config.name;
  document.getElementById('tooltip-value').textContent = 
    point.value.toFixed(1) + ' ' + config.unit;
  document.getElementById('tooltip-time').textContent = 
    formatTimeDetailed(point.time);
  
  tooltip.style.left = (event.pageX + 10) + 'px';
  tooltip.style.top = (event.pageY - 10) + 'px';
  tooltip.classList.add('visible');
}

function hideTooltip() {
  document.getElementById('tooltip').classList.remove('visible');
}

function createChart(type) {
  const svg = document.getElementById(`${type}-chart`);
  const config = chartConfig[type];
  const points = getFilteredData(type);
  
  if (!svg || points.length === 0) return;
  
  const width = 400, height = 200;
  const margin = {top:10, right:30, bottom:30, left:50};
  const chartWidth = width - margin.left - margin.right;
  const chartHeight = height - margin.top - margin.bottom;
  
  // Auto-Skalierung mit vernünftigen Grenzen
  if (points.length > 0) {
    const values = points.map(p => p.value);
    const minVal = Math.min(...values);
    const maxVal = Math.max(...values);
    const range = maxVal - minVal;
    
    // Mindest-Range für bessere Darstellung
    let minRange;
    if (type === 'temp') {
      minRange = 5; // Mindestens 5°C Range
    } else if (type === 'press') {
      minRange = 20; // Mindestens 20 hPa Range
    } else {
      minRange = 10; // Mindestens 10% Range für Humidity
    }
    
    const actualRange = Math.max(range, minRange);
    const center = (minVal + maxVal) / 2;
    
    config.min = center - actualRange / 2 - actualRange * 0.2;
    config.max = center + actualRange / 2 + actualRange * 0.2;
    
    // Grenzen einhalten
    if (type === 'temp') {
      config.min = Math.max(-20, config.min);
      config.max = Math.min(60, config.max);
    } else if (type === 'press') {
      config.min = Math.max(900, config.min);
      config.max = Math.min(2000, config.max);
    } else if (type === 'hum') {
      config.min = Math.max(0, config.min);
      config.max = Math.min(100, config.max);
    }
  }
  
  const defs = svg.querySelector('defs');
  svg.innerHTML = '';
  if (defs) svg.appendChild(defs);
  
  const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
  g.setAttribute('transform', `translate(${margin.left}, ${margin.top})`);
  svg.appendChild(g);
  
  // Grid
  for (let i = 0; i <= 5; i++) {
    const y = (chartHeight / 5) * i;
    const value = config.max - (config.max - config.min) * (i / 5);
    
    const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
    line.setAttribute('x1', '0');
    line.setAttribute('y1', y.toString());
    line.setAttribute('x2', chartWidth.toString());
    line.setAttribute('y2', y.toString());
    line.setAttribute('class', 'chart-grid');
    g.appendChild(line);
    
    const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
    label.setAttribute('x', '-5');
    label.setAttribute('y', (y + 3).toString());
    label.setAttribute('text-anchor', 'end');
    label.setAttribute('class', 'y-label');
    label.textContent = value.toFixed(1);
    g.appendChild(label);
  }
  
  // X-Achse Labels
  if (points.length > 1) {
    const labelCount = Math.min(5, points.length);
    const step = Math.max(1, Math.floor(points.length / labelCount));
    
    for (let i = 0; i < points.length; i += step) {
      const x = (i / (points.length - 1)) * chartWidth;
      const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      label.setAttribute('x', x.toString());
      label.setAttribute('y', (chartHeight + 20).toString());
      label.setAttribute('text-anchor', 'middle');
      label.setAttribute('class', 'x-label');
      label.textContent = formatTime(points[i].time);
      g.appendChild(label);
    }
  }
  
  // Achsen
  const xAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
  xAxis.setAttribute('x1', '0');
  xAxis.setAttribute('y1', chartHeight.toString());
  xAxis.setAttribute('x2', chartWidth.toString());
  xAxis.setAttribute('y2', chartHeight.toString());
  xAxis.setAttribute('class', 'chart-axis');
  g.appendChild(xAxis);
  
  const yAxis = document.createElementNS('http://www.w3.org/2000/svg', 'line');
  yAxis.setAttribute('x1', '0');
  yAxis.setAttribute('y1', '0');
  yAxis.setAttribute('x2', '0');
  yAxis.setAttribute('y2', chartHeight.toString());
  yAxis.setAttribute('class', 'chart-axis');
  g.appendChild(yAxis);
  
  // Daten zeichnen
  if (points.length > 1) {
    const yRange = config.max - config.min;
    let linePath = '';
    let areaPath = `M 0 ${chartHeight}`;
    
    const maxDrawPoints = 150;
    let drawPoints = points;
    if (points.length > maxDrawPoints) {
      const step = Math.floor(points.length / maxDrawPoints);
      drawPoints = points.filter((_, i) => i % step === 0);
    }
    
    drawPoints.forEach((point, index) => {
      const x = (index / (drawPoints.length - 1)) * chartWidth;
      const y = chartHeight - ((point.value - config.min) / yRange) * chartHeight;
      
      if (index === 0) {
        linePath += `M ${x} ${y}`;
        areaPath += ` L ${x} ${y}`;
      } else {
        linePath += ` L ${x} ${y}`;
        areaPath += ` L ${x} ${y}`;
      }
    });
    
    areaPath += ` L ${chartWidth} ${chartHeight} Z`;
    
    const area = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    area.setAttribute('d', areaPath);
    area.setAttribute('fill', `url(#${type}Gradient)`);
    g.appendChild(area);
    
    const line = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    line.setAttribute('d', linePath);
    line.setAttribute('class', `chart-line ${type}-line`);
    g.appendChild(line);
    
    // Interaktive Punkte
    const interactivePoints = drawPoints.slice(-30);
    interactivePoints.forEach(point => {
      const idx = drawPoints.indexOf(point);
      const x = (idx / (drawPoints.length - 1)) * chartWidth;
      const y = chartHeight - ((point.value - config.min) / yRange) * chartHeight;
      
      const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
      circle.setAttribute('cx', x.toString());
      circle.setAttribute('cy', y.toString());
      circle.setAttribute('class', `data-point ${type}`);
      
      circle.addEventListener('mouseenter', (e) => showTooltip(e, type, point));
      circle.addEventListener('mouseleave', hideTooltip);
      
      g.appendChild(circle);
    });
  }
}

function updateAllCharts() {
  createChart('temp');
  createChart('hum');
  createChart('press');
}

async function poll() {
  try {
    const response = await fetch('/data.json');
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const result = await response.json();
    
    document.getElementById('temp-value').textContent = 
      result.temp.toFixed(1) + ' °C';
    document.getElementById('hum-value').textContent = 
      result.hum.toFixed(1) + ' %';
    document.getElementById('press-value').textContent = 
      result.press.toFixed(1) + ' hPa';
    
    const gpsCard = document.getElementById('gps-card');
    const gpsEl = document.getElementById('gps');
    if (result.gps) {
      gpsEl.textContent = 'OK';
      gpsCard.className = 'status-card ok';
    } else {
      gpsEl.textContent = 'NO';
      gpsCard.className = 'status-card error';
    }
    
    // Update GPS Position auf Karte
    if (result.gpsLat !== undefined && result.gpsLon !== undefined) {
      updateGPSPosition(result.gpsLat, result.gpsLon, result.gps);
    }
    
    // SD-Karten Info speichern
    if (result.sdFree !== undefined) {
      window.lastSDInfo = {
        free: result.sdFree,
        total: result.sdTotal
      };
      updateDataInfo();
    }
    
    const sdCard = document.getElementById('sd-card');
    const sdEl = document.getElementById('sd');
    if (result.sd) {
      sdEl.textContent = 'OK';
      sdCard.className = 'status-card ok';
    } else {
      sdEl.textContent = 'NO';
      sdCard.className = 'status-card error';
    }
    
    document.getElementById('clients').textContent = result.clients;
    
    // Historie alle 3 Sekunden neu laden
    if (Date.now() - lastHistoryLoad > 3000) {
      await loadHistory();
      lastHistoryLoad = Date.now();
    }
    
    // GPS Track updaten wenn aktiv
    if (isTracking) {
      await updateGPSTrack();
    }
    
  } catch (error) {
    console.error('Error:', error);
    document.getElementById('last-update').textContent = 'ERROR';
  }
}

document.getElementById('range').addEventListener('change', function(e) {
  const value = e.target.value;
  rangeSeconds = value === 'all' ? 'all' : parseInt(value);
  
  // Lade neue Daten wenn längerer Zeitraum gewählt wird
  loadHistory();
  
  updateDataInfo();
  updateStats();
  updateAllCharts();
});

document.addEventListener('DOMContentLoaded', function() {
  console.log('=== DOM geladen, starte Initialisierung ===');
  
  // Warte kurz bis alles geladen ist
  setTimeout(() => {
    console.log('Prüfe Map-Container...');
    const mapDiv = document.getElementById('map');
    if (mapDiv) {
      console.log('Map-Container gefunden, initialisiere Karte...');
      initMap();
    } else {
      console.error('FEHLER: Map-Container nicht gefunden!');
      // Versuche es nochmal nach 1 Sekunde
      setTimeout(() => {
        console.log('Zweiter Versuch Map zu initialisieren...');
        initMap();
      }, 1000);
    }
  }, 500);
  
  // Starte Daten-Polling
  poll();
  setInterval(poll, 2000);
  
  // Lade Historie
  setTimeout(loadHistory, 1000);
});
</script>
</body></html>
)rawliteral";

// =======================  HANDLER ===============================
void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  
  server.sendContent_P(HTML_CONTENT);
  server.sendContent_P(CSS_STYLES);
  server.sendContent_P(HTML_BODY);
  server.sendContent_P(JAVASCRIPT_CODE);
  
  server.sendContent("");
}

void handleDataJSON() {
  // SD-Karten Info berechnen
  uint32_t cardSize = 0;
  uint32_t freeSize = 0;
  
  if (sdAvailable) {
    // ESP32 SD library Methode für freien Speicher
    cardSize = SD.totalBytes() / (1024 * 1024); // Total in MB
    uint32_t usedSize = SD.usedBytes() / (1024 * 1024); // Used in MB
    freeSize = cardSize - usedSize;
  }
  
  char json[400];
  snprintf(json, sizeof(json),
    "{\"temp\":%.1f,\"hum\":%.1f,\"press\":%.1f,\"gps\":%s,\"sd\":%s,\"clients\":%d,\"gpsLat\":%.6f,\"gpsLon\":%.6f,\"sdFree\":%lu,\"sdTotal\":%lu}",
    temp, hum, press, 
    gpsActive ? "true" : "false",
    sdAvailable ? "true" : "false",
    connectedClients,
    gpsLat, gpsLon,
    freeSize, cardSize
  );
  
  // Debug output
  Serial.print("GPS Debug - Active: ");
  Serial.print(gpsActive);
  Serial.print(", Lat: ");
  Serial.print(gpsLat, 6);
  Serial.print(", Lon: ");
  Serial.println(gpsLon, 6);
  
  server.send(200, "application/json", json);
}

// Handler für historische Daten - kombiniert RAM und SD-Karte
void handleHistoryJSON() {
  DynamicJsonDocument doc(16384);
  doc["serverTime"] = millis();
  
  JsonArray pointsArray = doc.createNestedArray("points");
  
  // Prüfe ob nach längeren Zeiträumen gefragt wird (Parameter aus Query String)
  String rangeStr = server.arg("range");
  int requestedSeconds = 300; // Standard: 5 Minuten
  
  if (rangeStr != "") {
    requestedSeconds = rangeStr.toInt();
  }
  
  // Für kurze Zeiträume: Nutze RAM-Buffer
  if (requestedSeconds <= 300) {
    int startIdx = (historyCount < MAX_HISTORY_SIZE) ? 0 : historyIndex;
    
    for (int i = 0; i < historyCount; i++) {
      int idx = (startIdx + i) % MAX_HISTORY_SIZE;
      
      // Nur Daten innerhalb des Zeitraums
      unsigned long age = millis() - dataHistory[idx].timestamp;
      if (age <= (requestedSeconds * 1000UL)) {
        JsonObject point = pointsArray.createNestedObject();
        point["t"] = dataHistory[idx].timestamp;
        point["temp"] = serialized(String(dataHistory[idx].temp, 1));
        point["hum"] = serialized(String(dataHistory[idx].hum, 1));
        point["press"] = serialized(String(dataHistory[idx].press, 1));
      }
    }
  } 
  // Für längere Zeiträume: Lese von SD-Karte
  else if (sdAvailable) {
    File logFile = SD.open("/log.csv", FILE_READ);
    
    if (logFile) {
      // Skip header
      logFile.readStringUntil('\n');
      
      unsigned long cutoffTime = (millis() / 1000) - requestedSeconds;
      int pointCount = 0;
      int skipCounter = 0;
      
      // Berechne Skip-Rate basierend auf Zeitraum (max 300 Punkte)
      int skipRate = 1;
      if (requestedSeconds > 1800) skipRate = 5;     // >30min: jeden 5. Punkt
      if (requestedSeconds > 3600) skipRate = 10;    // >1h: jeden 10. Punkt
      if (requestedSeconds > 7200) skipRate = 30;    // >2h: jeden 30. Punkt
      if (requestedSeconds > 18000) skipRate = 60;   // >5h: jeden 60. Punkt
      
      while (logFile.available() && pointCount < 300) {
        String line = logFile.readStringUntil('\n');
        
        // Parse CSV line
        int commaIndex1 = line.indexOf(',');
        if (commaIndex1 > 0) {
          unsigned long timeStamp = line.substring(0, commaIndex1).toInt();
          
          // Nur Daten innerhalb des Zeitfensters
          if ((requestedSeconds == 999999) || (timeStamp >= cutoffTime)) {
            skipCounter++;
            
            if (skipCounter >= skipRate) {
              skipCounter = 0;
              
              int commaIndex2 = line.indexOf(',', commaIndex1 + 1);
              int commaIndex3 = line.indexOf(',', commaIndex2 + 1);
              int commaIndex4 = line.indexOf(',', commaIndex3 + 1);
              
              if (commaIndex4 > 0) {
                float tempVal = line.substring(commaIndex1 + 1, commaIndex2).toFloat();
                float humVal = line.substring(commaIndex2 + 1, commaIndex3).toFloat();
                float pressVal = line.substring(commaIndex3 + 1, commaIndex4).toFloat();
                
                JsonObject point = pointsArray.createNestedObject();
                point["t"] = timeStamp * 1000UL; // Konvertiere zu Millisekunden
                point["temp"] = serialized(String(tempVal, 1));
                point["hum"] = serialized(String(humVal, 1));
                point["press"] = serialized(String(pressVal, 1));
                
                pointCount++;
              }
            }
          }
        }
      }
      
      logFile.close();
      doc["source"] = "SD";
      doc["skipRate"] = skipRate;
    } else {
      doc["error"] = "SD read failed";
    }
  } else {
    doc["error"] = "SD not available";
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// GPS Tracking Handler
void handleGPSStart() {
  gpsTracking = true;
  gpsTrackCount = 0;
  gpsTrackIndex = 0;
  gpsTrackStartTime = millis();
  
  // Erstelle neue Track-Datei auf SD-Karte mit Zeitstempel
  if (sdAvailable) {
    char filename[32];
    unsigned long now = millis() / 1000;
    snprintf(filename, sizeof(filename), "/track_%lu.csv", now);
    
    File trackFile = SD.open(filename, FILE_WRITE);
    if (trackFile) {
      trackFile.println("Zeit (s),Latitude,Longitude,Distanz (m)");
      trackFile.close();
      Serial.print("GPS Track gestartet: ");
      Serial.println(filename);
    }
  }
  
  server.send(200, "text/plain", "GPS tracking started");
}

void handleGPSPause() {
  gpsTracking = false;
  server.send(200, "text/plain", "GPS tracking paused");
}

void handleGPSStop() {
  gpsTracking = false;
  
  // Speichere finalen Track-Status
  if (sdAvailable && gpsTrackCount > 0) {
    // Finde die neueste Track-Datei
    File root = SD.open("/");
    File file = root.openNextFile();
    String latestTrack = "";
    unsigned long latestTime = 0;
    
    while (file) {
      String name = file.name();
      if (name.startsWith("track_") && name.endsWith(".csv")) {
        unsigned long fileTime = name.substring(6, name.length() - 4).toInt();
        if (fileTime > latestTime) {
          latestTime = fileTime;
          latestTrack = name;
        }
      }
      file = root.openNextFile();
    }
    
    // Füge Zusammenfassung am Ende hinzu
    if (latestTrack != "") {
      File trackFile = SD.open("/" + latestTrack, FILE_APPEND);
      if (trackFile) {
        trackFile.println("# Track beendet");
        trackFile.print("# Punkte: ");
        trackFile.println(gpsTrackCount);
        trackFile.print("# Dauer: ");
        trackFile.print((millis() - gpsTrackStartTime) / 1000);
        trackFile.println(" Sekunden");
        trackFile.close();
      }
    }
  }
  
  gpsTrackCount = 0;
  gpsTrackIndex = 0;
  server.send(200, "text/plain", "GPS tracking stopped");
}

void handleGPSTrack() {
  DynamicJsonDocument doc(16384); // Größer für mehr Punkte
  JsonArray points = doc.createNestedArray("points");
  
  // Lese Track von SD-Karte wenn verfügbar
  if (sdAvailable && gpsTracking) {
    // Finde die neueste Track-Datei
    File root = SD.open("/");
    File file = root.openNextFile();
    String latestTrack = "";
    unsigned long latestTime = 0;
    
    while (file) {
      String name = file.name();
      if (name.startsWith("track_") && name.endsWith(".csv")) {
        unsigned long fileTime = name.substring(6, name.length() - 4).toInt();
        if (fileTime > latestTime) {
          latestTime = fileTime;
          latestTrack = name;
        }
      }
      file = root.openNextFile();
    }
    
    if (latestTrack != "") {
      File trackFile = SD.open("/" + latestTrack, FILE_READ);
      if (trackFile) {
        // Skip header
        trackFile.readStringUntil('\n');
        
        int pointCount = 0;
        int skipCounter = 0;
        
        // Downsampling für große Tracks
        int skipRate = 1;
        long fileSize = trackFile.size();
        if (fileSize > 50000) skipRate = 2;  // >50KB: jeden 2. Punkt
        if (fileSize > 100000) skipRate = 5; // >100KB: jeden 5. Punkt
        if (fileSize > 200000) skipRate = 10; // >200KB: jeden 10. Punkt
        
        while (trackFile.available() && pointCount < 500) {
          String line = trackFile.readStringUntil('\n');
          
          // Ignoriere Kommentare
          if (line.startsWith("#")) continue;
          
          skipCounter++;
          if (skipCounter >= skipRate) {
            skipCounter = 0;
            
            int comma1 = line.indexOf(',');
            int comma2 = line.indexOf(',', comma1 + 1);
            int comma3 = line.indexOf(',', comma2 + 1);
            
            if (comma2 > 0) {
              float lat = line.substring(comma1 + 1, comma2).toFloat();
              float lon = line.substring(comma2 + 1, comma3 > 0 ? comma3 : line.length()).toFloat();
              unsigned long time = line.substring(0, comma1).toInt();
              
              if (lat != 0 && lon != 0) {
                JsonObject point = points.createNestedObject();
                point["lat"] = lat;
                point["lon"] = lon;
                point["t"] = time * 1000UL;
                pointCount++;
              }
            }
          }
        }
        
        trackFile.close();
        doc["source"] = "SD";
        doc["file"] = latestTrack;
        doc["skipRate"] = skipRate;
      }
    }
  } else {
    // Fallback auf RAM-Buffer
    int startIdx = (gpsTrackCount < MAX_GPS_POINTS) ? 0 : gpsTrackIndex;
    
    for (int i = 0; i < gpsTrackCount; i++) {
      int idx = (startIdx + i) % MAX_GPS_POINTS;
      
      JsonObject point = points.createNestedObject();
      point["lat"] = gpsTrack[idx].lat;
      point["lon"] = gpsTrack[idx].lon;
      point["t"] = gpsTrack[idx].timestamp;
    }
    doc["source"] = "RAM";
  }
  
  doc["tracking"] = gpsTracking;
  doc["count"] = points.size();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// Funktion zum Hinzufügen von GPS-Punkten
void addGPSPoint() {
  if (!gpsTracking || !gpsActive || gpsLat == 0.0 || gpsLon == 0.0) return;
  
  // Nur alle 5 Sekunden einen Punkt hinzufügen
  static unsigned long lastGPSAdd = 0;
  if (millis() - lastGPSAdd < 5000) return;
  lastGPSAdd = millis();
  
  // Berechne Distanz zum letzten Punkt
  float distance = 0;
  if (gpsTrackCount > 0) {
    int lastIdx = (gpsTrackIndex - 1 + MAX_GPS_POINTS) % MAX_GPS_POINTS;
    float lastLat = gpsTrack[lastIdx].lat;
    float lastLon = gpsTrack[lastIdx].lon;
    
    // Haversine-Formel
    float R = 6371000; // Radius der Erde in Metern
    float dLat = (gpsLat - lastLat) * M_PI / 180;
    float dLon = (gpsLon - lastLon) * M_PI / 180;
    float a = sin(dLat/2) * sin(dLat/2) +
              cos(lastLat * M_PI / 180) * cos(gpsLat * M_PI / 180) *
              sin(dLon/2) * sin(dLon/2);
    float c = 2 * atan2(sqrt(a), sqrt(1-a));
    distance = R * c;
  }
  
  // Speichere in RAM-Buffer
  gpsTrack[gpsTrackIndex].lat = gpsLat;
  gpsTrack[gpsTrackIndex].lon = gpsLon;
  gpsTrack[gpsTrackIndex].timestamp = millis();
  
  gpsTrackIndex = (gpsTrackIndex + 1) % MAX_GPS_POINTS;
  if (gpsTrackCount < MAX_GPS_POINTS) {
    gpsTrackCount++;
  }
  
  // Speichere auf SD-Karte
  if (sdAvailable) {
    // Finde die neueste Track-Datei
    File root = SD.open("/");
    File file = root.openNextFile();
    String latestTrack = "";
    unsigned long latestTime = 0;
    
    while (file) {
      String name = file.name();
      if (name.startsWith("track_") && name.endsWith(".csv")) {
        unsigned long fileTime = name.substring(6, name.length() - 4).toInt();
        if (fileTime > latestTime) {
          latestTime = fileTime;
          latestTrack = name;
        }
      }
      file = root.openNextFile();
    }
    
    if (latestTrack != "") {
      File trackFile = SD.open("/" + latestTrack, FILE_APPEND);
      if (trackFile) {
        unsigned long elapsed = (millis() - gpsTrackStartTime) / 1000;
        trackFile.print(elapsed);
        trackFile.print(",");
        trackFile.print(gpsLat, 6);
        trackFile.print(",");
        trackFile.print(gpsLon, 6);
        trackFile.print(",");
        trackFile.println(distance, 1);
        trackFile.close();
      }
    }
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "File Not Found");
}

// =======================  WIFI / SERVER =========================
void initWiFiWeb() {
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
  server.on("/history.json", handleHistoryJSON);
  server.on("/gps/start", HTTP_POST, handleGPSStart);
  server.on("/gps/pause", HTTP_POST, handleGPSPause);
  server.on("/gps/stop", HTTP_POST, handleGPSStop);
  server.on("/gps/track", handleGPSTrack);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web server started");
  
  sessionStartMillis = millis();
}

void updateWiFiWeb() {
  server.handleClient();
  connectedClients = WiFi.softAPgetStationNum();
  
  // Daten zur Historie hinzufügen (alle 1 Sekunde)
  static unsigned long lastHistoryUpdate = 0;
  if (millis() - lastHistoryUpdate > 1000) {
    addToHistory();
    lastHistoryUpdate = millis();
  }
  
  // GPS-Punkte hinzufügen wenn Tracking aktiv
  addGPSPoint();
}

// ===== Legacy‑Wrapper für alte .inos ============================
void updateConnectedClients() { 
  connectedClients = WiFi.softAPgetStationNum(); 
}

void initWiFi() { 
  initWiFiWeb(); 
}

void updateWebServer() { 
  updateWiFiWeb(); 
}