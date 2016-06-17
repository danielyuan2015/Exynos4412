/*
 * (C) Copyright 2011 Samsung Electronics Co. Ltd
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
 
#include <common.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gpio.h>
#include "exynos4412-clk.h"
#include "exynos4412-fb.h"

unsigned int OmPin;

DECLARE_GLOBAL_DATA_PTR;
extern int nr_dram_banks;
unsigned int second_boot_info = 0xffffffff;

/* ------------------------------------------------------------------------- */
#define SMC9115_Tacs	(0x0)	// 0clk		address set-up
#define SMC9115_Tcos	(0x4)	// 4clk		chip selection set-up
#define SMC9115_Tacc	(0xe)	// 14clk	access cycle
#define SMC9115_Tcoh	(0x1)	// 1clk		chip selection hold
#define SMC9115_Tah	(0x4)	// 4clk		address holding time
#define SMC9115_Tacp	(0x6)	// 6clk		page mode access cycle
#define SMC9115_PMC	(0x0)	// normal(1data)page mode configuration

#define SROM_DATA16_WIDTH(x)	(1<<((x*4)+0))
#define SROM_WAIT_ENABLE(x)	(1<<((x*4)+1))
#define SROM_BYTE_ENABLE(x)	(1<<((x*4)+2))

/*
 * Miscellaneous platform dependent initialisations
 */
static void smc9115_pre_init(void)
{
        unsigned int cs1;
	/* gpio configuration */
	writel(0x00220020, 0x11000000 + 0x120);
	writel(0x00002222, 0x11000000 + 0x140);

	/* 16 Bit bus width */
	writel(0x22222222, 0x11000000 + 0x180);
	writel(0x0000FFFF, 0x11000000 + 0x188);
	writel(0x22222222, 0x11000000 + 0x1C0);
	writel(0x0000FFFF, 0x11000000 + 0x1C8);
	writel(0x22222222, 0x11000000 + 0x1E0);
	writel(0x0000FFFF, 0x11000000 + 0x1E8);

	/* SROM BANK1 */
        cs1 = SROM_BW_REG & ~(0xF<<4);
	cs1 |= ((1 << 0) |
		(0 << 2) |
		(1 << 3)) << 4;                

        SROM_BW_REG = cs1;

	/* set timing for nCS1 suitable for ethernet chip */
	SROM_BC1_REG = ( (0x1 << 0) |
		     (0x9 << 4) |
		     (0xc << 8) |
		     (0x1 << 12) |
		     (0x6 << 16) |
		     (0x1 << 24) |
		     (0x1 << 28) );
}


//#define S3C_ADDR_BASE	0xF6000000
//#ifndef haha//__ASSEMBLY__
//#define S3C_ADDR(x)	((void __iomem __force *)S3C_ADDR_BASE + (x))
//#else
//#define S3C_ADDR(x)	(S3C_ADDR_BASE + (x))
//#endif

#define S5P_VA_GPIO2		0x11000000//S3C_ADDR(0x02240000)
//#define S5P_VA_GPIO3		S3C_ADDR(0x02280000)
//#define S5P_VA_GPIO4		S3C_ADDR(0x022C0000)

#define smdk4x12_MP00CON	(S5P_VA_GPIO2 + 0x0120) //GPY0CON
#define smdk4x12_MP01CON	(S5P_VA_GPIO2 + 0x0140) //GPY1CON
#define smdk4x12_MP02CON	(S5P_VA_GPIO2 + 0x0160) //GPY2CON
#define smdk4x12_MP03CON	(S5P_VA_GPIO2 + 0x0180) //GPY3CON
#define smdk4x12_MP04CON	(S5P_VA_GPIO2 + 0x01A0) //GPY4CON
#define smdk4x12_MP05CON	(S5P_VA_GPIO2 + 0x01C0) //GPY5CON
#define smdk4x12_MP06CON	(S5P_VA_GPIO2 + 0x01E0) //GPY6CON

#define S5P_VA_SROMC		0x12570000//S3C_ADDR(0x024C0000)

#define S5P_SROMREG(x)		(S5P_VA_SROMC + (x))
#define S5P_SROM_BC3		S5P_SROMREG(0x10)

/* applies to same to BCS0 - BCS3 */
#define S5P_SROM_BCX__PMC__SHIFT		0
#define S5P_SROM_BCX__TACP__SHIFT		4
#define S5P_SROM_BCX__TCAH__SHIFT		8
#define S5P_SROM_BCX__TCOH__SHIFT		12
#define S5P_SROM_BCX__TACC__SHIFT		16
#define S5P_SROM_BCX__TCOS__SHIFT		24
#define S5P_SROM_BCX__TACS__SHIFT		28

//#define S5P_SROMREG(x)		(S5P_VA_SROMC + (x))
#define S5P_SROM_BW		S5P_SROMREG(0x0)

#define S5P_SROM_BW__CS_MASK			0xf

#define S5P_SROM_BW__DATAWIDTH__SHIFT		0
#define S5P_SROM_BW__ADDRMODE__SHIFT		1
#define S5P_SROM_BW__WAITENABLE__SHIFT		2
#define S5P_SROM_BW__BYTEENABLE__SHIFT		3
#define S5P_SROM_BW__NCS3__SHIFT		12

static void dm9000_pre_init(void)
{
    unsigned int cs1,cs3;
	u32 tmp;
	/* gpio configuration */
	//writel(0x00220020, 0x11000000 + 0x120);
	//writel(0x00002222, 0x11000000 + 0x140);
	//CS3, OEn(Xm0OEn), WEn(Xm0WEn)
	tmp = __raw_readl(smdk4x12_MP00CON);
	tmp &= ~((0xf << 12)|(0xf << 16)|(0xf << 20));
	tmp |= ((2 << 12)|(2 << 16)|(2 << 20));
	__raw_writel(tmp, smdk4x12_MP00CON);
	__raw_writel(0x00002222, smdk4x12_MP01CON);
	

	/* 16 Bit bus width */
	//writel(0x22222222, 0x11000000 + 0x180);
	//writel(0x0000FFFF, 0x11000000 + 0x188);
	//writel(0x22222222, 0x11000000 + 0x1C0);
	//writel(0x0000FFFF, 0x11000000 + 0x1C8);
	//writel(0x22222222, 0x11000000 + 0x1E0);
	//writel(0x0000FFFF, 0x11000000 + 0x1E8);
	
	/*DATA0~DATA15*/
	__raw_writel(0x22222222, smdk4x12_MP03CON);
	__raw_writel(0x0000FFFF, smdk4x12_MP03CON + 8);
	__raw_writel(0x22222222, smdk4x12_MP05CON);
	__raw_writel(0x0000FFFF, smdk4x12_MP05CON + 8);
	__raw_writel(0x22222222, smdk4x12_MP06CON);
	__raw_writel(0x0000FFFF, smdk4x12_MP06CON + 8);

	/* SROM BANK1 */
  /*      cs1 = SROM_BW_REG & ~(0xF<<4);
	cs1 |= ((1 << 0) |
		(0 << 2) |
		(1 << 3)) << 4;                

        SROM_BW_REG = cs1;*/
        
		/* configure nCS3 width to 16 bits */	
		cs3 = __raw_readl(S5P_SROM_BW) &
			~(S5P_SROM_BW__CS_MASK << S5P_SROM_BW__NCS3__SHIFT);
		cs3 |= ((1 << S5P_SROM_BW__DATAWIDTH__SHIFT) |
			(1 << S5P_SROM_BW__ADDRMODE__SHIFT) |
			(1 << S5P_SROM_BW__WAITENABLE__SHIFT) |
			(1 << S5P_SROM_BW__BYTEENABLE__SHIFT)) <<
			S5P_SROM_BW__NCS3__SHIFT;
		__raw_writel(cs3, S5P_SROM_BW);

	/* set timing for nCS1 suitable for ethernet chip */
	/*SROM_BC1_REG = ( (0x1 << 0) |
		     (0x9 << 4) |
		     (0xc << 8) |
		     (0x1 << 12) |
		     (0x6 << 16) |
		     (0x1 << 24) |
		     (0x1 << 28) );*/
		     
	/* set timing for nCS3 suitable for ethernet chip */
	__raw_writel((0x1 << S5P_SROM_BCX__PMC__SHIFT) |
		     (0x9 << S5P_SROM_BCX__TACP__SHIFT) |
		     (0xc << S5P_SROM_BCX__TCAH__SHIFT) |
		     (0x1 << S5P_SROM_BCX__TCOH__SHIFT) |
		     (0x6 << S5P_SROM_BCX__TACC__SHIFT) |
		     (0x1 << S5P_SROM_BCX__TCOS__SHIFT) |
		     (0x1 << S5P_SROM_BCX__TACS__SHIFT), S5P_SROM_BC3);		     
}

int board_init(void)
{
	char bl1_version[9] = {0};

	/* display BL1 version */
#ifdef CONFIG_TRUSTZONE
	printf("TrustZone Enabled BSP\n");
	strncpy(&bl1_version[0], (char *)0x0204f810, 8);
#else
	strncpy(&bl1_version[0], (char *)0x020233c8, 8);
#endif
	printf("BL1 version: %s\n", &bl1_version[0]);

	/* check half synchronizer for asynchronous bridge */
	if(*(unsigned int *)(0x10010350) == 0x1)
		printf("Using half synchronizer for asynchronous bridge\n");
	
#ifdef CONFIG_SMC911X
	smc9115_pre_init();
#endif
#ifdef CONFIG_DRIVER_DM9000
	dm9000_pre_init();
#endif

#ifdef CONFIG_SMDKC220
	gd->bd->bi_arch_number = MACH_TYPE_C220;
#else
	if(((PRO_ID & 0x300) >> 8) == 2)
		gd->bd->bi_arch_number = MACH_TYPE_C210;
	else
		gd->bd->bi_arch_number = MACH_TYPE_V310;
#endif

	gd->bd->bi_boot_params = (PHYS_SDRAM_1+0x100);

   	OmPin = INF_REG3_REG;
	printf("\n\nChecking Boot Mode ...");
	if(OmPin == BOOT_ONENAND) {
		printf(" OneNand\n");
	} else if (OmPin == BOOT_NAND) {
		printf(" NAND\n");
	} else if (OmPin == BOOT_MMCSD) {
		printf(" SDMMC\n");
	} else if (OmPin == BOOT_EMMC) {
		printf(" EMMC4.3\n");
	} else if (OmPin == BOOT_EMMC_4_4) {
		printf(" EMMC4.41\n");
	}

	return 0;
}

int dram_init(void)
{
	//gd->ram_size = get_ram_size((long *)PHYS_SDRAM_1, PHYS_SDRAM_1_SIZE);
	
	return 0;
}

void dram_init_banksize(void)
{
		nr_dram_banks = CONFIG_NR_DRAM_BANKS;
		gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
		gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;
		gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
		gd->bd->bi_dram[1].size = PHYS_SDRAM_2_SIZE;
		gd->bd->bi_dram[2].start = PHYS_SDRAM_3;
		gd->bd->bi_dram[2].size = PHYS_SDRAM_3_SIZE;
		gd->bd->bi_dram[3].start = PHYS_SDRAM_4;
		gd->bd->bi_dram[3].size = PHYS_SDRAM_4_SIZE;
#ifdef USE_2G_DRAM
		gd->bd->bi_dram[4].start = PHYS_SDRAM_5;
		gd->bd->bi_dram[4].size = PHYS_SDRAM_5_SIZE;
		gd->bd->bi_dram[5].start = PHYS_SDRAM_6;
		gd->bd->bi_dram[5].size = PHYS_SDRAM_6_SIZE;
		gd->bd->bi_dram[6].start = PHYS_SDRAM_7;
		gd->bd->bi_dram[6].size = PHYS_SDRAM_7_SIZE;
		gd->bd->bi_dram[7].start = PHYS_SDRAM_8;
		gd->bd->bi_dram[7].size = PHYS_SDRAM_8_SIZE;
#endif

#ifdef CONFIG_TRUSTZONE
	gd->bd->bi_dram[nr_dram_banks - 1].size -= CONFIG_TRUSTZONE_RESERVED_DRAM;
#endif
}

int board_eth_init(bd_t *bis)
{
	int rc = 0;
#ifdef CONFIG_SMC911X
	//rc = smc911x_initialize(0, CONFIG_SMC911X_BASE);
#endif
#ifdef CONFIG_DRIVER_DM9000
    rc = dm9000_initialize(bis);  
#endif   
	return rc;
}

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	printf("Board:\tSMDKV310\n");
	
	return 0;

}
#endif

void x4412_framebuffer_init(void);
int force_update_flag = 0;
int board_late_init (void)
{
	x4412_framebuffer_init();
	
	GPIO_Init();
	GPIO_SetFunctionEach(eGPIO_X1, eGPIO_0, 0);
	GPIO_SetPullUpDownEach(eGPIO_X1, eGPIO_0, 0);

	udelay(10);
	if (GPIO_GetDataEach(eGPIO_X1, eGPIO_0) == 0)
	{
		printf("[LEFT KEY] pressed, force update\r\n");
		force_update_flag = 1;
	}
	return 0;
}

int board_mmc_init(bd_t *bis)
{
#ifdef CONFIG_S3C_HSMMC
	setup_hsmmc_clock();
	setup_hsmmc_cfg_gpio();
	if (OmPin == BOOT_EMMC_4_4 || OmPin == BOOT_EMMC) {
		smdk_s5p_mshc_init();
		smdk_s3c_hsmmc_init();
	} else {
		smdk_s5p_mshc_init();
		smdk_s3c_hsmmc_init();
	}
#endif
	return 0;
}

#ifdef CONFIG_ENABLE_MMU
ulong virt_to_phy_s5pv310(ulong addr)
{
	if ((0xc0000000 <= addr) && (addr < 0xe0000000))
		return (addr - 0xc0000000 + 0x40000000);

	return addr;
}
#endif

void x4412_set_mpll(unsigned int mdiv, unsigned int pdiv, unsigned int sdiv)
{
	unsigned int mpll_con0 = (1<<31 | mdiv<<16 | pdiv<<8 | sdiv);

	writel(mpll_con0, 0x10040000 + 0x0108);
	while((readl(0x10040000 + 0x0108) & (0x1 << 29)) != (0x1 << 29));
}

void x4412_framebuffer_init(void)
{
	char *commandline = getenv ("bootargs");
	
	exynos4412_clk_initial();
	
	/* FOUT = (MDIV X FIN) / (PDIV X 2SDIV) */
	if(strstr(commandline, "lcd=vga-1024x768"))
	{
		/* M = 780MHZ */
		x4412_set_mpll(195, 6, 0);
		exynos4412_fix_fimd(780 * 1000 * 1000);
	}
	else if(strstr(commandline, "lcd=vga-1440x900"))
	{
		/* M = 710MHZ */
		//x4412_set_mpll(355, 6, 1);
		//exynos4412_fix_fimd(710 * 1000 * 1000);

		/* M = 800MHZ */
		x4412_set_mpll(100, 3, 0);
		exynos4412_fix_fimd(800 * 1000 * 1000);
	}
	else if(strstr(commandline, "lcd=vga-1280x1024"))
	{
		/* M = 756MHZ */
		//x4412_set_mpll(252, 8, 0);
		//exynos4412_fix_fimd(756 * 1000 * 1000);

		/* M = 800MHZ */
		x4412_set_mpll(100, 3, 0);
		exynos4412_fix_fimd(800 * 1000 * 1000);
	}
	else
	{
		/* M = 800MHZ */
		exynos4412_fix_fimd(800 * 1000 * 1000);
	}
	exynos4412_fb_initial(commandline);
}
