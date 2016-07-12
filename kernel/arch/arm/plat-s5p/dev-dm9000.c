#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <mach/map.h>
#include <plat/mipi_csis.h>
#include <linux/dm9000.h>

static struct resource s5p_dm9000_resources[] = {
	[0] = {
		.start = EXYNOS4_PA_SROM_BANK(3),
		.end   = EXYNOS4_PA_SROM_BANK(3) + 3,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
#if defined(CONFIG_DM9000_16BIT)
		.start = EXYNOS4_PA_SROM_BANK(3) + 4,
		.end   = EXYNOS4_PA_SROM_BANK(3) + 7,
		.flags = IORESOURCE_MEM,
#else
		.start = EXYNOS4_PA_SROM_BANK(3) + 1,
		.end   = EXYNOS4_PA_SROM_BANK(3) + 1,
		.flags = IORESOURCE_MEM,
#endif
	},
	[2] = {
		.start = IRQ_EINT(21),
		.end   = IRQ_EINT(21),
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	}
};

static struct dm9000_plat_data s5p_dm9000_platdata = {
#if defined(CONFIG_DM9000_16BIT)
	.flags = DM9000_PLATF_16BITONLY | DM9000_PLATF_NO_EEPROM,
#else
	.flags = DM9000_PLATF_8BITONLY | DM9000_PLATF_NO_EEPROM,
#endif
	.dev_addr = {0x00, 0x09, 0xc0, 0xff, 0xec, 0x48},
};

struct platform_device s5p_device_dm9000 = {
	.name = "dm9000",
	.id = 0,
	.num_resources = ARRAY_SIZE(s5p_dm9000_resources),
	.resource = s5p_dm9000_resources,
	.dev = {
		.platform_data = &s5p_dm9000_platdata,
	}
};
EXPORT_SYMBOL(s5p_device_dm9000);

unsigned int MACADDR[6] = {0x00, 0x09, 0xc0, 0xff, 0xec, 0x48};
EXPORT_SYMBOL(MACADDR);

static int __init mac_setup(char * str)
{
	unsigned int tmp[6];

    if ((str != NULL) && (*str != '\0'))
    {
    	if(sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x", &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]) == 6)
    	{
			MACADDR[0] = tmp[0] & 0xff;
			MACADDR[1] = tmp[1] & 0xff;
			MACADDR[2] = tmp[2] & 0xff;
			MACADDR[3] = tmp[3] & 0xff;
			MACADDR[4] = tmp[4] & 0xff;
			MACADDR[5] = tmp[5] & 0xff;

			s5p_dm9000_platdata.dev_addr[0] = MACADDR[0];
			s5p_dm9000_platdata.dev_addr[1] = MACADDR[1];
			s5p_dm9000_platdata.dev_addr[2] = MACADDR[2];
			s5p_dm9000_platdata.dev_addr[3] = MACADDR[3];
			s5p_dm9000_platdata.dev_addr[4] = MACADDR[4];
			s5p_dm9000_platdata.dev_addr[5] = MACADDR[5];
    	}
	}
	else
	{
		s5p_dm9000_platdata.dev_addr[0] = 0x00;
		s5p_dm9000_platdata.dev_addr[1] = 0x09;
		s5p_dm9000_platdata.dev_addr[2] = 0xc0;
		s5p_dm9000_platdata.dev_addr[3] = 0xff;
		s5p_dm9000_platdata.dev_addr[4] = 0xec;
		s5p_dm9000_platdata.dev_addr[5] = 0x48;
	}

	printk("DM9000 using MAC address: %pM\n", s5p_dm9000_platdata.dev_addr);
	return 1;
}
__setup("mac=", mac_setup);

