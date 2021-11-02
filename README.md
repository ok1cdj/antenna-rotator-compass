# Antenna Rotator Compass

#### By Peter "**Tekk**" [OM7TEK](https://www.om7tek.com)

My friend OM7AMO created a custom manual rotator and needed a remote compass for it, just to be sure on what azimuth the 2xHB9CV is pointing at in night.

## ESP32 + QMC5883L (HMC5883)

This project aims to transfer current antenna orientation *Azimuth* with manual rotators.
Data is transferred over **ultra-low-latency** Websocket protocol and it has very appealing and responsive frontend app design that runs in the browser on a mobile phone or a computer from the ESP32.

## Project blogpost
[Here](https://www.om7tek.com/2021/open-source-remote-compass-for-your-antenna-rotator/) you can find detailed information about this project

## Usage
- ESP32 creates WIFI Soft AP `COMPASS`, no password required
- User connects to the WIFI AP and enters URL [http://compass.local](http://compass.local) in the browser
  - **NOTE! If that doesn't work, enter directly [http://192.168.4.1](http://192.168.4.1)** which is the default static IP of the ESP32 when you connect to the Soft AP
- Low latency Websocket link is established using `ESPAsyncWebServer` and the compass is displayed on frontend app
- You can see connection status on the web as well

## Future work
- Add motor control for automatically rotating the antenna
  - Project is already prepared for this, you just need to uncomment some of the code
  
## Contributions
Are very welcome. For bugs or suggestions and enhancements open a Github Issue on the repo.

## Credits
- Eva, my wife ❤️
- Michal OM7AMO

