<?php


$con=mysqli_connect("127.0.0.1","user","pass","db");
if (mysqli_connect_errno())
{
	echo "Failed to connect to MySQL: " . mysqli_connect_error();
}
$result = mysqli_query($con,"SELECT * FROM wifiProd");
if (!$result) 
{
	$message  = 'Invalid query: ' . mysql_error() . "\n";
	$message .= 'Whole query: ' . $sql;
	die($message);
}
else
{
	$emparray  = array();
	while($row = mysqli_fetch_array($result))
	{
		$emparray[] = $row;
	}
	$encoded = json_encode($emparray);
	echo $encoded;
	mysqli_free_result($result);
}	
mysqli_close($con)
?>