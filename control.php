<?php
// control.php
$relay = $_GET['relay'] ?? '';
$file = 'status.json';

if (!file_exists($file)) exit;

$status = json_decode(file_get_contents($file), true);

// toggle relay
if (isset($status[$relay])) {
    $status[$relay] = !$status[$relay];

    // ในระบบจริง: ส่งคำสั่ง MQTT/HTTP ไป ESP32
    // ตัวอย่าง: mqtt_publish($relay, $status[$relay] ? 'ON' : 'OFF');

    file_put_contents($file, json_encode($status));
    echo json_encode(['relay'=>$relay,'state'=>$status[$relay]]);
}
