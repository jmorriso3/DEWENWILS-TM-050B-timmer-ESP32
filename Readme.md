# ESP32 control for the ECO Plugs TM-050B

DBhome V13.132 operated a DEWENWILS TM-050B on firmware 2.3.0 in both directions using this cloud packet format. This extracted Arduino example uses the serial monitor and sends one request per command. The standalone example has not been tested on hardware.

Enable cloud control in the ECO app. Install the Espressif ESP32 Arduino board package, open EcoTimer/EcoTimer.ino, and set your Wi-Fi credentials and timer MAC locally. Upload to an ESP32 on a network that receives the timer broadcasts. Select 115200 baud and New Line. Wait for the reported relay state and send on or off. The example waits for two subsequent timer broadcasts to agree. Relay status alone cannot verify pump flow.

The timer advertises the cloud endpoint and device fields over UDP 10228. The ESP32 sends from port 9090 with routing subtype 0x66, operation 0x6A and desired state 1 or 0. The cloud route requires internet access. This example implements manual operation.

Protocol references: https://github.com/ltsch/ha-eco-plugs-local and https://github.com/homebridge-plugins/homebridge-ecoplugs . Thanks to the upstream authors for documenting the packet format.
