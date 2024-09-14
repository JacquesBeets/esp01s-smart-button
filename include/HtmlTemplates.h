#pragma once
#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>SMART BUTTON</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {font-size: 1.2rem;}
    body {margin: 0; background-color: #2f4468; color: white;}
    .topnav {overflow: hidden; background-color: #2f4468; color: white; font-size: 1.7rem;}
    .content {padding: 20px; text-align: center;}
    .card {background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);}
    .cards {max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));}
    .reading {font-size: 2.8rem;}
    .packet {color: #bebebe;}
    .card.temperature {color: #fd7e14;}
    .card.humidity {color: #1b78e2;}
    #serial {height: 500px; overflow: scroll; background-color: #2f4468; color: white; font-size: 1.2rem; padding: 10px;}
  </style>
</head>
<body>
  <div class="topnav"><h3>SMART BUTTON</h3></div>
  <div class="content">
    <button id="button1">Button 1</button>
    <button id="button2">Button 2</button>
  </div>
  <div class="content">
    <button id="descover">HomeAssitant Descover</button>
    <button id="un-descover">HomeAssitant UnDescover</button>
  </div>
  <div id="serial"></div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected", e);
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);

 source.addEventListener('serial', function(e) {
  console.log("serial message: ", e.data);
  document.getElementById("serial").innerHTML += '<br/>' + e.data;
 }, false);
}

function sendMessage(message) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log("Message sent");
    }
  };
  xhttp.open("POST", "/toggle", true);
  xhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
  xhttp.send("message=" + message); 
}

function sendDescover(type) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log("Descover sent");
    }
  };
  xhttp.open("POST", "/" + type, true);
  xhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
  xhttp.send(); 
}

window.onload = function() {
  document.getElementById("button1").addEventListener("click", function() {
    sendMessage("button1");
  });   
  document.getElementById("button2").addEventListener("click", function() {
    sendMessage("button2");
  });
  document.getElementById("descover").addEventListener("click", function() {
    sendDescover("descover");
  });
  document.getElementById("un-descover").addEventListener("click", function() {
    // Confirm dialog
    if (confirm("Are you sure you want to un-descover the device?")) {
      sendDescover("un-descover");
    }
  });
}
</script>
</body>
</html>
)rawliteral";

const char upload_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP8266 File Upload</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 0 auto; padding: 20px; max-width: 800px; }
        h1 { color: #333; }
        form { margin-bottom: 20px; }
        input[type="file"] { margin-bottom: 10px; }
        button { background-color: #4CAF50; border: none; color: white; padding: 10px 20px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; }
        #fileList { border: 1px solid #ddd; padding: 10px; }
    </style>
</head>
<body>
    <h1>ESP8266 File Upload</h1>
    <form id="upload_form" enctype="multipart/form-data">
        <input type="file" name="file" id="file"><br>
        <button type="submit">Upload File</button>
    </form>
    <div id="fileList"></div>

    <script>
        document.getElementById('upload_form').onsubmit = function(e) {
            e.preventDefault();
            var form = new FormData();
            var file = document.getElementById('file').files[0];
            form.append('file', file, file.name);

            fetch('/upload', {
                method: 'POST',
                body: form
            })
            .then(response => response.text())
            .then(result => {
                alert(result);
                loadFileList();
            })
            .catch(error => console.error('Error:', error));
        };

        function loadFileList() {
            fetch('/list')
            .then(response => response.json())
            .then(files => {
                var fileList = document.getElementById('fileList');
                fileList.innerHTML = '<h2>File List:</h2>';
                files.forEach(file => {
                    fileList.innerHTML += `<p>${file.name} (${file.type})</p>`;
                });
            })
            .catch(error => console.error('Error:', error));
        }

        loadFileList();
    </script>
</body>
</html>
)rawliteral";