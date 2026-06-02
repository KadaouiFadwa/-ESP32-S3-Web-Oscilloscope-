# ESP32-S3 Web Oscilloscope + Function Generator

[![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)](https://www.espressif.com/en/products/socs/esp32-s3)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Web UI](https://img.shields.io/badge/Web%20UI-Responsive-orange)]()

An all-in-one web-controlled **oscilloscope** and **function generator** built around the ESP32-S3 microcontroller. Features a beautiful dark-themed web interface accessible from any device on your local network.

## 📸 Features

### 🎯 Oscilloscope
- **Live waveform display** - Real-time signal visualization
- **Start/Stop control** - Freeze waveform for detailed analysis
- **Automatic measurements**:
  - Frequency (Hz/kHz)
  - Peak-to-Peak Voltage (Vpp)
  - RMS Voltage
- **10 kSPS sampling rate** with 256 samples per frame
- **12-bit ADC resolution** (0-3.3V input range)

### 🎵 Function Generator
- **4 waveform types**:
  - 🔵 Sine Wave - Smooth, pure tones
  - 🟨 Square Wave - Sharp, buzzy signals
  - 🔺 Triangle Wave - Soft but punchy
  - ⚡ Sawtooth Wave - Aggressive, rising edges
- **Frequency range**: 10 Hz - 10 kHz
- **Amplitude control**: 0-100% (0-3.3V output via DAC)
- **MCP4725 DAC** for high-quality analog output

### 🎛️ Hardware Controls
- **LED control** - On/Off with visual feedback
- **Musical Beeper** - Generates tones matching selected waveform & frequency
- **Status indicators** - Real-time system status

### 🌐 Web Interface
- **Responsive design** - Works on desktop, tablet, and mobile
- **Dark theme** with neon accents
- **Real-time updates** - No page refresh needed
- **Intuitive controls** with waveform preview icons

## 🔧 Hardware Requirements

### Components
| Component | Specification | Pin Connection |
|-----------|--------------|----------------|
| ESP32-S3 DevKit | Any ESP32-S3 board | - |
| MCP4725 DAC Module | I2C interface | SDA→GPIO8, SCL→GPIO9 |
| LED | Any color + 220Ω resistor | GPIO2 |
| Passive Buzzer | 3-5V piezo element | GPIO4 |
| ADC Input | Signal under test | GPIO1 (ADC1_CH0) |

### Optional
- Breadboard and jumper wires
- Oscilloscope probe or BNC connector
- Potentiometer for testing

### Circuit Diagram









ESP32-S3 MCP4725
┌─────────┐ ┌─────────┐
│ 3.3V├─────────┤VCC │
│ GND├─────────┤GND │
│ GPIO8 ├─────────┤SDA │
│ GPIO9 ├─────────┤SCL │
│ │ │OUT ├───► To DUT
└─────────┘ └─────────┘

LED: GPIO2 ──┬── 220Ω ──► LED ──► GND
Buzzer: GPIO4 ──────────────► Buzzer (+) ──► GND
Input: GPIO1 ◄────────────── Signal Input
text


## 🚀 Getting Started

### 1. Install Required Libraries

Open Arduino IDE and install these libraries:
- **WiFi** (built-in with ESP32)
- **WebServer** (built-in)
- **Wire** (built-in)
- **math.h** (built-in)

### 2. Configure WiFi

Edit these lines in the code:
```cpp
const char* WIFI_SSID     = "Your_WiFi_Name";     // Change this
const char* WIFI_PASSWORD = "Your_Password";      // Change this

3. Upload to ESP32-S3

    Select board: ESP32S3 Dev Module

    Select correct COM port

    Click Upload

4. Connect to Device

After upload, check Serial Monitor (115200 baud) for:
text

✅ Connected! IP: 192.168.1.xxx

Open a web browser and navigate to: http://[ESP32_IP_ADDRESS]
🎮 How to Use
Oscilloscope

    Connect your test signal to GPIO1

    Click "START OSCILLOSCOPE"

    View live waveform on canvas

    Automatic frequency, Vpp, and RMS readings appear

    Click "STOP" to freeze the waveform

Function Generator

    Select waveform type (Sine/Square/Triangle/Sawtooth)

    Adjust Frequency slider (10Hz - 10kHz)

    Adjust Amplitude slider (0-100%)

    Click "START GENERATOR" to output signal

    Connect output from MCP4725 to your circuit

Musical Beep

    Click "🎵 MUSICAL BEEP" button

    Beep automatically uses the currently selected waveform and frequency

    Different waveforms produce different sound characteristics:

        Sine: Smooth, pleasant tone

        Square: Sharp, buzzy sound

        Triangle: Soft but punchy

        Sawtooth: Aggressive, rising sweep

LED Control

    Click "LED ON/OFF" to control the connected LED

    Visual feedback shows LED state

📊 Technical Specifications
ADC (Oscilloscope)
Parameter	Value
Resolution	12-bit (0-4095)
Voltage range	0-3.3V (ADC_11db attenuation)
Sampling rate	10,000 samples/second
Buffer size	256 samples
Frequency measurement	Zero-crossing detection
DAC (Function Generator)
Parameter	Value
Resolution	12-bit (MCP4725)
Output range	0-3.3V
Update rate	100 kHz
Waveforms	Sine, Square, Triangle, Sawtooth
Web Interface
Parameter	Value
Update rate	5 fps (when oscilloscope running)
Data format	JSON over HTTP
Canvas size	Responsive (auto-scaling)
🎨 Web Interface Preview

The interface features:

    RTL (Right-to-Left) text support for Arabic

    Neon glow effects on active controls

    Status indicators with pulsing dots

    Waveform preview icons for each signal type

    Toast notifications for user actions

    Responsive grid layout

🔍 Troubleshooting
WiFi Connection Fails

    The ESP32 will automatically start Access Point mode with SSID: ESP32-Oscilloscope, Password: 12345678

    Connect to this AP and navigate to 192.168.4.1

No Waveform Displayed

    Check input signal is connected to GPIO1

    Verify signal voltage is within 0-3.3V range

    Click "START OSCILLOSCOPE" button

No Output from Function Generator

    Verify MCP4725 is properly connected (I2C)

    Check power supply to DAC module

    Click "START GENERATOR" button

Buzzer Not Working

    Passive buzzer requires PWM signal (works with analogWrite or tone)

    Check connection polarity (if applicable)

    Adjust beep frequency (some buzzers have resonance ranges)

📝 Code Structure
text

ESP32_Oscilloscope.ino
├── WiFi Configuration
├── Pin Definitions
├── MCP4725 Driver
├── Generator Task (RTOS)
├── ADC Sampling
├── Measurements (Freq, Vpp, RMS)
├── Musical Beep Generator
├── Web Server
│   ├── HTML/CSS/JS (embedded)
│   └── HTTP Endpoints
└── Main Loop

API Endpoints
Endpoint	Method	Description
/	GET	Web interface
/osc/start	GET	Start oscilloscope
/osc/stop	GET	Stop oscilloscope
/osc/data	GET	Get sample data + measurements
/gen/start	GET	Start function generator
/gen/stop	GET	Stop function generator
/gen/wave?value=X	GET	Set waveform (0-3)
/gen/freq?value=X	GET	Set frequency (10-10000 Hz)
/gen/amp?value=X	GET	Set amplitude (0-100%)
/led/on	GET	Turn LED on
/led/off	GET	Turn LED off
/buzzer/beep	GET	Play musical beep
🛠️ Customization
Changing Sample Rate

Modify SAMPLE_RATE definition:
cpp

#define SAMPLE_RATE 10000  // Hz - Increase for faster sampling

Adjusting Beep Duration

Modify BEEP_DURATION_MS:
cpp

#define BEEP_DURATION_MS 250  // milliseconds

Adding More Waveforms

Extend the switch statement in generatorTask():
cpp

case 4: // Custom waveform
  val = /* your waveform function */;
  break;

Modifying Web Interface Colors

Edit CSS variables in the :root section of HTML_PAGE:
css

--accent:   #00d4ff;  /* Primary blue */
--accent2:  #00ff88;  /* Secondary green */
--accent3:  #ff6b35;  /* Tertiary orange */

📈 Future Enhancements

    Trigger modes (rising/falling edge)

    Save/Load waveforms

    External trigger input

    Dual-channel oscilloscope

    Arbitrary waveform generator

    WiFi spectrum analyzer

    Data logging to SD card

    Bluetooth control option

⚠️ Notes & Warnings

    Input voltage on GPIO1 should not exceed 3.3V (use voltage divider for higher voltages)

    MCP4725 requires 3.3V or 5V power (check your module's specification)

    Passive buzzer works best with square waves (but code handles all waveforms)

    Sampling rate is limited by ADC conversion time and web transmission

    Frequency measurement accuracy decreases below ~50 Hz and above ~5 kHz

📚 Resources

    ESP32-S3 Datasheet

    MCP4725 Datasheet

    Arduino ESP32 Core
