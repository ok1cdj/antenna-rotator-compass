# Antenna Rotator Compass

## ESP32 + QMC5883L (HMC5883)

This project aims to transfer current antenna orientation *Azimuth* with manual rotators.
Data is transferred over **ultra-low-latency** Websocket protocol and it has very appealing and responsive frontend app design that runs in the browser on a mobile phone or a computer from the ESP32.

#### Usage
- ESP32 creates WIFI Soft AP `COMPASS`, no password required
- User connects to the WIFI AP and enters URL [http://compass.local](http://compass.local) in the browser
- Low latency Websocket link is established using `ESPAsyncWebServer` and the compass is displayed on frontend app
- You can see connection status on the web as well

### Improvements
- Add motor control for automatically rotating the antenna
  - Project is already prepared for this, you just need to uncomment some of the code
  
### Contributions
Are very welcome. For bugs or suggestions use Github Issues on the repo.

## Credits
- Michal OM7AMO

