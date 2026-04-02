* **Activate BLE**
  * pio run -t menuconfig 
    *  Component config → Bluetooth (space *)
    *  Bluetooth host → NimBLE (space *)

* **NOTE: If using ESP32-H2**
  * You need to update the plattformio.ini to match your hardware (this is **not needed on S3** as it's already selected)