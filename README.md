@@ -0,0 +1,7 @@
* **Activate BLE**
  * pio run -t menuconfig 
    *  Component config → Bluetooth (space *)
    *  Bluetooth host → NimBLE (space *)

* **Update plattformio.ini**
  * To match your MCU.. (H2 or S3)