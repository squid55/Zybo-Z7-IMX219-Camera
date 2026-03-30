/**
 * IMX219 MIPI Camera Test on Zybo Z7-20
 *
 * Based on Digilent Pcam-5C demo, modified for IMX219 (RPi Camera v2).
 * Outputs camera feed to HDMI.
 *
 * Hardware: Zybo Z7-20 + IMX219 (MIPI CSI-2)
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

#include "MIPI_D_PHY_RX.h"
#include "MIPI_CSI_2_RX.h"

/* ===== SDT mode: use BASEADDR instead of DEVICE_ID ===== */
#define IRPT_CTL_DEVID      XPAR_XSCUGIC_0_BASEADDR
#define GPIO_DEVID           XPAR_XGPIOPS_0_BASEADDR
#define GPIO_IRPT_ID         XPAR_XGPIOPS_0_INTERRUPTS
#define CAM_I2C_DEVID        XPAR_XIICPS_0_BASEADDR
#define CAM_I2C_IRPT_ID      XPAR_XIICPS_0_INTERRUPTS
#define VDMA_DEVID           XPAR_XAXIVDMA_0_BASEADDR
#define VDMA_MM2S_IRPT_ID    XPAR_AXI_VDMA_0_INTERRUPTS
#define VDMA_S2MM_IRPT_ID    XPAR_AXI_VDMA_0_INTERRUPTS_1
#define CAM_I2C_SCLK_RATE    100000

#define DDR_BASE_ADDR        XPAR_PS7_DDR_0_BASEADDRESS
#define MEM_BASE_ADDR        (DDR_BASE_ADDR + 0x0A000000)
#define GAMMA_BASE_ADDR      XPAR_AXI_GAMMACORRECTION_0_BASEADDR

#define MIPI_CSI2_BASEADDR   XPAR_MIPI_CSI_2_RX_0_BASEADDR
#define MIPI_DPHY_BASEADDR   XPAR_MIPI_D_PHY_RX_0_BASEADDR

#define VTC_DEVID            XPAR_XVTC_0_BASEADDR
#define DYNCLK_DEVID         XPAR_VIDEO_DYNCLK_BASEADDR

using namespace digilent;

void pipeline_mode_change(AXI_VDMA<ScuGicInterruptController>& vdma_driver,
                          IMX219& cam, VideoOutput& vid,
                          Resolution res, IMX219_cfg::mode_t mode)
{
    /* Bring up input pipeline back-to-front */
    {
        vdma_driver.resetWrite();
        MIPI_CSI_2_RX_mWriteReg(MIPI_CSI2_BASEADDR, CR_OFFSET,
                                (CR_RESET_MASK & ~CR_ENABLE_MASK));
        MIPI_D_PHY_RX_mWriteReg(MIPI_DPHY_BASEADDR, CR_OFFSET,
                                (CR_RESET_MASK & ~CR_ENABLE_MASK));
        cam.reset();
    }

    {
        vdma_driver.configureWrite(timing[static_cast<int>(res)].h_active,
                                   timing[static_cast<int>(res)].v_active);
        Xil_Out32(GAMMA_BASE_ADDR, 3);  /* Gamma = 1/1.8 */
        cam.init();
    }

    {
        vdma_driver.enableWrite();
        MIPI_CSI_2_RX_mWriteReg(MIPI_CSI2_BASEADDR, CR_OFFSET, CR_ENABLE_MASK);
        MIPI_D_PHY_RX_mWriteReg(MIPI_DPHY_BASEADDR, CR_OFFSET, CR_ENABLE_MASK);
        cam.set_mode(mode);
    }

    /* Bring up output pipeline back-to-front */
    {
        vid.reset();
        vdma_driver.resetRead();
    }

    {
        vid.configure(res);
        vdma_driver.configureRead(timing[static_cast<int>(res)].h_active,
                                  timing[static_cast<int>(res)].v_active);
    }

    {
        vid.enable();
        vdma_driver.enableRead();
    }
}

int main()
{
    init_platform();

    xil_printf("\r\n====================================\r\n");
    xil_printf("  IMX219 MIPI Camera Test\r\n");
    xil_printf("  Zybo Z7-20 + IMX219\r\n");
    xil_printf("====================================\r\n\r\n");

    /* Initialize camera pipeline */
    ScuGicInterruptController irpt_ctl(IRPT_CTL_DEVID);
    PS_GPIO<ScuGicInterruptController> gpio_driver(GPIO_DEVID, irpt_ctl, GPIO_IRPT_ID);
    PS_IIC<ScuGicInterruptController> iic_driver(CAM_I2C_DEVID, irpt_ctl, CAM_I2C_IRPT_ID, 100000);

    xil_printf("Initializing IMX219 camera...\r\n");

    IMX219 cam(iic_driver, gpio_driver);

    AXI_VDMA<ScuGicInterruptController> vdma_driver(VDMA_DEVID, MEM_BASE_ADDR, irpt_ctl,
            VDMA_MM2S_IRPT_ID,
            VDMA_S2MM_IRPT_ID);
    VideoOutput vid(VTC_DEVID, DYNCLK_DEVID);

    /* Start camera at 720p 60fps */
    pipeline_mode_change(vdma_driver, cam, vid,
                         Resolution::R1280_720_60_PP,
                         IMX219_cfg::mode_t::MODE_720P_1280_720_60fps);

    xil_printf("Video pipeline initialized (720p 60fps).\r\n");
    xil_printf("IMX219 camera streaming to HDMI.\r\n\r\n");

    /* Main loop */
    u32 frame_count = 0;
    while (1) {
        frame_count++;
        if (frame_count % 300 == 0) {
            xil_printf("[%06d] IMX219 streaming OK\r\n", frame_count);
        }
        usleep(200000);  /* 200ms */
    }

    cleanup_platform();
    return 0;
}
