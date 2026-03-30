/*
 * IMX219.h
 *
 * IMX219 MIPI CSI-2 Camera Driver for Zybo Z7-20
 * Based on OV5640 driver structure from Digilent Pcam-5C demo
 *
 * IMX219 (Sony) - Raspberry Pi Camera Module v2
 *   I2C Address: 0x10
 *   Chip ID: 0x0219
 *   MIPI: 2-lane, RAW10
 *   Max: 3280x2464 @15fps, 1920x1080 @30fps, 1280x720 @60fps
 */

#ifndef IMX219_H_
#define IMX219_H_

#include <sstream>
#include <iostream>
#include <cstdio>
#include <climits>

#include "I2C_Client.h"
#include "GPIO_Client.h"
#include "../hdmi/VideoOutput.h"

#define SIZEOF_ARRAY(x) sizeof(x)/sizeof(x[0])

namespace digilent {

typedef enum {OK=0, ERR_LOGICAL, ERR_GENERAL} Errc;

namespace IMX219_cfg {
	using config_word_t = struct { uint16_t addr; uint8_t data; } ;
	using mode_t = enum {
		MODE_720P_1280_720_60fps = 0,
		MODE_1080P_1920_1080_30fps,
		MODE_END
	};
	using config_modes_t = struct { mode_t mode; config_word_t const* cfg; size_t cfg_size; };

	/* ===== IMX219 Common Init Registers ===== */
	config_word_t const cfg_init_[] =
	{
		/* Software Reset */
		{0x0100, 0x00},  /* mode_select: standby */

		/* Access registers over 0x3000 */
		{0x30EB, 0x05},
		{0x30EB, 0x0C},
		{0x300A, 0xFF},
		{0x300B, 0xFF},
		{0x30EB, 0x05},
		{0x30EB, 0x09},

		/* CSI-2 related */
		{0x0114, 0x01},  /* CSI_LANE_MODE: 2-lane */
		{0x0128, 0x00},  /* DPHY_CTRL: auto */

		/* MIPI global timing */
		{0x012A, 0x18},  /* EXCK_FREQ[15:8] = 24MHz */
		{0x012B, 0x00},  /* EXCK_FREQ[7:0] */

		/* Frame/line length default (overridden by mode) */
		{0x0160, 0x06},  /* FRM_LENGTH_A[15:8] */
		{0x0161, 0xE2},  /* FRM_LENGTH_A[7:0] = 1762 */
		{0x0162, 0x0D},  /* LINE_LENGTH_A[15:8] */
		{0x0163, 0x78},  /* LINE_LENGTH_A[7:0] = 3448 */

		/* Analog gain */
		{0x0157, 0x00},  /* ANA_GAIN_GLOBAL_A */
		{0x0158, 0x01},  /* DIG_GAIN_GLOBAL_A[15:8] */
		{0x0159, 0x00},  /* DIG_GAIN_GLOBAL_A[7:0] */

		/* Exposure */
		{0x015A, 0x03},  /* COARSE_INTEGRATION_TIME_A[15:8] */
		{0x015B, 0xE8},  /* COARSE_INTEGRATION_TIME_A[7:0] */

		/* Output format: RAW10 */
		{0x018C, 0x0A},  /* CSI_DATA_FORMAT_A[15:8] = RAW10 */
		{0x018D, 0x0A},  /* CSI_DATA_FORMAT_A[7:0] */

		/* MIPI PHY timing */
		{0x0171, 0x01},
		{0x0174, 0x00},  /* BINNING_MODE_H: no binning */
		{0x0175, 0x00},  /* BINNING_MODE_V: no binning */

		/* PLL settings for 24MHz input */
		{0x0301, 0x05},  /* VTPXCK_DIV */
		{0x0303, 0x01},  /* VTSYCK_DIV */
		{0x0304, 0x03},  /* PREPLLCK_VT_DIV */
		{0x0305, 0x03},  /* PREPLLCK_OP_DIV */
		{0x0306, 0x00},  /* PLL_VT_MPY[10:8] */
		{0x0307, 0x39},  /* PLL_VT_MPY[7:0] = 57 */
		{0x030B, 0x01},  /* OPSYCK_DIV */
		{0x030C, 0x00},  /* PLL_OP_MPY[10:8] */
		{0x030D, 0x72},  /* PLL_OP_MPY[7:0] = 114 */
	};

	/* ===== 1280x720 @ 60fps ===== */
	config_word_t const cfg_720p_60fps_[] =
	{
		/* Crop */
		{0x0164, 0x01},  /* X_ADD_STA_A[11:8] */
		{0x0165, 0x68},  /* X_ADD_STA_A[7:0] = 360 */
		{0x0166, 0x0B},  /* X_ADD_END_A[11:8] */
		{0x0167, 0x67},  /* X_ADD_END_A[7:0] = 2919 */
		{0x0168, 0x02},  /* Y_ADD_STA_A[11:8] */
		{0x0169, 0x00},  /* Y_ADD_STA_A[7:0] = 512 */
		{0x016A, 0x07},  /* Y_ADD_END_A[11:8] */
		{0x016B, 0x9F},  /* Y_ADD_END_A[7:0] = 1951 */

		/* Output size */
		{0x016C, 0x05},  /* X_OUTPUT_SIZE[11:8] */
		{0x016D, 0x00},  /* X_OUTPUT_SIZE[7:0] = 1280 */
		{0x016E, 0x02},  /* Y_OUTPUT_SIZE[11:8] */
		{0x016F, 0xD0},  /* Y_OUTPUT_SIZE[7:0] = 720 */

		/* Binning 2x2 */
		{0x0174, 0x01},  /* BINNING_MODE_H: x2 */
		{0x0175, 0x01},  /* BINNING_MODE_V: x2 */

		/* Frame/line */
		{0x0160, 0x03},  /* FRM_LENGTH_A[15:8] */
		{0x0161, 0x14},  /* FRM_LENGTH_A[7:0] = 788 */
		{0x0162, 0x0D},  /* LINE_LENGTH_A[15:8] */
		{0x0163, 0x78},  /* LINE_LENGTH_A[7:0] = 3448 */
	};

	/* ===== 1920x1080 @ 30fps ===== */
	config_word_t const cfg_1080p_30fps_[] =
	{
		/* Crop */
		{0x0164, 0x02},  /* X_ADD_STA_A[11:8] */
		{0x0165, 0xA8},  /* X_ADD_STA_A[7:0] = 680 */
		{0x0166, 0x0A},  /* X_ADD_END_A[11:8] */
		{0x0167, 0x27},  /* X_ADD_END_A[7:0] = 2599 */
		{0x0168, 0x02},  /* Y_ADD_STA_A[11:8] */
		{0x0169, 0xB4},  /* Y_ADD_STA_A[7:0] = 692 */
		{0x016A, 0x06},  /* Y_ADD_END_A[11:8] */
		{0x016B, 0xEB},  /* Y_ADD_END_A[7:0] = 1771 */

		/* Output size */
		{0x016C, 0x07},  /* X_OUTPUT_SIZE[11:8] */
		{0x016D, 0x80},  /* X_OUTPUT_SIZE[7:0] = 1920 */
		{0x016E, 0x04},  /* Y_OUTPUT_SIZE[11:8] */
		{0x016F, 0x38},  /* Y_OUTPUT_SIZE[7:0] = 1080 */

		/* No binning */
		{0x0174, 0x00},
		{0x0175, 0x00},

		/* Frame/line */
		{0x0160, 0x06},  /* FRM_LENGTH_A[15:8] */
		{0x0161, 0xE2},  /* FRM_LENGTH_A[7:0] = 1762 */
		{0x0162, 0x0D},  /* LINE_LENGTH_A[15:8] */
		{0x0163, 0x78},  /* LINE_LENGTH_A[7:0] = 3448 */
	};

	config_modes_t const modes_[] =
	{
		{MODE_720P_1280_720_60fps, cfg_720p_60fps_, SIZEOF_ARRAY(cfg_720p_60fps_)},
		{MODE_1080P_1920_1080_30fps, cfg_1080p_30fps_, SIZEOF_ARRAY(cfg_1080p_30fps_)},
	};

} /* namespace IMX219_cfg */


class IMX219 {
public:
	class HardwareError;

	IMX219(I2C_Client& iic, GPIO_Client& gpio) :
		iic_(iic), gpio_(gpio)
	{
		reset();
		init();
	}

	void init()
	{
		uint8_t id_h, id_l;
		readReg(reg_ID_h_, id_h);
		readReg(reg_ID_l_, id_l);
		if (id_h != dev_ID_h_ || id_l != dev_ID_l_)
		{
			char msg[100];
			snprintf(msg, sizeof(msg), "IMX219: Got %02x %02x. Expected %02x %02x\r\n",
			         id_h, id_l, dev_ID_h_, dev_ID_l_);
			throw HardwareError(HardwareError::WRONG_ID, msg);
		}

		/* Write init sequence */
		for (size_t i = 0; i < SIZEOF_ARRAY(IMX219_cfg::cfg_init_); ++i)
		{
			writeReg(IMX219_cfg::cfg_init_[i].addr, IMX219_cfg::cfg_init_[i].data);
		}
	}

	Errc reset()
	{
		/* Hardware reset via GPIO */
		gpio_.setPin(0, 0); /* CAM_GPIO active low reset */
		for (volatile int i = 0; i < 100000; ++i);
		gpio_.setPin(0, 1);
		for (volatile int i = 0; i < 100000; ++i);
		return OK;
	}

	Errc set_mode(IMX219_cfg::mode_t mode)
	{
		/* Standby */
		writeReg(0x0100, 0x00);

		for (size_t i = 0; i < IMX219_cfg::modes_[mode].cfg_size; ++i)
		{
			writeReg(IMX219_cfg::modes_[mode].cfg[i].addr,
			         IMX219_cfg::modes_[mode].cfg[i].data);
		}

		/* Streaming on */
		writeReg(0x0100, 0x01);
		return OK;
	}

	Errc set_gain(uint8_t analog_gain)
	{
		/* analog_gain: 0~232 (0dB ~ 20.6dB) */
		writeReg(0x0157, analog_gain);
		return OK;
	}

	Errc set_exposure(uint16_t coarse_time)
	{
		writeReg(0x015A, (coarse_time >> 8) & 0xFF);
		writeReg(0x015B, coarse_time & 0xFF);
		return OK;
	}

	class HardwareError : public std::runtime_error {
	public:
		enum errorType { WRONG_ID };
		HardwareError(errorType errType, const char* msg) :
			std::runtime_error(msg), errType_(errType) {}
	private:
		errorType errType_;
	};

private:
	/* IMX219 uses 16-bit register address, 8-bit data */
	Errc readReg(uint16_t addr, uint8_t& data)
	{
		uint8_t buf_addr[2] = {(uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF)};
		uint8_t buf = 0;
		for (unsigned int retries = retry_count_; retries > 0; --retries)
		{
			try {
				iic_.write(dev_address_, buf_addr, 2);
				iic_.read(dev_address_, &buf, 1);
				data = buf;
				return OK;
			} catch (...) {
				if (retries == 1) throw;
			}
		}
		return ERR_GENERAL;
	}

	Errc writeReg(uint16_t addr, uint8_t data)
	{
		uint8_t buf[3] = {(uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF), data};
		for (unsigned int retries = retry_count_; retries > 0; --retries)
		{
			try {
				iic_.write(dev_address_, buf, 3);
				return OK;
			} catch (...) {
				if (retries == 1) throw;
			}
		}
		return ERR_GENERAL;
	}

	I2C_Client& iic_;
	GPIO_Client& gpio_;
	uint8_t dev_address_ = 0x10;         /* IMX219 I2C address */
	uint8_t const dev_ID_h_ = 0x02;      /* Chip ID high = 0x02 */
	uint8_t const dev_ID_l_ = 0x19;      /* Chip ID low  = 0x19 → 0x0219 */
	uint16_t const reg_ID_h_ = 0x0000;   /* Model ID high register */
	uint16_t const reg_ID_l_ = 0x0001;   /* Model ID low register */
	unsigned int const retry_count_ = 10;
};

} /* namespace digilent */

#endif /* IMX219_H_ */
