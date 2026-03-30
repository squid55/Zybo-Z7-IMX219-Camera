#include "xclk_wiz.h"

XClk_Wiz_Config XClk_Wiz_ConfigTable[] __attribute__ ((section (".drvcfg_sec"))) = {

	{
		"xlnx,clk-wiz-6.0", /* compatible */
		0x43c00000, /* reg */
		0x64, /* xlnx,prim-in-freq */
		0xffff, /* interrupts */
		0xffff /* interrupt-parent */
	},
	 {
		 NULL
	}
};