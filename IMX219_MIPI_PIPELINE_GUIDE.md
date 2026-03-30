# IMX219 MIPI Camera Pipeline Guide

Zybo Z7-20 + IMX219 (RPi Camera v2) HDMI 출력 파이프라인
Vivado/Vitis 2023.2 | Xilinx MIPI CSI-2 Rx Subsystem

---

## 전체 아키텍처

```
IMX219 (RAW10, 720p60)
    │  MIPI CSI-2, 2-lane, 700 Mbps
    ▼
┌──────────────────────────┐
│  MIPI CSI-2 Rx Subsystem │  Xilinx IP (soft D-PHY 내장)
│  SupportLevel=1          │  0x43C20000
└──────────┬───────────────┘
           │  AXI4-Stream (16-bit, RAW10 Bayer)
           ▼
┌──────────────────────────┐
│  v_demosaic              │  Xilinx HLS IP
│  10-bit, 1280x720       │  0x43C30000
│  Bayer phase = 2 (GBRG) │
└──────────┬───────────────┘
           │  AXI4-Stream (32-bit, 10-bit RGB packed)
           ▼
┌──────────────────────────┐
│  v_gamma_lut             │  Xilinx HLS IP
│  10-bit, 1280x720       │  0x43C40000
│  채널별 WB + 밝기 보정    │
└──────────┬───────────────┘
           │  AXI4-Stream (32-bit)
           ▼
┌──────────────────────────┐
│  axis_subset_converter   │  32-bit → 24-bit
│  TDATA_REMAP:            │  10-bit → 8-bit per channel
│  {[29:22],[19:12],[9:2]} │
└──────────┬───────────────┘
           │  AXI4-Stream (24-bit, 8-bit RGB)
           ▼
┌──────────────────────────┐
│  AXI VDMA                │  Triple-buffered DDR 프레임 버퍼
│  S2MM: 3840 bytes/line   │  0x43000000
│  MM2S: 3840 bytes/line   │
└──────────┬───────────────┘
           │  AXI4-Stream (24-bit RGB)
           ▼
┌──────────────────────────┐
│  v_axi4s_vid_out → rgb2dvi → HDMI TX
│  720p 60Hz               │
└──────────────────────────┘
```

---

## IP 주소 맵

| IP | Base Address | Range | AXI-Lite Clock |
|---|---|---|---|
| AXI VDMA | `0x43000000` | 64KB | 50 MHz |
| Video Timing Controller | `0x43C10000` | 64KB | 50 MHz |
| MIPI CSI-2 Rx Subsystem | `0x43C20000` | 4KB | 50 MHz |
| v_demosaic | `0x43C30000` | 64KB | 150 MHz |
| v_gamma_lut | `0x43C40000` | 64KB | 150 MHz |
| Video Dynamic Clock | `0x43C00000` | 64KB | 50 MHz |

---

## 클럭 도메인

| 클럭 | 주파수 | 소스 | 사용처 |
|---|---|---|---|
| `s_axil_clk_50` | 50 MHz | `clk_wiz_0/clk_out1` | PS AXI-Lite, VDMA 제어 |
| `mm_clk_150` | 150 MHz | `clk_wiz_0/clk_out2` | 비디오 파이프라인 전체 |
| `ref_clk_200` | 200 MHz | `clk_wiz_0/clk_out3` | MIPI D-PHY 참조 클럭 |

리셋: `rst_mm_clk_150` (150 MHz 도메인 전용 proc_sys_reset)

---

## MIPI CSI-2 Rx Subsystem

### 설정 (Vivado)

| 파라미터 | 값 | 설명 |
|---|---|---|
| `CMN_NUM_LANES` | 2 | MIPI 데이터 레인 수 |
| `CMN_PXL_FORMAT` | RAW10 | IMX219 출력 포맷 |
| `C_HS_LINE_RATE` | 700 | Mbps per lane |
| `C_HS_SETTLE_NS` | 100 | D-PHY HS settle 타이밍 |
| `SupportLevel` | 1 | Soft D-PHY 포함 (7-series) |
| `DPY_EN_REG_IF` | false | D-PHY 레지스터 비활성화 |

### 소프트웨어 초기화

```c
/* Soft reset */
Xil_Out32(0x43C20000 + 0x00, 0x02);  // CCR: soft reset
usleep(1000);
Xil_Out32(0x43C20000 + 0x00, 0x00);  // clear reset

/* Configure 2 active lanes */
u32 pcr = Xil_In32(0x43C20000 + 0x04);
pcr = (pcr & ~(0x1F << 8)) | (0x01 << 8);  // 2 lanes
Xil_Out32(0x43C20000 + 0x04, pcr);

/* Enable */
Xil_Out32(0x43C20000 + 0x00, 0x01);  // CCR: enable
```

### MIPI 물리 핀 (Zybo Z7-20)

| 신호 | 핀 | IO 표준 |
|---|---|---|
| CLK HS P/N | J18 / H18 | LVDS_25 |
| CLK LP P/N | H20 / J19 | HSUL_12 |
| DATA0 HS P/N | M19 / M20 | LVDS_25 |
| DATA0 LP P/N | L19 / M18 | HSUL_12 |
| DATA1 HS P/N | L16 / L17 | LVDS_25 |
| DATA1 LP P/N | J20 / L20 | HSUL_12 |
| CAM GPIO | G20 | LVCMOS33 |
| CAM I2C SCL/SDA | F20 / F19 | LVCMOS33 |

---

## v_demosaic (Bayer → RGB)

### 설정

| 파라미터 | 값 |
|---|---|
| `MAX_DATA_WIDTH` | 10 |
| `MAX_COLS` | 1920 |
| `MAX_ROWS` | 1080 |
| `SAMPLES_PER_CLOCK` | 1 |

### 레지스터 맵

| Offset | 이름 | 설명 |
|---|---|---|
| `0x00` | AP_CTRL | bit0=start(COH), bit7=auto_restart |
| `0x10` | width | 활성 가로 픽셀 (1280) |
| `0x18` | height | 활성 세로 라인 (720) |
| `0x20` | bayer_phase | 0=RGGB, 1=GRBG, 2=GBRG, 3=BGGR |

### 초기화

```c
Xil_Out32(DEMOSAIC + 0x10, 1280);
Xil_Out32(DEMOSAIC + 0x18, 720);
Xil_Out32(DEMOSAIC + 0x20, 2);     // GBRG
Xil_Out32(DEMOSAIC + 0x00, 0x81);  // auto_restart + start
```

---

## v_gamma_lut (감마/화이트밸런스)

### 레지스터 맵

| Offset | 이름 | 설명 |
|---|---|---|
| `0x00` | AP_CTRL | bit0=start, bit7=auto_restart |
| `0x10` | width | 1280 |
| `0x18` | height | 720 |
| `0x20` | video_format | 0 = RGB |
| `0x0800-0x0FFF` | gamma_lut_0 | R 채널 LUT (512 words) |
| `0x1000-0x17FF` | gamma_lut_1 | G 채널 LUT (512 words) |
| `0x1800-0x1FFF` | gamma_lut_2 | B 채널 LUT (512 words) |

### LUT 포맷

1024개 엔트리 (10-bit input → 16-bit output), 2개씩 32-bit 워드에 패킹:

```
Word n: [15:0]  = LUT[2n]     (even entry)
        [31:16] = LUT[2n+1]   (odd entry)
```

### 화이트 밸런스 설정

```c
/* 채널별 부스트: R=250%, G=170%, B=225% */
u32 boost[3] = {250, 170, 225};
u32 lut_bases[3] = {0x0800, 0x1000, 0x1800};

for (int ch = 0; ch < 3; ch++) {
    for (int i = 0; i < 512; i++) {
        u32 v0 = (2*i)   * boost[ch] / 100;
        u32 v1 = (2*i+1) * boost[ch] / 100;
        if (v0 > 1023) v0 = 1023;
        if (v1 > 1023) v1 = 1023;
        Xil_Out32(GAMMA + lut_bases[ch] + i*4, (v1 << 16) | v0);
    }
}
```

주의: LUT는 IP 시작(`AP_CTRL=0x81`) **전에** 로드해야 함. 실행 중 로드 시 IP가 멈출 수 있음.

---

## axis_subset_converter (10-bit → 8-bit)

32-bit packed 10-bit RGB → 24-bit 8-bit RGB 변환.

```
Input  (32-bit): [31:30]=pad, [29:20]=B, [19:10]=G, [9:0]=R
Output (24-bit): [23:16]=B[9:2], [15:8]=G[9:2], [7:0]=R[9:2]

TDATA_REMAP: tdata[29:22], tdata[19:12], tdata[9:2]
```

각 10-bit 채널의 상위 8-bit만 추출 (하위 2-bit 버림).

---

## VDMA 설정

### 주의사항

Digilent AXI_VDMA 래퍼가 HSIZE를 잘못 계산함 (`S2MM_TDATA_WIDTH`를 하드웨어에서 읽지만, 값이 실제 스트림과 불일치). **수동 오버라이드 필요:**

```c
u32 vdma = 0x43000000;
u32 hsize = Xil_In32(vdma + 0xA8);

if (hsize != 1280 * 3) {
    /* S2MM (write) 재설정 */
    Xil_Out32(vdma + 0x30, Xil_In32(vdma + 0x30) & ~1);  // stop
    Xil_Out32(vdma + 0xA4, 3840);   // stride = 1280 * 3
    Xil_Out32(vdma + 0xA8, 3840);   // hsize  = 1280 * 3
    Xil_Out32(vdma + 0xAC, fb0);    // frame 0 addr
    Xil_Out32(vdma + 0xB0, fb1);    // frame 1 addr
    Xil_Out32(vdma + 0xB4, fb2);    // frame 2 addr
    Xil_Out32(vdma + 0x30, Xil_In32(vdma + 0x30) | 1);   // restart
    Xil_Out32(vdma + 0xA0, 720);    // vsize (triggers start)
}
```

### 프레임 버퍼 레이아웃

```
Frame 0: 0x0A000000  (1280 * 720 * 3 = 2,764,800 bytes)
Frame 1: 0x0A2A3000
Frame 2: 0x0A546000
```

---

## IMX219 카메라 설정

### I2C

- 주소: `0x10`
- Chip ID: `0x0219`
- SCLK: 100 kHz

### PLL 구성 (MIPI 출력 레이트)

```
Input:          24 MHz
PREPLLCK_OP_DIV: 3
PLL_OP_MPY:      85
OP_PLL_CLK:      24 / 3 * 85 = 680 MHz → ~680 Mbps/lane
```

D-PHY의 `HS_LINE_RATE`는 이 값에 근접해야 함 (현재 700).

### 최종 설정

| 파라미터 | 값 |
|---|---|
| Analog Gain | 225 (0-232, 0dB~20.6dB) |
| Exposure | 4000 (coarse integration time) |
| Mode | 720p 60fps (1280x720, 2x2 binning) |
| Output Format | RAW10 |

---

## 트러블슈팅 기록

### 1. HDMI 노이즈 (스펙클 패턴)

**원인**: `axis_broadcaster`가 CNN IP(M01)로도 데이터를 보내야 하는데, CNN이 데이터를 소비하지 않아 전체 파이프라인이 간헐적으로 stall → MIPI 수신 버퍼 오버플로우.

**해결**: broadcaster + CNN IP 제거. subset_converter → VDMA 직접 연결.

**진단**: `CSI_ISR=0x80060000` (에러 플래그), broadcaster 제거 후 `CSI_ISR=0x00000000`.

### 2. 형광등이 4개로 색깔별 복사

**원인**: VDMA `HSIZE`가 10240 (8 bytes/pixel)으로 잘못 설정. 실제 스트림은 3 bytes/pixel (3840 bytes/line). 3:8 비율 불일치로 라인이 밀리면서 색 채널이 분리되어 보임.

**해결**: VDMA S2MM HSIZE를 3840으로 수동 오버라이드.

### 3. Gamma LUT 레지스터 접근 불가 (data abort)

**원인**: BD에서 `0x43C50000`으로 주소 할당했지만 AXI crossbar에서 실제 라우팅 안 됨.

**해결**: Vivado에서 주소를 `0x43C40000`으로 변경 후 재합성.

### 4. Gamma LUT가 계속 멈춤 (AP_CTRL=0x00)

**원인**: LUT 테이블을 IP 시작 후 로드하면 IP 상태가 불안정해짐.

**해결**: LUT를 IP 시작 **전에** 로드 + 메인 루프에서 AP_CTRL을 주기적으로 확인/재시작.

### 5. 녹색 틴트

**원인**: Bayer 센서 특성상 G 채널이 2배 오버샘플링 + 감마 보정 없음.

**해결**: Gamma LUT에서 채널별 부스트 (R=250%, G=170%, B=225%).

### 6. Vitis SDT 생성 실패

**원인**: Vitis 2023.2 SDT 생성기 버그 — `v_gamma_lut` + `axis_broadcaster` 조합 처리 불가.

**해결**: 구 플랫폼 BSP 유지 + `xsct`로 새 비트스트림/ELF 별도 프로그램.

---

## 빌드 방법

### 소프트웨어 빌드 + 보드 프로그램

```bash
bash /home/hyeonjun/build_and_run.sh
```

이 스크립트는:
1. `arm-none-eabi-g++`로 크로스 컴파일
2. ELF 링크
3. `xsct`로 FPGA 프로그램 + ELF 다운로드

### Vivado 하드웨어 수정 시

```bash
source /home/hyeonjun/AMD/Vivado/2023.2/settings64.sh
vivado -mode batch -source <script>.tcl -notrace
```

주요 TCL 스크립트:
- `remove_broadcaster.tcl` — broadcaster/CNN 제거
- `fix_gamma_addr.tcl` — gamma LUT 주소 이동
- `fix_mipi_680.tcl` — MIPI 라인 레이트 조정

---

## 파일 구조

```
imx219_pcam_project/
├── imx219_cam/                    # Vitis 애플리케이션
│   └── src/
│       ├── main.cc                # 메인 (파이프라인 초기화 + 카메라 제어)
│       ├── IMX219.cpp             # IMX219 드라이버 (빈 파일, 헤더에 구현)
│       ├── platform.c             # PS 플랫폼 초기화
│       ├── MIPI_D_PHY_RX.h        # (미사용, 구 Digilent 드라이버)
│       ├── MIPI_CSI_2_RX.h        # (미사용, 구 Digilent 드라이버)
│       ├── imx219/
│       │   ├── IMX219.h           # IMX219 레지스터 설정 + I2C 드라이버
│       │   ├── AXI_VDMA.h         # Digilent VDMA 래퍼
│       │   ├── ScuGicInterruptController.h
│       │   ├── PS_GPIO.h
│       │   └── PS_IIC.h
│       └── hdmi/
│           └── VideoOutput.h      # VTC + DynClk 제어
├── imx219_zybo_platform/          # Vitis 플랫폼 (BSP)
├── cnn_pcam.xpr                   # Vivado 프로젝트 → /home/hyeonjun/cnn_pcam_project/
└── build_and_run.sh               # → /home/hyeonjun/build_and_run.sh
```

---

## 리소스 사용량 (xc7z020)

| 리소스 | 사용 | 가용 | 사용률 |
|---|---|---|---|
| Slice LUT | ~14,000 | 53,200 | ~26% |
| Slice Registers | ~21,000 | 106,400 | ~20% |
| Block RAM | ~30 | 140 | ~21% |
| DSP | ~12 | 220 | ~5% |

CNN IP 제거 후 리소스 여유 충분. 추가 IP 배치 가능.

---

## 참고 자료

- [Xilinx MIPI CSI-2 Rx Subsystem (PG232)](https://docs.amd.com/r/en-US/pg232-mipi-csi2-rx)
- [gtaylormb: IMX219 on Ultra96-V2](https://github.com/gtaylormb/ultra96v2_imx219_to_displayport)
- [Numato: IMX219 on Zynq-7020](https://numato.com/blog/live-imx219-video-pipeline-implementation-on-tityracore-zynq-7000-soc/)
- [Sony IMX219 Datasheet](https://www.sony-semicon.com/files/62/flyer_industry/IMX219PQH5_Flyer.pdf)
