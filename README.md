# 2DCNN OV2640 — Gesture Recognition with OV2640 Camera

A lightweight real-time gesture recognition system using the OV2640 camera sensor and a 2D convolutional neural network. The project targets STM32F103-class microcontrollers running RT-Thread, with a complete pipeline: **image capture over serial → dataset labeling → model training → ONNX export → pure-C inference on MCU**.

Recognized gestures: **rock**, **paper**, **scissors**, **none** (4 classes).

## Features

- **Serial JPEG stream capture** — `collect_img.py` receives binary JPEG frames from the OV2640 over UART and automatically organizes them into labeled class folders
- **Ultra-lightweight 2D CNN** — 2 convolutional layers with aggressive pooling; input 28×28 grayscale, output 4-class logits
- **ONNX model export** — generates `gesture_vision.onnx` for STM32Cube.AI import
- **Pure-C header export** — all weights and biases exported to `cnn_vision_weights.h` for hand-coded inference
- **RT-Thread application code** — ready-to-use `applications/` module with register-level OV2640 driver, soft I2C SCCB, and DVP data capture
- **Two C inference implementations** — `inference_优化模型.c` (optimized) and `inference_无优化.c` (baseline, currently empty)
- **Camera test tool** — `ov2640_test.py` captures and saves individual JPEG frames for debugging

## Tech Stack

| Layer | Technology |
|---|---|
| Language | Python 3.7+, C (RT-Thread) |
| Deep Learning | PyTorch, torchvision |
| Serial Communication | pySerial |
| Model Export | ONNX (opset 11) |
| MCU Framework | RT-Thread |
| Target Hardware | STM32F103 + OV2640 |

## Directory Structure

```
2DCNN OV2640/
├── train.py                   # Model training + ONNX + C header export
├── train_old.py               # Earlier training script variant
├── collect_img.py             # Serial JPEG capture & auto-labeling
├── ov2640_test.py             # Raw JPEG capture debug tool
├── inference_优化模型.c        # Optimized C inference for STM32
├── inference_无优化.c          # Baseline C inference (placeholder)
├── gesture_vision.onnx        # Exported ONNX model
├── cnn_vision_weights.h       # Auto-generated C weight header
├── applications/              # RT-Thread application code
│   ├── main.c                 # Entry point
│   ├── ov2640.c               # Register-level OV2640 driver
│   ├── ov2640.h               # Driver header & pin definitions
│   ├── soft_i2c.c             # Software I2C (SCCB) implementation
│   ├── soft_i2c.h             # Soft I2C header
│   └── SConscript             # RT-Thread build script
├── dataset/                   # Labeled gesture images
│   ├── rock/                  # Rock gesture samples
│   ├── paper/                 # Paper gesture samples
│   ├── scissors/              # Scissors gesture samples
│   └── none/                  # No-gesture / background samples
├── 实验1：图像数据采集.md
├── 实验2：手势数据标注.md
└── 实验3：手势识别模型训练与部署.md
```

## Installation

```bash
pip install torch torchvision pyserial
```

## Usage

### Step 1 — Test the camera stream

Verify the OV2640 is sending valid JPEG frames:

```bash
python ov2640_test.py   # Edit SERIAL_PORT and BAUD_RATE inside the script
```

Captured frames are saved as `capture_XXXX.jpg` in the current directory.

### Step 2 — Collect gesture data

```bash
python collect_img.py --port COM3 --baud 115200 --gesture rock --count 100 --dir dataset
```

Repeat for each gesture class (`rock`, `paper`, `scissors`, `none`). Images are automatically saved into `dataset/<gesture>/` with timestamped filenames.

### Step 3 — Train the model

```bash
python train.py
```

The script:
1. Loads images from `dataset/`, converts to 28×28 grayscale
2. Splits into 80% train / 20% validation
3. Trains for 70 epochs with Adam optimizer
4. Exports `gesture_vision.onnx` and `cnn_vision_weights.h`

### Step 4 — Deploy to STM32

1. Copy `cnn_vision_weights.h` and `inference_优化模型.c` into your RT-Thread project
2. Integrate the `applications/` module (OV2640 driver + soft I2C)
3. Wire the OV2640 according to the pin mapping in `ov2640.h`:
   - SCCB (Soft I2C): SCL → PB6, SDA → PB7
   - Control: RESET → PB9
   - DVP: VSYNC → PA0, HREF → PB8, PCLK → PB4
   - DVP D0-D7: see `ov2640.h` for the scattered port mapping
4. Build with RT-Thread SCons

## Model Architecture

```
Input: [1, 28, 28] grayscale
  ↓ Conv2d(1→4, k=3, s=2, p=1) → ReLU → MaxPool2d(2,2)
  ↓ Conv2d(4→8, k=3, p=1) → ReLU → MaxPool2d(2,2)
  ↓ Flatten → Linear(72→4)
Output: 4-class logits
```

**Total parameters:** under 500 weights — optimized for MCU flash/RAM constraints.

## API / Script Reference

### `collect_img.py`

| Argument | Type | Default | Description |
|---|---|---|---|
| `--port` | str | `COM3` | Serial port name |
| `--baud` | int | `115200` | Baud rate |
| `--dir` | str | `dataset` | Dataset root directory |
| `--gesture` | str | (required) | Gesture label: `rock`, `paper`, `scissors`, `none` |
| `--count` | int | `100` | Number of images to collect |

## Hardware Notes

- **Register-level DVP capture:** The OV2640 driver reads GPIO input data registers directly (bypassing RT-Thread pin API) to maximize throughput on STM32F103.
- **Interrupt masking:** System scheduling is disabled during frame capture to prevent data loss.
- **Scattered DVP pins:** The 8-bit data bus is mapped across multiple GPIO ports (not a contiguous port) — check `ov2640.h` for exact wiring.
- The soft I2C module handles SCCB communication at ~100 kHz; no hardware I2C peripheral required.

## Notes

- `inference_无优化.c` is currently empty (0 bytes) and serves as a placeholder for a baseline non-optimized inference implementation.
- Dataset images are included in this repository (JPEG, ~2 KB each) — consider using Git LFS if expanding the dataset significantly.
- Ensure no serial monitor or other application occupies the COM port before running capture scripts.

## License

This project is provided for educational and research purposes. Verify your hardware pin mapping against `ov2640.h` before deployment.
