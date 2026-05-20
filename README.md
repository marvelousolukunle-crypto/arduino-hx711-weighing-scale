# ⚖️ Arduino Digital Weighing Scale

A professional-grade digital weighing scale built with an Arduino Uno, HX711 load cell amplifier, and a 16×2 I2C LCD. Features adaptive averaging, stability detection via a circular buffer, automatic unit switching (g/kg), and noise filtering — all implemented in clean, documented firmware.

> Custom firmware designed and developed as a technical consultant for an MCE Group 2 embedded systems project.

---

## 📸 image

![Front View](images/Front view.png)
![Side View](images/side view.png)

---

## ✨ Features

- ✅ **Adaptive sampling** — fast reads while weight is changing, high-accuracy averaging when stable
- ✅ **Stability detection** — circular buffer tracks reading history to confirm a settled measurement
- ✅ **Auto unit switching** — displays in grams (g) below 1 kg, kilograms (kg) above
- ✅ **Zero threshold filter** — eliminates environmental noise near empty
- ✅ **Sensor fault detection** — detects unresponsive HX711 on startup with user-facing error message
- ✅ **Flicker-free display** — LCD only updates when the value meaningfully changes
- ✅ **I2C LCD** — reduces wiring from 6 pins to 2 (SDA/SCL)
- ✅ **Serial telemetry** — real-time weight and stability status over UART for debugging

---

## 🧰 Hardware Requirements

| Component | Specification |
|---|---|
| Microcontroller | Arduino Uno (ATmega328P) |
| Load Cell Amplifier | HX711 (24-bit ADC) |
| Load Cell | Any single-point or beam load cell (rated capacity as needed) |
| Display | 16×2 LCD with I2C backpack (PCF8574) |
| Power Supply | USB or 5V regulated |

---

## 🔌 Wiring Diagram

### HX711 → Arduino

| HX711 Pin | Arduino Pin |
|---|---|
| VCC | 5V |
| GND | GND |
| DT (Data) | D3 |
| SCK (Clock) | D2 |

### Load Cell → HX711

| Load Cell Wire | HX711 Terminal |
|---|---|
| RED | E+ (Excitation +) |
| BLACK | E− (Excitation −) |
| WHITE | A− (Signal −) |
| GREEN | A+ (Signal +) |

### LCD (I2C) → Arduino

| LCD Pin | Arduino Pin |
|---|---|
| VCC | 5V |
| GND | GND |
| SDA | A4 |
| SCL | A5 |

> **Note:** I2C address is `0x27` by default. If the display stays blank, try `0x3F` in `config` section of the code.

---

## 📚 Libraries Required

Install via Arduino IDE → **Sketch → Include Library → Manage Libraries**

| Library | Author | Purpose |
|---|---|---|
| `HX711` | bogde | Load cell ADC communication |
| `LiquidCrystal_I2C` | Frank de Brabander | I2C LCD control |

---

## ⚙️ Calibration Procedure

Calibration is required once per physical setup. If you change the load cell or mounting, recalibrate.

**Step 1** — Set `SCALE_FACTOR` to `1.0` in the code and upload.

**Step 2** — Open Serial Monitor at **9600 baud**.

**Step 3** — Leave the scale empty. Note the raw reading (should be near 0).

**Step 4** — Place a known reference weight (e.g., 500g).

**Step 5** — Note the raw number shown in Serial Monitor.

**Step 6** — Calculate:
```
SCALE_FACTOR = raw_reading / known_weight_in_grams
```

**Step 7** — Update `SCALE_FACTOR` in the code and re-upload. Verify accuracy with your reference weight.

---

## 🧠 Firmware Architecture

```
loop()
  │
  ├── 1. Fast read (HX711)          — responsive while weight is changing
  ├── 2. Circular buffer update     — stores last N readings for stability check
  ├── 3. Stability detection        — range check across history window
  ├── 4. Adaptive final read        — high-accuracy average if stable, fast if not
  ├── 5. Zero threshold filter      — suppresses noise below ZERO_THRESHOLD
  └── 6. Display update (if changed)— flicker-free LCD + Serial telemetry
```

### Key Design Decisions

| Decision | Reason |
|---|---|
| Circular buffer for stability | O(1) memory, no array shifting, standard embedded pattern |
| Adaptive reads (3 vs 10) | Balances responsiveness with accuracy depending on system state |
| `dtostrf()` for float formatting | `sprintf()` doesn't support `%f` on AVR (stripped from stdlib) |
| I2C LCD (2 wires) | Saves GPIO pins; uses PCF8574 I/O expander on the backpack |
| Update display only on change | Prevents LCD flicker and reduces unnecessary I2C bus traffic |

---

## 📁 Project Structure

```
WeighingScale/
└── WeighingScale.ino    ← Full firmware (single-file, beginner-friendly)
```

> **Planned refactor:** Multi-file structure with `config.h`, `scale_sensor.cpp`, `display.cpp`, and a formal state machine. See [Roadmap](#-roadmap) below.

---

## 🗺️ Roadmap

- [ ] Move configuration to `config.h` header file
- [ ] Refactor `loop()` into single-responsibility sub-functions
- [ ] Replace `delay()` with `millis()`-based non-blocking timing
- [ ] Implement a formal state machine (`INIT → TARING → MEASURING → STABLE → ERROR`)
- [ ] Add physical tare button (interrupt-driven)
- [ ] Structured CSV serial output for host-side logging
- [ ] Multi-file project structure following embedded industry conventions

---

## 🐛 Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| LCD stays blank | Wrong I2C address | Change `LCD_ADDRESS` to `0x3F` |
| "SENSOR ERROR!" on startup | HX711 wiring issue | Check DT/SCK connections and 5V supply |
| Reading drifts constantly | Poor mechanical mounting | Ensure load cell is rigidly fixed |
| Always reads 0 | `SCALE_FACTOR` not set | Follow calibration procedure above |
| Reading never shows "Stable" | `STABLE_MARGIN` too tight | Increase `STABLE_MARGIN` value slightly |

---

## 👨‍💻 Author

**MCE Group 2**
Embedded Systems Project

---

## 📄 License

This project is open source under the [MIT License](LICENSE).
