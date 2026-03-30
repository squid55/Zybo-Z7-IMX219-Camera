# Zybo Z7-20 IMX219 MIPI Camera

Zybo Z7-20 FPGA에서 IMX219 (Raspberry Pi Camera Module v2)를 MIPI CSI-2로 연결하여 HDMI 출력하는 프로젝트.

기존 [Zybo-Z7-Pcam-MNIST-CNN](https://github.com/squid55/Zybo-Z7-Pcam-MNIST-CNN) (OV5640/Pcam-5C) 프로젝트를 IMX219용으로 수정.

## Hardware

| 항목 | 값 |
|------|-----|
| FPGA Board | Zybo Z7-20 (xc7z020clg400-1) |
| Camera | IMX219 (RPi Camera Module v2) |
| Interface | MIPI CSI-2 (2-lane) |
| Output | HDMI |
| Vivado | 2023.2 |

## OV5640 → IMX219 변경 사항

| 항목 | OV5640 (Pcam-5C) | IMX219 (RPi Cam v2) |
|------|-----------------|-------------------|
| I2C Address | 0x3C | 0x10 |
| Chip ID | 0x5640 | 0x0219 |
| Output Format | RAW10/YUV | RAW10 |
| AWB | Software AWB | N/A (RAW only) |
| Driver | OV5640.h | IMX219.h |

## Project Structure

```
sw/src/
├── main.cc          # IMX219 카메라 초기화 + HDMI 출력
├── imx219/
│   ├── IMX219.h     # IMX219 드라이버 (I2C 레지스터 설정)
│   ├── IMX219.cpp
│   ├── AXI_VDMA.h   # VDMA 드라이버
│   ├── PS_GPIO.h    # GPIO 드라이버
│   ├── PS_IIC.h     # I2C 드라이버
│   └── ...
├── platform/        # Zynq 플랫폼
└── hdmi/            # HDMI 출력
```

## Block Design

![Block Design](docs/block_design_pcam.pdf)

Vivado Block Design은 기존 Pcam-5C 프로젝트의 MIPI D-PHY + CSI-2 RX 파이프라인을 재사용.

## Build

1. Vivado 2023.2에서 블록 디자인 열기
2. Generate Bitstream
3. Export Hardware (.xsa)
4. Vitis에서 `sw/src/` 소스로 애플리케이션 빌드
5. JTAG로 Zybo에 다운로드

## Related

- [Zybo-Z7-Pcam-MNIST-CNN](https://github.com/squid55/Zybo-Z7-Pcam-MNIST-CNN) — 원본 OV5640 프로젝트
- [Jetson-AI-Camera](https://github.com/squid55/Jetson-AI-Camera) — Jetson Orin Nano YOLOv8
- [Multi-Board-Viewer](https://github.com/squid55/Multi-Board-Viewer) — 멀티 보드 통합 뷰어
