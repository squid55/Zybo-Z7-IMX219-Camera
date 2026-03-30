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

## Current Status

- Vitis 빌드 성공, 프로그램 실행 성공 (ScuGic, GPIO, IIC, VDMA, VTC 초기화 OK)
- HDMI 출력 동작 (신호 있음)
- **카메라 영상 노이즈** — Digilent MIPI D-PHY IP가 OV5640 전용으로 IMX219와 호환 안 됨
- **해결 방안**: Xilinx 공식 MIPI CSI-2 Rx Subsystem IP로 블록 디자인 재설계 필요

## Reference

- [Xilinx MIPI CSI-2 Rx Subsystem IP Documentation](https://docs.amd.com/r/en-US/pg232-mipi-csi2-rx) — Xilinx 공식 MIPI CSI-2 수신 IP
- [Numato Lab: IMX219 on Zynq 7000](https://numato.com/blog/live-imx219-video-pipeline-implementation-on-tityracore-zynq-7000-soc/) — Zynq-7020에서 IMX219 구현 사례 (Xilinx MIPI CSI-2 Rx Subsystem 사용)
- [gtaylormb: IMX219 to DisplayPort (Ultra96-V2)](https://github.com/gtaylormb/ultra96v2_imx219_to_displayport) — IMX219 + Xilinx MIPI IP 오픈소스 프로젝트
- [circuitvalley: IMX219 MIPI CSI Receiver FPGA](https://github.com/circuitvalley/mipi_csi_receiver_FPGA) — IMX219 레지스터 설정 참고

## Related

- [Zybo-Z7-Pcam-MNIST-CNN](https://github.com/squid55/Zybo-Z7-Pcam-MNIST-CNN) — 원본 OV5640 프로젝트
- [Jetson-AI-Camera](https://github.com/squid55/Jetson-AI-Camera) — Jetson Orin Nano YOLOv8
- [Multi-Board-Viewer](https://github.com/squid55/Multi-Board-Viewer) — 멀티 보드 통합 뷰어
