function docReady(fn) {
  // see if DOM is already available
  if (document.readyState === "complete" || document.readyState === "interactive") {
    // call on next available tick
    setTimeout(fn, 1);
  } else {
    document.addEventListener("DOMContentLoaded", fn);
  }
}

function init() {
  document.getElementById("status").innerHTML = "CONNECTING";
  ws = new WebSocket('ws://' + document.location.host + '/ws', ['compass']);

  ws.onopen = ws_onopen;
  ws.onclose = function () { document.getElementById("status").innerHTML = "CLOSED"; };
  ws.onerror = function () { document.getElementById("status").innerHTML = "ERROR"; };
  ws.onmessage = ws_onmessage;
}

function ws_onmessage(e_msg) {
  e_msg = e_msg || window.event;
  angle = Number(e_msg.data);
  showData(angle);
}

function ws_onopen() {
  document.getElementById("status").innerHTML = "<span style=\"color: #322;\">OK</span>";

  setTimeout(function () {
    ws.send("GET");
  }, 50);
}

// function rotate_plate(angle)
// {
//   angle = angle - CENTER_ANGLE;
//   var dial = document.getElementById("dial");
//   var ctx = dial.getContext("2d");

//   ctx.clearRect(-PIVOT_X, -PIVOT_Y, CANVAS_WIDTH, CANVAS_HEIGHT);
//   ctx.rotate(-angle / 180 * Math.PI);

//   ctx.drawImage(plate_img, -PIVOT_X, -PIVOT_Y, CANVAS_WIDTH, CANVAS_HEIGHT);

//   ctx.rotate(angle / 180 * Math.PI);

//   debug = document.getElementById("debug");
//   debug.innerHTML = plate_angle.toFixed(1);
// }

function showData(azimuth) {
  // gamma: Tilting the device from left to right. Tilting the device to the right will result in a positive value.
  // beta: Tilting the device from the front to the back. Tilting the device to the front will result in a positive value.
  // alpha: The direction the compass of the device aims to in degrees.

  // Call the function to use the data on the page.
  document.getElementById('azimuth').innerHTML = Math.ceil(azimuth) + "&deg;";

  // rotate the needle arrow on the compass
  rotate(azimuth);
}

function rotate(azimuth) {
  const arrow = document.getElementById('arrow');

  // Rotate the arrow of the compass. - CSS transform
  arrow.style.transform = `rotate(${azimuth}deg)`;
  arrow.style.webkitTransform = `rotate(${azimuth}deg)`;
  arrow.style.MozTransform = `rotate(${azimuth}deg)`;
}