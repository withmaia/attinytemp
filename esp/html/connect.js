function $(s) { return document.querySelector(s); };
var status_text = $('.status');
document.forms['connect'].style.display = 'none';

// Check connection status
var check_connection_interval = null;

function checkConnection() {
    var fetch_connection = fetch('/connection.json').then(function(resp) {
        return resp.json();
    });
    fetch_connection.then(handleConnection);
};

// Interpret and react to connection status
function handleConnection(connection) {
    if (connection.status == 'connected') {
        window.location = '/register';
    }

    else if (connection.status == 'connecting') {
    }

    else if (connection.status == 'failed') {
        document.forms['connect'].style.display = 'block';
        $('form[name=connect] button').textContent = 'Try again';
        $('form[name=connect] button').disabled = false;
        if (connection.error) {
            status_text.innerHTML = '<strong>Failed to connect</strong>: ' + connection.error
        } else {
            status_text.innerHTML = '<strong>Failed to connect</strong>'
        }
        status_text.className = 'status error'
    }

    else {
        document.forms['connect'].style.display = 'block';
        loadScan();
        $('#ssid').focus();
        status_text.textContent = '';
    }
}

// Load available Wifi networks from scan
function loadScan() {
    fetch('/scan.json').then(function (r) { return r.json() }).then(function (stations) {
        stations.sort(function(a, b) { return (a.rssi > b.rssi) ? -1 : 1; });
        stations.map(function (station) {
            var option = document.createElement('option');
            option.value = station.ssid;
            option.textContent = station.ssid;
            $('#ssid').appendChild(option);
        });
    });
}

checkConnection(); // Start by checking connection

// Submit connect form -> post connection info, start checking connection status
document.forms['connect'].onsubmit = function() {
    $('form[name=connect] button').textContent = 'Connecting...';
    $('form[name=connect] button').disabled = true;
    status_text.textContent = '';
    status_text.className = 'status';

    fetch('/connect.json', {
        method: 'post',
        body: JSON.stringify({
            ssid: $('#ssid').value,
            pass: $('#pass').value
        })
    });

    check_connection_interval = setInterval(checkConnection, 2500);
    return false;
};

