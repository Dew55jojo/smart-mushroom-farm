<?php include 'db.php'; ?>
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Mushroom Farm Dashboard</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
/* Modal CSS */
.modal {
  display: none;
  position: fixed;
  z-index: 1000;
  left:0; top:0;
  width:100%; height:100%;
  background: rgba(0,0,0,0.7);
}
.modal-content {
  background: #1e1e1e;
  margin: 10% auto;
  padding: 20px;
  border-radius: 12px;
  width: 80%; max-width: 600px;
  color: #fff;
}
.close { float:right; font-size:24px; cursor:pointer; }

/* Doughnut cards */
.card { width:150px; display:inline-block; margin:10px; text-align:center; cursor:pointer; }
</style>
</head>
<body>
<h2>Mushroom Farm Dashboard</h2>

<div class="cards">
  <div class="card">
    <h4>Temp IN</h4>
    <canvas id="chartTempIn"></canvas>
  </div>
  <div class="card">
    <h4>Humidity IN</h4>
    <canvas id="chartHumIn"></canvas>
  </div>
  <div class="card">
    <h4>CO2</h4>
    <canvas id="chartCO2"></canvas>
  </div>
</div>

<!-- Modal -->
<div id="sensorModal" class="modal">
  <div class="modal-content">
    <span class="close">&times;</span>
    <h3 id="modalTitle">Sensor Graph</h3>
    <canvas id="modalChart"></canvas>
    <p>
      <button onclick="loadModalData(currentSensor,'3d')">ย้อนหลัง 3 วัน</button>
      <button onclick="loadModalData(currentSensor,'7d')">ย้อนหลัง 7 วัน</button>
    </p>
  </div>
</div>

<script>
// Doughnut Chart data (ตัวอย่างเริ่มต้น)
function createDoughnut(id, color) {
  return new Chart(document.getElementById(id), {
    type:'doughnut',
    data:{ labels:['Value','Remaining'], datasets:[{data:[0,100], backgroundColor:[color,'#555']}] },
    options:{ rotation:-90, circumference:180, plugins:{legend:{display:false}}, cutout:'70%' }
  });
}

const chartTempIn = createDoughnut("chartTempIn","#FF5733");
const chartHumIn = createDoughnut("chartHumIn","#33C1FF");
const chartCO2 = createDoughnut("chartCO2","#28a745");

// Modal
const modal = document.getElementById("sensorModal");
const modalTitle = document.getElementById("modalTitle");
const modalCtx = document.getElementById("modalChart").getContext("2d");
let modalChart;
let currentSensor = "";

// ปิด modal
document.querySelector(".close").onclick = ()=> modal.style.display="none";
window.onclick = e=> { if(e.target==modal) modal.style.display="none"; };

// Fetch data จาก PHP
function loadModalData(sensor, period='3d'){
  currentSensor = sensor;
  fetch(`history_sensor.php?sensor=${sensor}&period=${period}`)
    .then(res=>res.json())
    .then(data=>{
      const labels = data.map(r=>r.ts_hour);
      const values = data.map(r=>parseFloat(r.value));
      if(modalChart) modalChart.destroy();
      modalChart = new Chart(modalCtx,{
        type:'line',
        data:{ labels:labels, datasets:[{label:sensor,data:values,borderColor:'orange',fill:false,tension:0.1}] },
        options:{ responsive:true }
      });
      modalTitle.innerText = sensor + " - History";
      modal.style.display = "block";
    });
}

// เชื่อม Doughnut คลิก popup
chartTempIn.canvas.onclick = ()=> loadModalData('temp_in');
chartHumIn.canvas.onclick = ()=> loadModalData('hum_in');
chartCO2.canvas.onclick = ()=> loadModalData('co2');
</script>
</body>
</html>
