document.addEventListener("DOMContentLoaded", () => {
  const broker = "broker.emqx.io",
    port = 8083,
    clientId = "webDashboard-" + Math.random().toString(16).substr(2, 8);
  const topicStatus = "farmingmushroom/status",
    topicControl = "farmingmushroom/control";
  const client = mqtt.connect(`ws://${broker}:${port}/mqtt`, { clientId });

  client.on("connect", () => {
    console.log("MQTT connected");
    client.subscribe(topicStatus);
  });

  // --- Chart setup ---
  Chart.register({
    id: "centerText",
    afterDraw(chart) {
      const {
        ctx,
        chartArea: { width, height },
      } = chart;
      ctx.save();
      ctx.font = "bold 16px Arial";
      ctx.fillStyle = "#fff";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      const value = chart.config.data.datasets[0].data[0];
      ctx.fillText(value, width / 2, height / 1.7);
    },
  });

  function createDoughnut(id, color, max) {
    return new Chart(document.getElementById(id), {
      type: "doughnut",
      data: {
        labels: ["Value", "Remaining"],
        datasets: [{ data: [0, max], backgroundColor: [color, "#555"] }],
      },
      options: {
        rotation: -90,
        circumference: 180,
        plugins: { legend: { display: false } },
        cutout: "70%",
      },
      plugins: ["centerText"],
    });
  }

  const chartTempIn = createDoughnut("chartTempIn", "#FF5733", 50);
  const chartTempOut = createDoughnut("chartTempOut", "#C70039", 50);
  const chartHumIn = createDoughnut("chartHumIn", "#33C1FF", 100);
  const chartHumOut = createDoughnut("chartHumOut", "#3385FF", 100);
  const chartCO2 = createDoughnut("chartCO2", "#28a745", 2000);
  const chartLDR = createDoughnut("chartLDR", "#FFC300", 1023);

  // --- Map setup ---
  const map = L.map("map").setView([16.74658630800237, 100.19607993768527], 15);
  L.tileLayer("http://{s}.google.com/vt/lyrs=s&x={x}&y={y}&z={z}", {
    maxZoom: 20,
    subdomains: ["mt0", "mt1", "mt2", "mt3"],
  }).addTo(map);
  const greenhouseIcon = L.icon({
    iconUrl: "greenhouse.png",
    iconSize: [30, 30],
    iconAnchor: [25, 50],
    popupAnchor: [0, -50],
  });
  L.marker([16.74658630800237, 100.19607993768527], { icon: greenhouseIcon })
    .addTo(map)
    .bindPopup("Mushroom Farm")
    .openPopup();

  // --- Modal setup ---
  let modalChart,
    currentSensor = "",
    currentPeriod = "3d";
  window.loadModalData = function (sensor, period = "3d") {
    currentSensor = sensor;
    currentPeriod = period;
    const modal = document.getElementById("sensorModal");
    const modalTitle = document.getElementById("modalTitle");
    const modalCtx = document.getElementById("modalChart").getContext("2d");
    modal.style.display = "flex";

    fetch(`history_sensor.php?sensor=${sensor}&period=${period}`)
      .then((res) => res.json())
      .then((data) => {
        const labels = data.map((r) => r.ts_hour);
        const values = data.map((r) => parseFloat(r.value));
        if (modalChart) modalChart.destroy();
        modalChart = new Chart(modalCtx, {
          type: "line",
          data: {
            labels,
            datasets: [
              {
                label: sensor,
                data: values,
                borderColor: "orange",
                fill: false,
              },
            ],
          },
          options: { responsive: true, maintainAspectRatio: false },
        });
        modalTitle.innerText = `${sensor} - History (${period})`;
      })
      .catch((err) => console.error("Fetch error:", err));
  };

  window.switchPeriod = function (period) {
    if (currentSensor) loadModalData(currentSensor, period);
  };
  document.querySelector(".close").onclick = () => {
    document.getElementById("sensorModal").style.display = "none";
  };

  // --- Doughnut click ---
  chartTempIn.canvas.onclick = () => loadModalData("temp_in");
  chartTempOut.canvas.onclick = () => loadModalData("temp_out");
  chartHumIn.canvas.onclick = () => loadModalData("hum_in");
  chartHumOut.canvas.onclick = () => loadModalData("hum_out");
  chartCO2.canvas.onclick = () => loadModalData("co2");
  chartLDR.canvas.onclick = () => loadModalData("ldr");

  // --- MQTT realtime update ---
  client.on("message", (topic, message) => {
    if (topic === topicStatus) {
      const data = message.toString().split(",");
      data.forEach((p) => {
        const [key, val] = p.split(":");
        const value = parseFloat(val);

        if (key === "TEMP_IN") {
          chartTempIn.data.datasets[0].data = [value, 50 - value];
          chartTempIn.update();
          document.getElementById("tempInValue").innerText = value + "°C";
        }
        if (key === "TEMP_OUT") {
          chartTempOut.data.datasets[0].data = [value, 50 - value];
          chartTempOut.update();
          document.getElementById("tempOutValue").innerText = value + "°C";
        }
        if (key === "HUM_IN") {
          chartHumIn.data.datasets[0].data = [value, 100 - value];
          chartHumIn.update();
          document.getElementById("humInValue").innerText = value + "%";
        }
        if (key === "HUM_OUT") {
          chartHumOut.data.datasets[0].data = [value, 100 - value];
          chartHumOut.update();
          document.getElementById("humOutValue").innerText = value + "%";
        }
        if (key === "CO2") {
          chartCO2.data.datasets[0].data = [value, 2000 - value];
          chartCO2.update();
          document.getElementById("co2Value").innerText = value + " ppm";
        }
        if (key === "LDR") {
          chartLDR.data.datasets[0].data = [value, 1023 - value];
          chartLDR.update();
          document.getElementById("ldrValue").innerText = value + " lux";
        }

        if (["SOL", "EVAP", "FAN", "LED", "SOL2"].includes(key)) {
          const toggle = document.getElementById(key);
          if (toggle) toggle.className = val == "1" ? "toggle on" : "toggle";
        }
      });
    }
  });

  // --- Toggle buttons for all devices ---
  ["SOL", "EVAP", "FAN", "LED", "SOL2"].forEach((r) => {
    const el = document.getElementById(r);
    if (!el) {
      console.error(`Element ${r} not found`);
      return;
    }

    el.addEventListener("click", () => {
      el.classList.toggle("on"); // Update local state
      const action = el.classList.contains("on") ? r + "_ON" : r + "_OFF";

      if (client.connected()) {
        client.publish(topicControl, action);
        console.log("MQTT published:", action);
      } else {
        console.error("MQTT not connected!");
      }
    });
  });
});
