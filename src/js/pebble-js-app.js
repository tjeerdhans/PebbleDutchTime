//  file: pebble-js-app.js
//  auth: Matthew Clark, SetPebble, modifications by tjeerdhans

// change this token for your project
//var setPebbleToken = 'VJY7'; //v1.0
var setPebbleToken = 'ZGRY'; // v1.1

function getAndApplySettings() {
    var settings = localStorage.getItem(setPebbleToken);
    console.log(settings);
    if (typeof (settings) == 'string') {
        try {
            Pebble.sendAppMessage(JSON.parse(settings));
        } catch (ex) {
            console.log(ex.message);
        }
    }
    var request = new XMLHttpRequest();
    request.open('GET', 'http://x.SetPebble.com/api/' + setPebbleToken + '/' + Pebble.getAccountToken(), true);
    request.onload = function (e) {
        if (request.readyState == 4)
            if (request.status == 200)
                try {
                    var settings = JSON.parse(request.responseText);
                    localStorage.setItem(setPebbleToken, request.responseText);
                    Pebble.sendAppMessage(settings);
                } catch (ex) {
                    console.log(ex.message);
                }
    };
    request.send(null);
}

Pebble.addEventListener('ready', function (e) {
    getAndApplySettings();
});
Pebble.addEventListener('appmessage', function (e) {
    var key = e.payload.action;
    if (typeof (key) != 'undefined') {
        getAndApplySettings();
    }
});
Pebble.addEventListener('showConfiguration', function (e) {
    Pebble.openURL('http://x.SetPebble.com/' + setPebbleToken + '/' + Pebble.getAccountToken());
});
Pebble.addEventListener('webviewclosed', function (e) {
    if ((typeof (e.response) == 'string') && (e.response.length > 0)) {
        try {
            Pebble.sendAppMessage(JSON.parse(e.response));
            localStorage.setItem(setPebbleToken, e.response);
        } catch (ex) {
            console.log(ex.message);
        }
    }
});
