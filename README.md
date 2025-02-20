# Nordic Firmware Development
#### Client Name: Stephen Long

## Project Requirements
The system consists of a Host Board (nRF52840 and nRF9160) and Client Board (nRF52840, DHT11 sensor). The client and host boards will build a BLE mesh network and the client boards will send the humidity, temperature, and voltage sensor to the host device over the mesh network. The nRF9160 of the host board will receive the sensor values over UART and transfer the GPS value and sensor data to the AWS server over LTE or NB-IoT.
