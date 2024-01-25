// Complete project details: https://randomnerdtutorials.com/esp32-plot-readings-charts-multiple/

// Get current sensor readings when the page loads
window.addEventListener('load', getReadings);

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

// Create Temperature Chart
var chartT = new Highcharts.Chart({
    chart: {
        renderTo: 'chart-temperature'
    },
    series: [
        {
            name: 'Temperature',
            type: 'line',
            color: '#101D42',
            marker: {
                symbol: 'circle',
                radius: 3,
                fillColor: '#00A6A6',
            }
        }
    ],
    title: {
        text: undefined
    },
    xAxis: {
        type: 'datetime',
        dateTimeLabelFormats: { second: '%H:%M' }
    },
    yAxis: {
        title: {
            text: 'Temperature Celsius Degrees'
        }
    },
    credits: {
        enabled: false
    }
});

// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

function getReadings() {
    websocket.send("getReadings");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}


//Plot temperature in the temperature chart
function plotTemperature(jsonValue) {

    var keys = Object.keys(jsonValue);
    console.log(keys);
    console.log(keys.length);

    for (var i = 0; i < keys.length; i++) {
        var x = (new Date()).getTime();
        console.log(x);
        const key = keys[i];
        var y = Number(jsonValue[key]);
        console.log(y);

        if (chartT.series[i].data.length > 40) {
            chartT.series[i].addPoint([x, y], true, true, true);
        } else {
            chartT.series[i].addPoint([x, y], true, false, true);
        }

    }
}

// When websocket is established, call the getReadings() function
function onOpen(event) {
    console.log('Connection opened');
    getReadings();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

// Function that receives the message from the ESP32 with the readings
function onMessage(event) {
    var data = event.data;
    console.log(data);
    var dataObj = JSON.parse(data);
    console.log(dataObj);
    plotTemperature(dataObj);
}


document.getElementById('downloadButton').addEventListener('click', function () {
    // Create a hidden iframe
    var iframe = document.createElement('iframe');
    iframe.style.display = 'none';
    document.body.appendChild(iframe);

    // Set the iframe source to trigger the download
    iframe.src = '/download';
});