function initialize() 
{
	loadingBarUpdate = window.setInterval(loadingBarCount, 500);
	intervalUpdate = window.setInterval(updateCount, 1000);
}

function loadingBarCount()
{
	document.getElementById("loadingDiv").style.visibility = 'visible';
}

function updateCount()
{
    getJSON('https://stephen.glass/api/getQueueData.php', function(err, data) // Call API which queries SQL database for location data
    {
        if (err !== null) 
        {
          alert('Something went wrong fetching queue data: ' + err);
        } 
        else 
        {
	        updateData(data);
        }
    });
}

function updateData(data)
{
	var distanceAway = 255; // The measured distance from where the device is mounted to where the destination is

	var Booting = 0;
	var rawDistance = 0;
	var unixTime = 0;
	var numDistance = 0;
	var numPeople = 0;
	if(data[0].VALUE === "ESP")
	{
		Booting = 1;
	}
	else
	{
	    rawDistance = data[0].VALUE; // Lower distance = more people, higher distance = less people
	    numDistance = (distanceAway - rawDistance);
	    if(numDistance == 0) numPeople = 0;
	    else var numPeople = parseInt(numDistance/12); // Cannot be greater than 21
	    unixTime = data[0].UNIX;
	}

    var avgRawDistance = data[1].VALUE;
    var numAvgDistance = (distanceAway - avgRawDistance);
    if(numAvgDistance == 0) numAvgPeople = 0;
    else var numAvgPeople = parseInt(numAvgDistance/12);

    var maxUnixTime = data[2].UNIX;
    var maxRawDistance = data[2].VALUE;
    var numMaxDistance = (distanceAway - maxRawDistance);
    if(numMaxDistance == 0) numMaxPeople = 0;
    else var numMaxPeople = parseInt(numMaxDistance/12);

	var minUnixTime = data[3].UNIX;
    var minRawDistance = data[3].VALUE;
    var numMinDistance = (distanceAway - minRawDistance);
    if(numMinDistance == 0) numMinPeople = 0;
    else var numMinPeople = parseInt(numMinDistance/12);

    // Assuming 12 inches separation distance between people
    // Max length of sensor is 255 inches
    // Max people that can be displayed is 255/12 = 21
    var lineHTML = '<span style="font-size: 60px;">';
    for(i = 0; i < numPeople; i++)
    {
    	lineHTML += '<i class="fa fa-male" aria-hidden="true"></i> ';
    }
    lineHTML += '</span>';
    if(Booting == 0)
    {
	    document.getElementById("data_Length").innerHTML = lineHTML;
	    document.getElementById("data_numPeople").innerHTML = numPeople;
	    document.getElementById("data_numDistance").innerHTML = numDistance;
	    document.getElementById("data_lastUpdate").innerHTML = timeConverter(unixTime);
	    document.getElementById("raw_distance").innerHTML = parseInt(250 - numDistance);
	}
	else document.getElementById("data_Length").innerHTML = 'Device booting...please wait....';

    document.getElementById("data_lastMaxUpdate").innerHTML = timeConverter(maxUnixTime);
	document.getElementById("data_lastMinUpdate").innerHTML = timeConverter(minUnixTime);

	document.getElementById("data_avgPeople").innerHTML = numAvgPeople;
	document.getElementById("data_avgDistance").innerHTML = numAvgDistance;

	document.getElementById("data_maxPeople").innerHTML = numMaxPeople;
	document.getElementById("data_maxDistance").innerHTML = numMaxDistance;

	document.getElementById("data_minPeople").innerHTML = numMinPeople;
	document.getElementById("data_minDistance").innerHTML = numMinDistance;

	document.getElementById("data_calibration").innerHTML = distanceAway;

	document.getElementById("loadingDiv").style.visibility = 'hidden';
	clearInterval(loadingBarUpdate);
	loadingBarUpdate = window.setInterval(loadingBarCount, 9500);

	clearInterval(intervalUpdate);
	intervalUpdate = window.setInterval(updateCount, 10000);
}

var getJSON = function(url, callback)  // function found on Google
{
	var xhr = new XMLHttpRequest();
	xhr.open('GET', url, true);
	xhr.responseType = 'json';
	xhr.onload = function() {
	  	var status = xhr.status;
	  	if (status === 200) 
	  	{
	    	callback(null, xhr.response);
	  	}
	  	else 
	  	{
	    	callback(status, xhr.response);
	  	}
	};
	xhr.send();
};

function timeConverter(UNIX_timestamp){
  var a = new Date(UNIX_timestamp * 1000);
  var months = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
  var year = a.getFullYear();
  var month = months[a.getMonth()];
  var date = a.getDate();
  var hour = a.getHours();
  var min = a.getMinutes();
  var sec = a.getSeconds();
  var time = date + ' ' + month + ' ' + year + ' ' + hour + ':' + min + ':' + sec ;
  return time;
}
