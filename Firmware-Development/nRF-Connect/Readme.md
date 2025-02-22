# Flashing Guide
1. Download the Software from [Segger Jlink](https://www.segger.com/downloads/jlink/)
2. Open J-Link Commander.
3. Type "connect" and hit enter.
4. If desired MCU name is already written, then press enter. Otherwise type `?` and hit Enter. Then select the MCU `nRF52840/ nRF9160`.
5. Type `s` for SWD and hit enter.
6. Select the default by pressing Enter.
7. Go to binary file. Hold Shift key + Right Click. Select Copy as Path.
8. Enter this command in jlink window `loadfile "Paste the path here" 0x0`. Hit Enter.
9. Repeat the process for all three MCU's.
10. Restart all the boards.

# Testing 
1. Open  J-Link RTT Viewer.
2. Select Ok.
3. Select the j-link the MCU is connected with.
4. Select the device `nRF9160_xxAA`.
5. Now start configuring the device with `nRF Mesh` mobile app.
