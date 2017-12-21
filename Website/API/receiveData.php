<?php

$AVG = 0;
$MAX = 0;
$MIN = 0;
$DATA = $_GET['data'];
$UNIX = time();

if(isset($_GET['data']))
{
	$con=mysqli_connect("127.0.0.1","user","pass","db");
	if (mysqli_connect_errno())
	{
		echo "Failed to connect to MySQL: " . mysqli_connect_error();
	}
	if(intval($DATA))
	{
		$query = "SELECT VALUE FROM wifiProd";
		$result = mysqli_query($con,$query);
		if(mysqli_num_rows($result) > 0)
		{
			mysqli_data_seek($result, 1); // Seek to AVG type
			$row = mysqli_fetch_row($result);
			$AVG = $row[0];

			mysqli_data_seek($result, 2); // Seek to MAX type
			$row = mysqli_fetch_row($result);	
			$MAX = $row[0];

			mysqli_data_seek($result, 3); // Seek to MIN type
			$row = mysqli_fetch_row($result);
			$MIN = $row[0];

			mysqli_free_result($result);
		}

		$NEW_AVG = (int)($AVG + $DATA)/2; // Calculate the new average
		$query = sprintf("UPDATE wifiProd SET VALUE = %d, UNIX = %d WHERE TYPE='AVG' LIMIT 1", $NEW_AVG, $UNIX);
		mysqli_query($con,$query);
		echo "Updated average<br>";

		if($DATA > $MAX) // If current data is the new maximum
		{
			$query = sprintf("UPDATE wifiProd SET VALUE = %d, UNIX = %d WHERE TYPE='MAX' LIMIT 1", $DATA, $UNIX);
			mysqli_query($con,$query);
			echo "Updated max<br>";
		}
		else if($DATA < $MIN) // Or if the current data is the new minimum
		{
			$query = sprintf("UPDATE wifiProd SET VALUE = %d, UNIX = %d WHERE TYPE='MIN' LIMIT 1", $DATA, $UNIX);
			mysqli_query($con,$query);
			echo "Updated min<br>";
		}
	}

	$query = sprintf("UPDATE wifiProd SET VALUE = '%s', UNIX = %d WHERE TYPE='CUR' LIMIT 1", $DATA, $UNIX); // Update current value
	mysqli_query($con,$query);
	echo "Updated current";
	mysqli_close($con);
}

?>