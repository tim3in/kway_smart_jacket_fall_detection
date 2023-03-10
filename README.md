# kway_smart_jacket_fall_detection

This project is used to monitor patient's activity and alerts if fall is detected.

<img src="https://raw.githubusercontent.com/tim3in/kway_smart_jacket_fall_detection/main/cover%20image.jpg" width="500" >

Project description is available at <a href="https://projecthub.arduino.cc/tim3in/f484b3a8-a8ae-4f55-884c-ff70bc44165a">here</a>

It is recommended to use Visual Studio Code and PlatformIO extension to compile and upload <b>kway_smart_jacket_firmware</b>. Following should be the configuration in the <i>platform.ini</i> file.

<img src="https://github.com/tim3in/kway_smart_jacket_fall_detection/blob/main/pio_1.png?raw=true" width="900" >

Upload Edge Impulse model in the <i>lib</i> directory of the project and copy code from <i>kway_smart_jacket_firmware.ino</i> to <i>main.cpp</i>.

<img src="https://github.com/tim3in/kway_smart_jacket_fall_detection/blob/main/pio_2.png?raw=true" width="900" >

The TinyML model for this project is available at Edge Impulse Public Project <a href="https://studio.edgeimpulse.com/public/184891/latest">K-Way Smart Jacket HAR</a>.

The Gateway script <i>kway_smart_jacket_gateway.py</i> should be uploaded and executed on the Raspberry Pi.

<img src="https://github.com/tim3in/kway_smart_jacket_fall_detection/blob/main/gateway.png?raw=true" width="400" >
