/*
 * Copyright (C) 2012 Eduardo José Tagle <ejtagle@tutopia.com>
 * Copyright (C) 2010 Google, Inc.
 *
 * Authors:
 *	Colin Cross <ccross@google.com>
 *	Eduardo José Tagle <ejtagle@tutopia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* Tegra2 external memory controller initialization */
 
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>

#include "board-n10.h"
#include "tegra2_emc.h"

// BCT dump...
static const struct tegra_emc_table n10_emc_tables[] = {
	{
		.rate = 333000,   /* SDRAM frequency */
		.regs = {
			0x00000014,   /* Rc */
			0x00000042,   /* Rfc */
			0x0000000f,   /* Ras */
			0x00000005,   /* Rp */
			0x00000004,   /* R2w */
			0x00000005,   /* W2r */
			0x00000003,   /* R2p */
			0x0000000a,   /* W2p */
			0x00000005,   /* RdRcd */
			0x00000005,   /* WrRcd */
			0x00000003,   /* Rrd */
			0x00000001,   /* Rext */
			0x00000003,   /* Wdv */
			0x00000004,   /* QUse */
			0x00000003,   /* QRst */
			0x00000009,   /* QSafe */
			0x0000000c,   /* Rdv */
			0x000009ff,   /* Refresh */
			0x00000000,   /* BurstRefreshNum */
			0x00000003,   /* PdEx2Wr */
			0x00000003,   /* PdEx2Rd */
			0x00000005,   /* PChg2Pden */
			0x00000005,   /* Act2Pden */
			0x00000001,   /* Ar2Pden */
			0x0000000e,   /* Rw2Pden */
			0x000000c8,   /* Txsr */
			0x00000003,   /* Tcke */
			0x0000000d,   /* Tfaw */
			0x00000006,   /* Trpab */
			0x0000000c,   /* TClkStable */
			0x00000002,   /* TClkStop */
			0x00000000,   /* TRefBw */
			0x00000000,   /* QUseExtra */
			0x00000002,   /* FbioCfg6 */
			0x00000000,   /* OdtWrite */
			0x00000000,   /* OdtRead */
			0x00000083,   /* FbioCfg5 */
			0xf0000413,   /* CfgDigDll */
			0x00000010,   /* DllXformDqs */
			0x00000008,   /* DllXformQUse */
			0x00000000,   /* ZcalRefCnt */
			0x00000000,   /* ZcalWaitCnt */
			0x00000000,   /* AutoCalInterval */
			0x00000000,   /* CfgClktrim0 */
			0x00000000,   /* CfgClktrim1 */
			0x00000000    /* CfgClktrim2 */
        }
    }
};

static const struct tegra_emc_chip n10_emc_chips[] = {
	{
		.description = "N10 memory",
		.mem_manufacturer_id = -1,
		.mem_revision_id1 = -1,
		.mem_revision_id2 = -1,
		.mem_pid = -1,
		.table = n10_emc_tables,
		.table_size = ARRAY_SIZE(n10_emc_tables),
	},
};

#if 0
static const struct tegra_emc_chip ventana_siblings_emc_chips[] = {
};  
#endif

void __init n10_init_emc(void)
{

	tegra_init_emc(n10_emc_chips, ARRAY_SIZE(n10_emc_chips));
#if 0	
	pr_info("ventana_emc_init: using ventana_siblings_emc_chips\n");
		tegra_init_emc(ventana_siblings_emc_chips,
			       ARRAY_SIZE(ventana_siblings_emc_chips)); 
#endif
}
