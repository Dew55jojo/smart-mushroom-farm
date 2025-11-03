<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Smart Mushroom Farm Dashboard</title>
<link rel="stylesheet" href="style.css">
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<script src="https://unpkg.com/mqtt/dist/mqtt.min.js"></script>
<script src="https://unpkg.com/leaflet/dist/leaflet.js"></script>
<link rel="stylesheet" href="https://unpkg.com/leaflet/dist/leaflet.css"/>
</head>
<body>
<div class="container">
  <h1>Smart Mushroom Farm Dashboard</h1>

  <div class="dashboard">
    <div class="card">
      <div class="card-icon">🌡️</div>
      <div>Temp In</div>
      <canvas id="chartTempIn" height="100"></canvas>
      <div id="tempInValue">0°C</div>
    </div>
    <div class="card">
      <div class="card-icon">🌡️</div>
      <div>Temp Out</div>
      <canvas id="chartTempOut" height="100"></canvas>
      <div id="tempOutValue">0°C</div>
    </div>
    <div class="card">
      <div class="card-icon">💧</div>
      <div>Hum In</div>
      <canvas id="chartHumIn" height="100"></canvas>
      <div id="humInValue">0%</div>
    </div>
    <div class="card">
      <div class="card-icon">💧</div>
      <div>Hum Out</div>
      <canvas id="chartHumOut" height="100"></canvas>
      <div id="humOutValue">0%</div>
    </div>
    <div class="card">
      <div class="card-icon">🌬️</div>
      <div>CO2</div>
      <canvas id="chartCO2" height="100"></canvas>
      <div id="co2Value">0 ppm</div>
    </div>
    <div class="card">
      <div class="card-icon">💡</div>
      <div>LDR</div>
      <canvas id="chartLDR" height="100"></canvas>
      <div id="ldrValue">0 lux</div>
    </div>
  </div>

  <div class="bottom-row">
  <!-- Control Panel -->
<div class="control-panel">
  <div class="section-title">Control Panel</div>

  <div class="control-item">
    <span>☀️ WED</span>
    <div id="SOL" class="toggle"></div>
  </div>

  <div class="control-item">
    <span>💧 EVAP</span>
    <div id="EVAP" class="toggle"></div>
  </div>

  <div class="control-item">
    <span>🌬️ FAN</span>
    <div id="FAN" class="toggle"></div>
  </div>

  <div class="control-item">
    <span>💡 LED</span>
    <div id="LED" class="toggle"></div>
  </div>

  <div class="control-item">
    <span>☀️ WATER</span>
    <div id="SOL2" class="toggle"></div>
  </div>
</div>


  <div class="map-container" id="map"></div>

</div>

<!-- Modal -->
<div id="sensorModal" class="modal">
  <div class="modal-content" id="modalContent">
    <span class="close">&times;</span>
    <h3 id="modalTitle">Sensor History</h3>
    <canvas id="modalChart" height="400"></canvas>
    <div>
      <button onclick="switchPeriod('3d')">3 Days</button>
      <button onclick="switchPeriod('7d')">7 Days</button>
    </div>
  </div>
</div>

<script src="app.js"></script>
</body>
</html>
