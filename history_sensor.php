<?php
header('Content-Type: application/json');
error_reporting(0);

$host='localhost'; $port='5432'; $dbname='mushroom_farm'; $user='postgres'; $password='postgres';
$conn = pg_connect("host=$host port=$port dbname=$dbname user=$user password=$password");
if(!$conn){ echo json_encode([]); exit; }

$sensor=$_GET['sensor']??'temp_in';ช
$period=$_GET['period']??'3d';
$days=$period==='3d'?3:7;

$allowed_sensors=['temp_in','temp_out','hum_in','hum_out','co2','ldr'];
if(!in_array($sensor,$allowed_sensors)){ echo json_encode([]); exit; }

$query="
SELECT date_trunc('hour', timestamp) AS ts_hour, AVG($sensor) AS value
FROM mushroomdata
WHERE timestamp >= NOW() - INTERVAL '$days days'
GROUP BY ts_hour
ORDER BY ts_hour ASC
";
$result=pg_query($conn,$query);
if(!$result){ echo json_encode([]); exit; }

$data=[];
while($row=pg_fetch_assoc($result)){
    $data[]= [
        'ts_hour'=>date('Y-m-d H:i',strtotime($row['ts_hour'])),
        'value'=>floatval($row['value'])
    ];
}
echo json_encode($data);
pg_close($conn);
