// SPDX-License-Identifier: GPL-2.0
/* Read-only probe for the STM32MP157 M4 resource-table vdev status word. */
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>

#define RSC_VDEV_STATUS_PHYS 0x10020154UL
#define VRING0_PHYS          0x10040000UL
#define VRING0_AVAIL_IDX_OFF 0x102UL
#define VRING0_USED_IDX_OFF  0x132UL

static int __init rsc_status_probe_init(void)
{
	void __iomem *reg;
	u32 value;

	reg = ioremap(RSC_VDEV_STATUS_PHYS, sizeof(value));
	if (!reg) {
		pr_err("rsc_status_probe: ioremap failed for 0x%08lx\n",
		       RSC_VDEV_STATUS_PHYS);
		return -ENOMEM;
	}

	value = readl(reg);
	pr_info("rsc_status_probe: phys 0x%08lx = 0x%08x "
		"(status=0x%02x num_vrings=%u)\n",
		RSC_VDEV_STATUS_PHYS, value, value & 0xff,
		(value >> 8) & 0xff);
	iounmap(reg);

	reg = ioremap(VRING0_PHYS, 0x1000);
	if (!reg) {
		pr_err("rsc_status_probe: ioremap failed for vring0 0x%08lx\n",
		       VRING0_PHYS);
		return -ENOMEM;
	}

	pr_info("rsc_status_probe: vring0 desc0=0x%08x%08x "
		"avail.idx=%u used.idx=%u\n",
		readl(reg + 4), readl(reg),
		readw(reg + VRING0_AVAIL_IDX_OFF),
		readw(reg + VRING0_USED_IDX_OFF));
	iounmap(reg);
	return 0;
}

static void __exit rsc_status_probe_exit(void)
{
	pr_info("rsc_status_probe: unloaded\n");
}

module_init(rsc_status_probe_init);
module_exit(rsc_status_probe_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Read-only STM32MP157 resource-table and vring probe");
