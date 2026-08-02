# 🌡️ ESP32-C6 Indoor Weather Station

An open-source indoor environmental monitoring system built on a custom **ESP32-C6-WROOM-1** dev board. It measures temperature, relative humidity, barometric pressure, and air quality (IAQ) using the **Bosch BME680**, displaying real-time metrics on a color TFT screen with dynamic, flicker-free backlight dimming.

Designed using **KiCad** for hardware development and programmed via **PlatformIO**.

---

## ✨ Features

* **Environmental Sensing:** High-accuracy indoor temperature, humidity, barometric pressure, and Gas/IAQ metrics via Bosch BME680.
* **Dynamic Backlight Control:** Ambient light-sensing using a photoresistor (LDR) voltage divider coupled with high-frequency PWM.
* **IEEE 1789 Compliant Dimming:** Backlight PWM frequency tuned to prevent visible flicker and health risks associated with LED dimming, as outlined in the IEEE 1789-2015 standard.
* **Custom Hardware:** Tailored PCB designed from scratch around the ESP32-C6 module.
* **TFT Display UI:** Real-time data visualization on a vibrant color LCD screen.
* **3D-Printed Enclosure:** Protective cover for the circuit designed in **FreeCAD**, sized to fit the custom PCB and TFT cutout.

---

## 🛠️ Hardware Overview

| Component | Details |
|---|---|
| **Microcontroller** | ESP32-C6-WROOM-1 (RISC-V 32-bit single-core, Wi-Fi 6, Bluetooth 5, 802.15.4 / Thread / Zigbee) |
| **Environmental Sensor** | Bosch BME680 (I2C) |
| **Display** | ST7735 TFT Display (SPI) |
| **Light Sensing** | Photoresistor (LDR) in a voltage divider network connected to an ESP32-C6 ADC pin |
| **Backlight Control** | TFT backlight pin driven directly via the ESP32-C6 **LEDC (LED Control)** peripheral to generate high-frequency PWM for IEEE 1789-compliant dimming |

---

## 💻 Tech Stack & Libraries

* **EDA / Hardware:** [KiCad](https://www.kicad.org/) (Schematics & PCB Layout)
* **3D CAD / Enclosure:** [FreeCAD](https://www.freecad.org/) (Circuit cover / enclosure design)
* **Firmware Framework:** [PlatformIO](https://platformio.org/) on Arduino Core
* **Core Libraries:**
  * `Adafruit_ST7735` & `Adafruit_GFX` – Display driver and graphics library
  * `Adafruit_BME680` & `Adafruit_Sensor` – Environmental sensor driver
  * `Wire.h` & `SPI.h` – Hardware bus communication

---

## 📋 Project Management

This project is managed using [Plane](https://app.plane.so) — an open-source Jira/Confluence alternative — for issue tracking, sprints, and documentation. Feature requests, bugs, and task boards live there rather than solely in GitHub Issues.

---

## 🗺️ Roadmap / Planned Features

* **Zephyr RTOS Port:** Migrate firmware from Arduino Core to Zephyr RTOS for improved power management, native threading, and better long-term driver support on the ESP32-C6.
* **MQTT Broker Integration:** Publish sensor readings (temperature, humidity, pressure, IAQ) to an MQTT broker, with support for **Home Assistant** MQTT auto-discovery.
* **OTA Firmware Updates:** Over-the-air update support to avoid re-flashing over USB for field-deployed units.
* **Web Configuration Portal:** Captive-portal Wi-Fi provisioning and settings UI served directly from the ESP32-C6.
* **Historical Data Logging:** Local logging (SD card or flash) with optional export/graphing.

> Have a feature request? Open an [issue](../../issues) or start a [discussion](../../discussions).

---

## 🚀 Getting Started

### 1. Hardware Design

Schematics, PCB layout files, and BOM (Bill of Materials) can be found in the [`/hardware`](./hardware) directory. Open them with **KiCad 10.0+**.

### 2. Firmware Compilation

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the [PlatformIO extension](https://platformio.org/platformio-ide).
2. Clone this repository:
   ```bash
   git clone https://github.com/your-username/your-repo-name.git
   cd your-repo-name
   ```
3. Open the project folder in VS Code — PlatformIO should detect the `platformio.ini` automatically and install the required libraries and toolchain.
4. Connect the ESP32-C6-WROOM-1 board via USB.
5. Build and upload:
   ```bash
   pio run --target upload
   ```
6. Monitor serial output:
   ```bash
   pio device monitor
   ```

---

## 📁 Repository Structure

```
.
├── hardware/        # KiCad schematics, PCB layout, and BOM
├── enclosure/        # FreeCAD source files and exported STL/STEP for the cover
├── src/              # Firmware source code
├── include/          # Header files
├── lib/               # Project-specific libraries
├── platformio.ini    # PlatformIO project configuration
└── README.md
```

---

## 🤝 Contributing

Contributions, issues, and feature requests are welcome. Feel free to check the [issues page](../../issues) or submit a pull request.

---

## 📄 License

This project is licensed under the terms of the license included in this repository — see [`LICENSE`](./LICENSE) for details.
