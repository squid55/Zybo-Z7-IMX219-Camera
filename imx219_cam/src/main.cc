/**
 * IMX219 MIPI Camera on Zybo Z7-20
 * Xilinx MIPI CSI-2 Rx Subsystem + v_demosaic + v_gamma_lut pipeline
 *
 * Hardware: Zybo Z7-20 + IMX219 (RPi Camera v2)
 * Vitis: 2023.2 Unified IDE (SDT mode)
 */

#include "xparameters.h"
#include "xil_io.h"
#include "sleep.h"

extern "C" {
#include "platform/platform.h"
}

#include "imx219/IMX219.h"
#include "imx219/ScuGicInterruptController.h"
#include "imx219/PS_GPIO.h"
#include "imx219/AXI_VDMA.h"
#include "imx219/PS_IIC.h"

/* ===== SDT mode: use BASEADDR instead of DEVICE_ID ===== */
#define IRPT_CTL_DEVID      XPAR_XSCUGIC_0_BASEADDR
#define GPIO_DEVID           XPAR_XGPIOPS_0_BASEADDR
#define GPIO_IRPT_ID         52
#define CAM_I2C_DEVID        XPAR_XIICPS_0_BASEADDR
#define CAM_I2C_IRPT_ID      57
#define VDMA_DEVID           XPAR_XAXIVDMA_0_BASEADDR
#define VDMA_MM2S_IRPT_ID    30
#define VDMA_S2MM_IRPT_ID    31
#define CAM_I2C_SCLK_RATE    100000

#define DDR_BASE_ADDR        XPAR_PS7_DDR_0_BASEADDRESS
#define MEM_BASE_ADDR        (DDR_BASE_ADDR + 0x0A000000)

#define VTC_DEVID            XPAR_XVTC_0_BASEADDR
#define DYNCLK_DEVID         XPAR_VIDEO_DYNCLK_BASEADDR

/* ===== New Xilinx IP base addresses ===== */
#define MIPI_CSI2_BASEADDR   0x43C20000
#define DEMOSAIC_BASEADDR    0x43C30000
#define GAMMA_LUT_BASEADDR   0x43C40000  /* try old gamma address */

/* ===== HLS IP Register Offsets (v_demosaic, v_gamma_lut) ===== */
#define HLS_AP_CTRL          0x00
#define HLS_AP_START         0x01
#define HLS_AP_AUTO_RESTART  0x80

/* v_demosaic registers */
#define DEMOSAIC_REG_WIDTH       0x10
#define DEMOSAIC_REG_HEIGHT      0x18
#define DEMOSAIC_REG_BAYER_PHASE 0x20

/* v_gamma_lut registers */
#define GAMMA_REG_WIDTH          0x10
#define GAMMA_REG_HEIGHT         0x18
#define GAMMA_REG_VIDEO_FORMAT   0x20
#define GAMMA_LUT_0_BASE         0x0800
#define GAMMA_LUT_1_BASE         0x1000
#define GAMMA_LUT_2_BASE         0x1800

/* MIPI CSI-2 Rx Controller registers */
#define CSI_CCR              0x00  /* Core Configuration: bit0=Enable, bit1=SoftReset */
#define CSI_PCR              0x04  /* Protocol Config: bits[12:8]=ActiveLanes (0=1,1=2) */

/* IMX219 Bayer phase (BGGR after readout) */
#define IMX219_BAYER_PHASE   2  /* GBRG */

using namespace digilent;

/* ================================================================
 * Xilinx IP Initialization Functions
 * ================================================================ */

/**
 * Initialize MIPI CSI-2 Rx Subsystem
 * - Soft reset, configure 2 active lanes, enable
 */
void mipi_csi2_init()
{
    xil_printf("  MIPI CSI-2 Rx init...\r\n");

    /* Soft reset */
    Xil_Out32(MIPI_CSI2_BASEADDR + CSI_CCR, 0x02);
    usleep(1000);
    Xil_Out32(MIPI_CSI2_BASEADDR + CSI_CCR, 0x00);
    usleep(1000);

    /* Configure 2 active lanes (value 1 = 2 lanes in bits[12:8]) */
    u32 pcr = Xil_In32(MIPI_CSI2_BASEADDR + CSI_PCR);
    pcr = (pcr & ~(0x1F << 8)) | (0x01 << 8);
    Xil_Out32(MIPI_CSI2_BASEADDR + CSI_PCR, pcr);

    /* Enable core */
    Xil_Out32(MIPI_CSI2_BASEADDR + CSI_CCR, 0x01);

    xil_printf("  MIPI CSI-2 Rx: enabled (2-lane)\r\n");
}

/**
 * Disable MIPI CSI-2 Rx
 */
void mipi_csi2_disable()
{
    Xil_Out32(MIPI_CSI2_BASEADDR + CSI_CCR, 0x00);
}

/**
 * Initialize v_demosaic (Bayer to RGB conversion)
 * HLS IP: set resolution, bayer phase, then start with auto-restart
 */
void demosaic_init(u32 width, u32 height)
{
    xil_printf("  Demosaic init (%lux%lu, phase=%d)...\r\n", width, height, IMX219_BAYER_PHASE);

    Xil_Out32(DEMOSAIC_BASEADDR + DEMOSAIC_REG_WIDTH, width);
    Xil_Out32(DEMOSAIC_BASEADDR + DEMOSAIC_REG_HEIGHT, height);
    Xil_Out32(DEMOSAIC_BASEADDR + DEMOSAIC_REG_BAYER_PHASE, IMX219_BAYER_PHASE);

    /* Start with auto-restart */
    Xil_Out32(DEMOSAIC_BASEADDR + HLS_AP_CTRL, HLS_AP_AUTO_RESTART | HLS_AP_START);

    xil_printf("  Demosaic: started\r\n");
}

/**
 * Initialize v_gamma_lut (gamma correction)
 * HLS IP: set resolution, video format, load LUT, then start
 *
 * Using gamma ~0.7 (power curve) for initial testing.
 * Linear LUT can be used by setting gamma_val = 1.0.
 */
static u16 gamma_lut_table[1024];

void generate_gamma_lut(float gamma_val)
{
    for (int i = 0; i < 1024; i++) {
        float normalized = (float)i / 1023.0f;
        float corrected = 0;
        if (gamma_val == 1.0f) {
            corrected = normalized;
        } else {
            /* Use powf for gamma correction */
            corrected = 1.0f;
            float base = normalized;
            /* Simple power approximation: x^gamma using repeated multiplication
             * For gamma=0.7, we use a lookup approach */
            float log_val = 0;
            if (normalized > 0) {
                /* x^gamma = exp(gamma * ln(x)) */
                /* Simple approximation: iterative */
                corrected = normalized;
                for (int j = 0; j < 10; j++) {
                    float f = 1.0f;
                    float x = corrected;
                    /* Newton's method: solve y^(1/gamma) = normalized_input */
                    /* Actually, let's just use a simple polynomial approximation */
                    corrected = corrected; /* placeholder */
                }
                /* For now, use linear until we can include math.h */
                corrected = normalized;
            }
        }
        gamma_lut_table[i] = (u16)(corrected * 1023.0f);
        if (gamma_lut_table[i] > 1023) gamma_lut_table[i] = 1023;
    }
}

/* Global flag for deferred LUT loading */
static volatile int g_gamma_lut_loaded = 0;

void gamma_lut_init(u32 width, u32 height)
{
    xil_printf("  Gamma LUT init (%lux%lu)...\r\n", width, height);

    Xil_Out32(GAMMA_LUT_BASEADDR + GAMMA_REG_WIDTH, width);
    Xil_Out32(GAMMA_LUT_BASEADDR + GAMMA_REG_HEIGHT, height);
    Xil_Out32(GAMMA_LUT_BASEADDR + GAMMA_REG_VIDEO_FORMAT, 0); /* RGB */

    /* Load gamma LUT with correction + white balance
     * R,B channels: boost 1.8x + gamma 2.0
     * G channel: 1.0x + gamma 2.0
     * Gamma: output = sqrt(input/1023) * 1023 (approx gamma 2.0) */
    u32 lut_bases[3] = {GAMMA_LUT_0_BASE, GAMMA_LUT_1_BASE, GAMMA_LUT_2_BASE};
    /* boost factors: ch0=R(1.8x), ch1=G(1.0x), ch2=B(1.8x) */
    /* WB + brightness: R=250%, G=170%, B=225% */
    u32 boost[3] = {250, 170, 225};
    for (int ch = 0; ch < 3; ch++) {
        for (int i = 0; i < 512; i++) {
            u32 v0 = 2*i;
            u32 v1 = 2*i + 1;
            v0 = v0 * boost[ch] / 100; if (v0 > 1023) v0 = 1023;
            v1 = v1 * boost[ch] / 100; if (v1 > 1023) v1 = 1023;
            u32 packed = (v1 << 16) | v0;
            Xil_Out32(GAMMA_LUT_BASEADDR + lut_bases[ch] + i * 4, packed);
        }
        xil_printf("    LUT ch%d OK (boost=%lu%%)\r\n", ch, boost[ch]);
    }

    /* Now start */
    Xil_Out32(GAMMA_LUT_BASEADDR + HLS_AP_CTRL, HLS_AP_AUTO_RESTART | HLS_AP_START);
    xil_printf("  Gamma LUT: started\r\n");
}

/* Load gamma LUT one word at a time from main loop */
void gamma_lut_load_deferred()
{
    if (g_gamma_lut_loaded) return;

    xil_printf("Loading gamma LUT...\r\n");
    u32 lut_bases[3] = {GAMMA_LUT_0_BASE, GAMMA_LUT_1_BASE, GAMMA_LUT_2_BASE};
    for (int ch = 0; ch < 3; ch++) {
        for (int i = 0; i < 512; i++) {
            u32 packed = (((u32)(2*i+1)) << 16) | ((u32)(2*i));
            Xil_Out32(GAMMA_LUT_BASEADDR + lut_bases[ch] + i * 4, packed);
        }
        xil_printf("  ch%d OK\r\n", ch);
    }
    g_gamma_lut_loaded = 1;
    xil_printf("Gamma LUT loaded.\r\n");
}

/* ================================================================
 * Pipeline Control
 * ================================================================ */

void pipeline_mode_change(AXI_VDMA<ScuGicInterruptController>& vdma_driver,
                          IMX219& cam, VideoOutput& vid,
                          Resolution res, IMX219_cfg::mode_t mode)
{
    u32 h_active = timing[static_cast<int>(res)].h_active;
    u32 v_active = timing[static_cast<int>(res)].v_active;

    xil_printf("Setting up pipeline: %lux%lu\r\n", h_active, v_active);

    /* === Reset input pipeline (back to front) === */
    vdma_driver.resetWrite();
    mipi_csi2_disable();
    cam.reset();

    /* === Configure processing pipeline === */
    demosaic_init(h_active, v_active);
    gamma_lut_init(h_active, v_active);

    /* === Configure VDMA write (camera → DDR) === */
    vdma_driver.configureWrite(h_active, v_active);

    /* === Initialize camera === */
    cam.init();

    /* === Enable input pipeline (back to front) === */
    vdma_driver.enableWrite();
    mipi_csi2_init();
    cam.set_mode(mode);

    /* === Setup output pipeline === */
    vid.reset();
    vdma_driver.resetRead();

    vid.configure(res);
    vdma_driver.configureRead(h_active, v_active);

    vid.enable();
    vdma_driver.enableRead();
}

/* ================================================================
 * Main
 * ================================================================ */

int main()
{
    init_platform();

    xil_printf("\r\n====================================\r\n");
    xil_printf("  IMX219 MIPI Camera (Xilinx IP)\r\n");
    xil_printf("  Zybo Z7-20 + IMX219\r\n");
    xil_printf("====================================\r\n\r\n");

    /* Initialize interrupt controller */
    xil_printf("1. ScuGic init...\r\n");
    ScuGicInterruptController irpt_ctl(IRPT_CTL_DEVID);
    xil_printf("   OK\r\n");

    /* Initialize GPIO */
    xil_printf("2. GPIO init...\r\n");
    PS_GPIO<ScuGicInterruptController> gpio_driver(GPIO_DEVID, irpt_ctl, GPIO_IRPT_ID);
    xil_printf("   OK\r\n");

    /* Initialize I2C */
    xil_printf("3. IIC init...\r\n");
    PS_IIC<ScuGicInterruptController> iic_driver(CAM_I2C_DEVID, irpt_ctl, CAM_I2C_IRPT_ID, 100000);
    xil_printf("   OK\r\n");

    /* Create camera and video drivers */
    xil_printf("4. Initializing IMX219 camera...\r\n");
    IMX219 cam(iic_driver, gpio_driver);

    AXI_VDMA<ScuGicInterruptController> vdma_driver(VDMA_DEVID, MEM_BASE_ADDR, irpt_ctl,
            VDMA_MM2S_IRPT_ID,
            VDMA_S2MM_IRPT_ID);
    VideoOutput vid(VTC_DEVID, DYNCLK_DEVID);

    /* Start camera at 720p 60fps */
    xil_printf("5. Starting video pipeline...\r\n");
    pipeline_mode_change(vdma_driver, cam, vid,
                         Resolution::R1280_720_60_PP,
                         IMX219_cfg::mode_t::MODE_720P_1280_720_60fps);

    /* Adjust exposure/gain */
    xil_printf("Adjusting gain/exposure...\r\n");
    cam.set_gain(225);
    cam.set_exposure(4000);

    /* Dump and FIX VDMA S2MM configuration */
    u32 vdma_base = 0x43000000;
    u32 hsize = Xil_In32(vdma_base + 0xA8);
    u32 stride = Xil_In32(vdma_base + 0xA4) & 0xFFFF;
    u32 vsize = Xil_In32(vdma_base + 0xA0);
    xil_printf("VDMA S2MM: HSIZE=%lu STRIDE=%lu VSIZE=%lu\r\n", hsize, stride, vsize);

    /* If HSIZE != 1280*3, the subset converter may not be working.
     * Override VDMA for 4 bytes/pixel if needed */
    if (hsize != 1280*3) {
        xil_printf("  HSIZE mismatch! Overriding to 3 bytes/pixel (3840)...\r\n");
        /* Stop S2MM */
        u32 cr = Xil_In32(vdma_base + 0x30);
        Xil_Out32(vdma_base + 0x30, cr & ~1);
        usleep(1000);

        /* Reconfigure for 3 bytes per pixel */
        u32 new_hsize = 1280 * 3;  /* 3840 */
        u32 new_stride = 1280 * 3;
        u32 frame_size = new_stride * 720;

        /* Set new frame buffer addresses (need more space) */
        u32 fb0 = 0x0A000000;
        u32 fb1 = fb0 + frame_size;
        u32 fb2 = fb1 + frame_size;

        Xil_Out32(vdma_base + 0xAC, fb0);  /* S2MM StartAddr 0 */
        Xil_Out32(vdma_base + 0xB0, fb1);  /* S2MM StartAddr 1 */
        Xil_Out32(vdma_base + 0xB4, fb2);  /* S2MM StartAddr 2 */
        Xil_Out32(vdma_base + 0xA4, new_stride);
        Xil_Out32(vdma_base + 0xA8, new_hsize);

        /* Also fix MM2S (read) side */
        u32 cr2 = Xil_In32(vdma_base + 0x00);
        Xil_Out32(vdma_base + 0x00, cr2 & ~1);
        usleep(1000);
        Xil_Out32(vdma_base + 0x5C, fb0);
        Xil_Out32(vdma_base + 0x60, fb1);
        Xil_Out32(vdma_base + 0x64, fb2);
        Xil_Out32(vdma_base + 0x54, new_stride);
        Xil_Out32(vdma_base + 0x58, new_hsize);

        /* Restart both */
        Xil_Out32(vdma_base + 0x30, cr | 1);
        Xil_Out32(vdma_base + 0xA0, 720); /* write VSIZE starts S2MM */
        Xil_Out32(vdma_base + 0x00, cr2 | 1);
        Xil_Out32(vdma_base + 0x50, 720); /* write VSIZE starts MM2S */

        xil_printf("  VDMA reconfigured: HSIZE=%lu STRIDE=%lu\r\n", new_hsize, new_stride);
        xil_printf("  FB: 0x%08lx 0x%08lx 0x%08lx\r\n", fb0, fb1, fb2);
    } else {
        xil_printf("  HSIZE OK (3 bytes/pixel)\r\n");
    }

    xil_printf("\r\n====================================\r\n");
    xil_printf("  Video pipeline active (720p 60fps)\r\n");
    xil_printf("  IMX219 → MIPI CSI-2 Rx → Demosaic\r\n");
    xil_printf("  → Gamma LUT → VDMA → HDMI\r\n");
    xil_printf("====================================\r\n\r\n");

    u32 fb_base = MEM_BASE_ADDR + 0x100000;
    u32 line_bytes = 1280 * 3;
    u32 frame_count = 0;

    while (1) {
        frame_count++;

        /* Keep gamma LUT running */
        u32 gamma_st = Xil_In32(GAMMA_LUT_BASEADDR + HLS_AP_CTRL);
        if ((gamma_st & 0x81) != 0x81) {
            Xil_Out32(GAMMA_LUT_BASEADDR + HLS_AP_CTRL, HLS_AP_AUTO_RESTART | HLS_AP_START);
        }

        if (frame_count % 50 == 0) {
            Xil_DCacheInvalidateRange(0x0A100000, 0x600000);
            u32 demo_ctrl = Xil_In32(DEMOSAIC_BASEADDR + HLS_AP_CTRL);
            u32 gamma_ctrl = Xil_In32(GAMMA_LUT_BASEADDR + HLS_AP_CTRL);
            u32 csi_isr = Xil_In32(MIPI_CSI2_BASEADDR + 0x24);

            /* VDMA status: base=0x43000000 */
            u32 vdma_base = 0x43000000;
            u32 mm2s_sr = Xil_In32(vdma_base + 0x04); /* read status */
            u32 s2mm_sr = Xil_In32(vdma_base + 0x34); /* write status */
            u32 s2mm_cr = Xil_In32(vdma_base + 0x30); /* write control */

            xil_printf("\r\n[%05lu] DEMO=0x%02lx GAMMA=0x%02lx CSI_ISR=0x%08lx\r\n",
                       frame_count, demo_ctrl, gamma_ctrl, csi_isr);
            xil_printf("  VDMA: S2MM_CR=0x%08lx S2MM_SR=0x%08lx MM2S_SR=0x%08lx\r\n",
                       s2mm_cr, s2mm_sr, mm2s_sr);

            /* Check all 3 frame buffers - first 12 bytes each */
            u32 fb_addrs[3] = {0x0A100000, 0x0A3A3000, 0x0A646000};
            for (int f = 0; f < 3; f++) {
                xil_printf("  F%d: ", f);
                for (int i = 0; i < 12; i++) {
                    u8 val = *(volatile u8*)(fb_addrs[f] + i);
                    xil_printf("%02x ", val);
                }
                xil_printf("\r\n");
            }
        }
        usleep(200000);
    }

    cleanup_platform();
    return 0;
}
