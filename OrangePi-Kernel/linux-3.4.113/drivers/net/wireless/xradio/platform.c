/*
 * Platform interfaces for XRadio drivers
 * 
 * Implemented by platform vendor(such as AllwinnerTech).
 *
 * Copyright (c) 2013, XRadio
 * Author: XRadio
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/delay.h>

#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ioport.h>

#include <linux/regulator/consumer.h>
#include <asm/mach-types.h>
#include <mach/sys_config.h>

#include "xradio.h"
#include "platform.h"
#include "sbus.h"

#define PLATFORM_RFKILL_PM     1  //if 1, Use system rf-kill for wifi power manage, default 0.
#define PLATFORM_SYSCONFIG     1  //if 1, Get Hardware settting from sysconfig.fex, default 0.
#define PMU_POWER_WLAN_RETAIN  1  //if 1, PMU power still up when wifi is closed, default 0.

//hardware platform config.
static int wlan_bus_id     = 0;
static u32 wlan_irq_gpio   = 0;
static u32 wlan_reset_gpio = 0;

#if (PLATFORM_SYSCONFIG)
#define WIFI_CONFIG        "wifi_para"
#define MAX_POWER_NUM      3
static char * axp_name[MAX_POWER_NUM]  = {0};
#endif

//--------------------------------------------------
//Select one platform if PLATFORM_RFKILL_PM=0.
//And you can platforms too.
//--------------------------------------------------
//#define  V819MINI
//#define  HP8_G2
//#define  EVB_A31_CARD0
//#define  EVB_A31_CARD1
//#define  EVB_A83T_ONBOARD
#define  EVB_A33_XR819
//#define  EVB_A33W_XR819
//#define  A33_CHIPHD
//#define  A33_YHK
//#define    A33W_SUNFLOWER_S1

#if (!PLATFORM_SYSCONFIG)
//--------------------------------------------------
// sdio and gpio settings of platforms 
// use following settings if PLATFORM_SYSCONFIG=0
// You can add configs for other platforms
//--------------------------------------------------
//V819MINI
#ifdef   V819MINI
#define  PLAT_GPIO_WRESET    (GPIOM(7))
#define  PLAT_GPIO_WLAN_INT  (GPIOL(6))
#define  PLAT_MMC_ID		  1

//HP8 G2
#elif defined(HP8_G2)
#define  PLAT_GPIO_WRESET    (GPIOL(6))
#define  PLAT_GPIO_WLAN_INT  (GPIOL(7))
#define  PLAT_MMC_ID          1

//EVB_A83T_ONBOARD
#elif defined(EVB_A83T_ONBOARD)
#define  PLAT_GPIO_WRESET    (GPIOL(9))
#define  PLAT_GPIO_WLAN_INT  (GPIOL(11))
#define  PLAT_MMC_ID          1

//EVB_A33_XR819_test
#elif defined(EVB_A33_XR819)
#define  PLAT_GPIO_WRESET    (GPIOL(10))
#define  PLAT_GPIO_WLAN_INT  (GPIOL(8))
#define  PLAT_MMC_ID          1

//EVB_A33W_XR819_test
#elif defined(EVB_A33W_XR819)
#define  PLAT_GPIO_WRESET    (GPIOL(6))
#define  PLAT_GPIO_WLAN_INT  (GPIOL(7))
#define  PLAT_MMC_ID          1

//A33W_SUNFLOWER_S1
#elif defined(A33W_SUNFLOWER_S1)
#define  PLAT_GPIO_WRESET    (GPIOL(8))
#define  PLAT_GPIO_WLAN_INT  (GPIOL(9))
#define  PLAT_MMC_ID          1

//A33_CHIPHD
#elif defined(A33_CHIPHD)
#define  PLAT_GPIO_WRESET    (GPIOL(6))
#define  PLAT_GPIO_WLAN_INT  (GPIOL(7))
#define  PLAT_MMC_ID          1

//A33_YHK
#elif defined(A33_YHK)
#define  PLAT_GPIO_WRESET    (GPIOL(5))
#define  PLAT_GPIO_WLAN_INT  (GPIOL(7))
#define  PLAT_MMC_ID          1

//defualt EVB_A31_CARD0.
#elif defined(EVB_A31_CARD0)
#define  PLAT_GPIO_WRESET    (GPIOM(4))
#define  PLAT_GPIO_WLAN_INT  (GPIOM(6))
#define  PLAT_MMC_ID          0

#else  //defualt EVB_A31_CARD1.
#define  PLAT_GPIO_WRESET    (GPIOM(4))
#define  PLAT_GPIO_WLAN_INT  (GPIOM(6))
#define  PLAT_MMC_ID          1

#endif //end of V819MINI

#endif //PLATFORM_SYSCONFIG

/*********************Interfaces on Allwinner platform. *********************/
#if (!PLATFORM_RFKILL_PM)

#if (defined(EVB_A31_CARD0)||defined(EVB_A31_CARD1))
static int plat_evb_a31_pwr(int on)
{
	struct regulator *ldo = NULL;
	unsigned int	clk;
	int val;
	int ret = 0;
	
	if (!on) return ret; //no need power down on evb_a31.

#ifdef EVB_A31_CARD0  
	printk(KERN_ERR "evb card0 (red card) present!.\n");
  //set sd card 0 to 1.8v
	ldo = regulator_get(NULL, "axp22_eldo2");	
	if(ldo) {
		//set new voltage 1.8v
		val = 1800000;
		if(regulator_set_voltage(ldo, val, val) < 0)
			printk(KERN_ERR "regulator_set_voltage failed.\n");
		//enable 	regulator
		ret = regulator_enable(ldo);
		if(ret < 0){
			printk(KERN_ERR "regulator_enable failed.\n");
		}
		//check voltage
		val = regulator_get_voltage(ldo);
		printk(KERN_ERR "sdio_vol(eldo2)=%duV.\n", val);
	} else {
		printk(KERN_ERR "regulator get eldo2 failed.\n");
	}
	regulator_put(ldo);
	
	//check pm_io voltage
	ldo = regulator_get(NULL, "axp22_aldo2");	
	if(ldo) {
		//try to set 1.8v to pm voltage
		val = 1800000;
		regulator_set_voltage(ldo, val, val);
		regulator_enable(ldo);
		
		val = regulator_get_voltage(ldo);
		printk(KERN_ERR "pm_io_vol(aldo2)=%duV.\n", val);
	} else {
		printk(KERN_ERR "regulator get aldo2 failed.\n");
	}
	regulator_put(ldo);
	
#else
	printk(KERN_ERR "evb card1 (green card) present!.\n");
		
	//check pm_io voltage
	ldo = regulator_get(NULL, "axp22_aldo2");	
	if(ldo) {
		//try to set 1.8v to pm voltage
		val = 1800000;
		regulator_set_voltage(ldo, val, val);
		regulator_enable(ldo);
		
		val = regulator_get_voltage(ldo);
		printk(KERN_ERR "pm_io_vol(aldo2)=%duV.\n", val);
	} else {
		printk(KERN_ERR "regulator get aldo2 failed.\n");
	}
	regulator_put(ldo);
	
		/* pm7Ƥ׃Ϊrtc_clk0 */
	clk = ioread32(0xf1f02c24);
	clk &= ~(0x3 << 28);
	clk |= 0x3 << 28;
	iowrite32(clk, 0xf1f02c24);

	/* loscƤԉexternal 32.768KHz */
	clk = ioread32(0xf1f00004);
	clk &= ~0x1;
	clk |= 0x1;
	iowrite32(clk, 0xf1f00004);

	/* clk_outputdʹŜ */
	clk = ioread32(0xf1f014f0);
	clk = 0x80000000;
	iowrite32(clk, 0xf1f014f0);

	printk(KERN_ERR "[clock] clock init RTC-CKO 32768Hz," \
	           " 0xf1f02c24=%#x, 0xf1f00004=%#x, 0xf1f014f0=%#x\n",
	           ioread32(0xf1f02c24), ioread32(0xf1f00004), ioread32(0xf1f014f0));

	gpio_request(GPIOM(3), "pmu_en");
	gpio_direction_output(GPIOM(3), 0);
	gpio_set_value(GPIOM(3), 1);
	udelay(300);
	
	gpio_request(GPIOG(12), "gbf_ena_reset");
	gpio_direction_output(GPIOG(12), 0);
	gpio_set_value(GPIOG(12), 1);
	udelay(300);
	
	gpio_free(GPIOM(3));
	gpio_free(GPIOG(12));
#endif //EVB_CARD0

	return ret;
}
#endif

#ifdef   V819MINI
static int plat_v819mini_pwr(int on)
{
	struct regulator *ldo = NULL;
	int ret = 0;

	xradio_dbg(XRADIO_DBG_ERROR, "V819mini Power %s!.\n", on? "Up":"Down");
	
	if (on) {
		//wifi vcc 3.3v for XR819
		ldo = regulator_get(NULL, "axp22_dldo1");
		if (ldo) {
			regulator_set_voltage(ldo, 3300000, 3300000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable dldo1=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
		//pg sdio pin 3.3v for XR819
		ldo = regulator_get(NULL, "axp22_dldo2");
		if (ldo) {
			regulator_set_voltage(ldo, 3300000, 3300000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable dldo2=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
		
		//wifi reset 3.3v for XR819
		ldo = regulator_get(NULL, "axp22_dldo4");
		if (ldo) {
			regulator_set_voltage(ldo, 3300000, 3300000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable dldo4=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);
		}
	} else {
		ldo = regulator_get(NULL, "axp22_dldo1");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
	}

  return ret;
}
#endif

#if defined(HP8_G2)
static int plat_hp8_g2_pwr(int on)
{
	struct regulator *ldo = NULL;
	int ret = 0;

	printk(KERN_ERR "hp8_g2 Power %s!.\n", on? "Up":"Down");

	if (on) {
		//wifi vcc 3.3v for XR819.
		ldo = regulator_get(NULL, "axp22_dldo1");
		if (ldo) {
			regulator_set_voltage(ldo, 3300000, 3300000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable dldo1=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);
		}
	
		//wifi vcc 1.8v for XR819.
		ldo = regulator_get(NULL, "axp22_dldo2");
		if (ldo) {
			regulator_set_voltage(ldo, 1800000, 1800000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable dldo2=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);
		}
		ldo = regulator_get(NULL, "axp22_eldo2");
		if (ldo) {
			regulator_set_voltage(ldo, 1800000, 1800000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable eldo2=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);
		}
	
		//wifi io pin 1.8~3.3v for XR819.
		ldo = regulator_get(NULL, "axp22_aldo1");
		if (ldo) {
			regulator_set_voltage(ldo, 3000000, 3000000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable aldo2=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);
		}
		//wifi reset & interrupt 1.8~3.3v for XR819.
	} else {
		ldo = regulator_get(NULL, "axp22_dldo1");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
		ldo = regulator_get(NULL, "axp22_dldo2");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
		ldo = regulator_get(NULL, "axp22_eldo2");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
	}

	return ret;
}
#endif

#if defined(EVB_A83T_ONBOARD)
static int plat_evb_a83t_onboard_pwr(int on)
{
	struct regulator *ldo = NULL;
	int ret = 0;
	printk(KERN_ERR "evb_a83t_onboard Power %s!.\n", on? "Up":"Down");
	if (on) {
		//wifi vcc 3.3v for XR819.
		ldo = regulator_get(NULL, "axp81x_dldo2");
		if (ldo) {
			regulator_set_voltage(ldo, 3300000, 3300000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable dldo2=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
	
		//wifi vcc 1.8v for XR819.
		ldo = regulator_get(NULL, "axp81x_aldo1");
		if (ldo) {
			regulator_set_voltage(ldo, 1800000, 1800000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable aldo1=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
	
		//wifi io pin 1.8~3.3v for XR819b.
		ldo = regulator_get(NULL, "axp81x_aldo2");
		if (ldo) {
			regulator_set_voltage(ldo, 3000000, 3000000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable aldo2=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
		
		//wifi reset & interrupt 1.8~3.3v for XR819.
		ldo = regulator_get(NULL, "axp81x_aldo3");
		if (ldo) {
			regulator_set_voltage(ldo, 3000000, 3000000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable aldo3=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);
		}
	} else {
		ldo = regulator_get(NULL, "axp81x_dldo2");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
		ldo = regulator_get(NULL, "axp81x_aldo1");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
	}

	return ret;
}
#endif

#if (defined(EVB_A33_XR819) || defined(EVB_A33W_XR819))
static int plat_a33_xr819_pwr(int on)
{
	struct regulator *ldo = NULL;
	int ret = 0;

	printk(KERN_ERR "plat_a33_xr819_pwr Power %s!.\n", on? "Up":"Down");
	if (on) {
		//wifi vcc 3.3v for XR819.
		ldo = regulator_get(NULL, "axp22_dldo1");
		if (ldo) {
			regulator_set_voltage(ldo, 3300000, 3300000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable dldo1=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
		ldo = regulator_get(NULL, "axp22_dldo2");
		if (ldo) {
			regulator_set_voltage(ldo, 3300000, 3300000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable dldo2=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
		ldo = regulator_get(NULL, "axp22_dldo4");
		if (ldo) {
			regulator_set_voltage(ldo, 3300000, 3300000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable dldo4=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
	
		//wifi vcc 1.8v for XR819.
		ldo = regulator_get(NULL, "axp22_eldo2");
		if (ldo) {
			regulator_set_voltage(ldo, 1800000, 1800000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable eldo2=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
		ldo = regulator_get(NULL, "axp22_eldo3");
		if (ldo) {
			regulator_set_voltage(ldo, 1800000, 1800000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable eldo3=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
		//wifi reset & interrupt 1.8~3.3v for XR819.
	} else {
		//wifi vcc 3.3v
		ldo = regulator_get(NULL, "axp22_dldo1");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
		ldo = regulator_get(NULL, "axp22_dldo2");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
		ldo = regulator_get(NULL, "axp22_dldo4");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
		//wifi vcc 1.8v
		ldo = regulator_get(NULL, "axp22_eldo2");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
		ldo = regulator_get(NULL, "axp22_eldo3");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
	}
	return ret;
}
#endif

#if (defined(A33W_SUNFLOWER_S1))
static int plat_a33w_sunflower_s1_pwr(int on)
{
	struct regulator *ldo = NULL;
	int ret = 0;
	struct clk *ap_32k = NULL;

	printk(KERN_ERR "plat_a33w_sunflower_s1_pwr Power %s!.\n", on? "Up":"Down");
	if (on) {
		//wifi vcc 3.3v for XR819.
		ldo = regulator_get(NULL, "axp22_dldo1");
		if (ldo) {
			regulator_set_voltage(ldo, 3300000, 3300000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable dldo1=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);
		}
		//wifi vcc 1.8v for XR819.
		ldo = regulator_get(NULL, "axp22_eldo1");
		if (ldo) {
			regulator_set_voltage(ldo, 1800000, 1800000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable eldo1=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
	} else {
		//wifi vcc 3.3v
		ldo = regulator_get(NULL, "axp22_dldo1");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
		//wifi vcc 1.8v
		ldo = regulator_get(NULL, "axp22_eldo1");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
	}
	//use external 32k clock.
	ap_32k = clk_get(NULL, "losc_out");
	if (!ap_32k){
		printk(KERN_ERR "Get ap 32k clk out failed!\n");
		return -1;
	}
	ret = clk_prepare_enable(ap_32k);
	if (ret){
		printk(KERN_ERR "losc out enable failed!\n");
	}
	return ret;
}
#endif

#if (defined(A33_CHIPHD) || defined(A33_YHK))
static int plat_a33_chiphd_pwr(int on)
{
	struct regulator *ldo = NULL;
	int ret = 0;
	struct clk *ap_32k = NULL;

	printk(KERN_ERR "plat_a33_chiphd_pwr Power %s!.\n", on? "Up":"Down");
	if (on) {
		//wifi vcc 3.3v for XR819.
		ldo = regulator_get(NULL, "axp22_dldo1");
		if (ldo) {
			regulator_set_voltage(ldo, 3300000, 3300000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable dldo1=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
		ldo = regulator_get(NULL, "axp22_dldo2");
		if (ldo) {
			regulator_set_voltage(ldo, 3300000, 3300000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable dldo2=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}

		//wifi vcc 1.8v for XR819.
		ldo = regulator_get(NULL, "axp22_eldo1");
		if (ldo) {
			regulator_set_voltage(ldo, 1800000, 1800000);
			ret = regulator_enable(ldo);
			if(ret < 0){
				printk(KERN_ERR "regulator_enable failed.\n");
			}
			printk(KERN_ERR "regulator_enable eldo1=%d.\n", regulator_get_voltage(ldo));
			regulator_put(ldo);        
		}
		//wifi reset & interrupt 1.8~3.3v for XR819.
	} else {
		//wifi vcc 3.3v
		ldo = regulator_get(NULL, "axp22_dldo1");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
		ldo = regulator_get(NULL, "axp22_dldo2");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
		//wifi vcc 1.8v
		ldo = regulator_get(NULL, "axp22_eldo1");
		if (ldo) {
			ret = regulator_disable(ldo);
			regulator_put(ldo);
		}
	}

	//use external 32k clock.
	ap_32k = clk_get(NULL, "losc_out");
	if (!ap_32k){
		printk(KERN_ERR "Get ap 32k clk out failed!\n");
		return -1;
	}
#if 1
	ret = clk_prepare_enable(ap_32k);
	if (ret){
		printk(KERN_ERR "losc out enable failed!\n");
	}
#else
	clk_disable(ap_32k); //disable 32k clk.
#endif

	return ret;
}
#endif

int plat_module_power(int enable)
{
	int ret = 0;

#ifdef   V819MINI
	ret = plat_v819mini_pwr(enable);
#elif defined(HP8_G2)
	ret = plat_hp8_g2_pwr(enable);
#elif defined(EVB_A83T_ONBOARD)
	ret = plat_evb_a83t_onboard_pwr(enable);
#elif (defined(EVB_A33_XR819) || defined(EVB_A33W_XR819))
	ret = plat_a33_xr819_pwr(enable);
#elif (defined(A33W_SUNFLOWER_S1))
	ret = plat_a33w_sunflower_s1_pwr(enable);
#elif (defined(A33_CHIPHD) || defined(A33_YHK))
	ret = plat_a33_chiphd_pwr(enable);
#elif (defined(EVB_A31_CARD0)||defined(EVB_A31_CARD1))
	ret = plat_evb_a31_pwr(enable);
#else
	printk(KERN_ERR "plat_module_power: Fail to setup power!\n");
#endif

	if (enable) {  //delay for power stable period.
		mdelay(50);
	}
	return ret;
}
#endif //PLATFORM_RFKILL_PM

/*********************Interfaces called by xradio core. *********************/
#if (PLATFORM_SYSCONFIG)
static int xradio_get_syscfg(void)
{
	script_item_u val;
	script_item_value_type_e type;

	/* Get SDIO/USB config. */
#if defined(CONFIG_XRADIO_SDIO)
	type = script_get_item(WIFI_CONFIG, "wifi_sdc_id", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		printk(KERN_ERR "failed to fetch sdio card's sdcid\n");
		return -1;
	}
#elif defined(CONFIG_XRADIO_USB)
	type = script_get_item(WIFI_CONFIG, "wifi_usbc_id", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		printk(KERN_ERR "failed to fetch usb's id\n");
		return -1;
	}
#endif
	wlan_bus_id = val.val;

	/* Get GPIO config. */
	type = script_get_item(WIFI_CONFIG, "wl_reg_on", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO != type) {
		printk(KERN_ERR "failed to fetch wl_reg_on\n");
		return -1;
	}
	wlan_reset_gpio = val.gpio.gpio;

	type = script_get_item(WIFI_CONFIG, "wl_host_wake", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO != type) {
		printk(KERN_ERR "failed to fetch xradio_wl_host_wake\n");
		return -1;
	}
	wlan_irq_gpio = val.gpio.gpio;

	/* Get Power config. */
#if 0
	type = script_get_item(WIFI_CONFIG, "wifi_power", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR == type) {
		axp_name[0] = val.str;
		printk("wifi_power=%s\n", axp_name[0]);
	} else {
		printk(KERN_ERR "wifi_power not config\n");
	}
	type = script_get_item(WIFI_CONFIG, "wifi_power_ext1", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR == type) {
		axp_name[1] = val.str;
		printk("wifi_power_ext1=%s\n", axp_name[1]);
	} else {
		printk(KERN_ERR "wifi_power_ext1 not config\n");
	}
	type = script_get_item(WIFI_CONFIG, "wifi_power_ext2", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR == type) {
		axp_name[2] = val.str;
		printk("wifi_power_ext2=%s\n", axp_name[2]);
	} else {
		printk(KERN_ERR "wifi_power_ext2 not config\n");
	}
#endif
	return 0;
}
#endif

int  xradio_plat_init(void)
{
	int ret = 0;
#if (!PLATFORM_SYSCONFIG)
	wlan_bus_id     = PLAT_MMC_ID;
	wlan_irq_gpio   = PLAT_GPIO_WLAN_INT;
	wlan_reset_gpio = PLAT_GPIO_WRESET;
#else  //initialize from sys config.
	ret = xradio_get_syscfg();
#endif  //PLATFORM_SYSCONFIG

#if (PMU_POWER_WLAN_RETAIN && !PLATFORM_RFKILL_PM)
	plat_module_power(1);
#endif
	return ret;
}

void xradio_plat_deinit(void)
{
#if (PMU_POWER_WLAN_RETAIN && !PLATFORM_RFKILL_PM)
	plat_module_power(0);
#endif
}

#if (PLATFORM_RFKILL_PM)
//extern void wifi_pm_power(int on);
//void wifi_pm_power(int on)
void wifi_pm(int on)
{
	printk(KERN_ERR "xradio wlan power %s\n", on?"on":"off");
	if (on) {
		wifi_pm_gpio_ctrl("wl_reg_on", 1);
		mdelay(50);
		wifi_pm_gpio_ctrl("wl_reg_on", 0);
		mdelay(2);
		wifi_pm_gpio_ctrl("wl_reg_on", 1);
		mdelay(50);
	} else {
		wifi_pm_gpio_ctrl("wl_reg_on", 0);
	}	
}
#endif
int xradio_wlan_power(int on)
{
	int ret = 0;

#if (PLATFORM_RFKILL_PM)
	//wifi_pm_power(on);
	wifi_pm(on);
	mdelay(50);
#else //PLATFORM_RFKILL_PM
	ret = gpio_request(wlan_reset_gpio, "wlan_reset");
	if (!ret) {
		if (on) {  //power up.
#if (!PMU_POWER_WLAN_RETAIN)
			plat_module_power(1);
			mdelay(50);
#endif
			gpio_direction_output(wlan_reset_gpio, 1);
			mdelay(50);
			gpio_direction_output(wlan_reset_gpio, 0);
			mdelay(2);
			gpio_direction_output(wlan_reset_gpio, 1);
			mdelay(50);
		} else { //power down.
			gpio_direction_output(wlan_reset_gpio, 0);
#if (!PMU_POWER_WLAN_RETAIN)
			plat_module_power(0);
#endif
		}
		gpio_free(wlan_reset_gpio);
	} else {
		xradio_dbg(XRADIO_DBG_ERROR, "%s: gpio_request err: %d\n", __func__, ret);
	}
#endif
	return ret;
}

int xradio_sdio_detect(int enable)
{
	int insert = enable;
	MCI_RESCAN_CARD(wlan_bus_id, insert);
	xradio_dbg(XRADIO_DBG_ALWY, "%s SDIO card %d\n", 
	           enable?"Detect":"Remove", wlan_bus_id);
	mdelay(10);
	return 0;
}

#ifdef CONFIG_XRADIO_USE_GPIO_IRQ
static u32 gpio_irq_handle = 0;
#ifdef PLAT_ALLWINNER_SUNXI
static irqreturn_t xradio_gpio_irq_handler(int irq, void *sbus_priv)
{
	struct sbus_priv *self = (struct sbus_priv *)sbus_priv;
	unsigned long flags;

	SYS_BUG(!self);
	spin_lock_irqsave(&self->lock, flags);
	if (self->irq_handler)
		self->irq_handler(self->irq_priv);
	spin_unlock_irqrestore(&self->lock, flags);
	return IRQ_HANDLED;
}
#else //PLAT_ALLWINNER_SUN6I
static u32 xradio_gpio_irq_handler(void *sbus_priv)
{
	struct sbus_priv *self = (struct sbus_priv *)sbus_priv;
	unsigned long flags;

	SYS_BUG(!self);
	spin_lock_irqsave(&self->lock, flags);
	if (self->irq_handler)
		self->irq_handler(self->irq_priv);
	spin_unlock_irqrestore(&self->lock, flags);
	return 0;
}
#endif

int xradio_request_gpio_irq(struct device *dev, void *sbus_priv)
{
	int ret = -1;
	if(!gpio_irq_handle) {

#ifdef PLAT_ALLWINNER_SUNXI
		gpio_request(wlan_irq_gpio, "xradio_irq");
		gpio_direction_input(wlan_irq_gpio);
		gpio_irq_handle = gpio_to_irq(wlan_irq_gpio);
		ret = devm_request_irq(dev, gpio_irq_handle, 
		                      (irq_handler_t)xradio_gpio_irq_handler,
		                       IRQF_TRIGGER_RISING, "xradio_irq", sbus_priv);
		if (IS_ERR_VALUE(ret)) {
			gpio_irq_handle = 0;
		}
#else//PLAT_ALLWINNER_SUN6I
		gpio_irq_handle = sw_gpio_irq_request(wlan_irq_gpio, TRIG_EDGE_POSITIVE, 
			                                        (peint_handle)xradio_gpio_irq_handler, sbus_priv);
#endif // PLAT_ALLWINNER_SUNXI
	} else {
		xradio_dbg(XRADIO_DBG_ERROR, "%s: error, irq exist already!\n", __func__);
	}

	if (gpio_irq_handle) {
		xradio_dbg(XRADIO_DBG_NIY, "%s: request_irq sucess! irq=0x%08x\n", 
		           __func__, gpio_irq_handle);
		ret = 0;
	} else {
		xradio_dbg(XRADIO_DBG_ERROR, "%s: request_irq err: %d\n", __func__, ret);
		ret = -1;
	}
	return ret;
}

void xradio_free_gpio_irq(struct device *dev, void *sbus_priv)
{
	struct sbus_priv *self = (struct sbus_priv *)sbus_priv;
#ifdef PLAT_ALLWINNER_SUNXI
	if(gpio_irq_handle) {
		//for linux3.4
		devm_free_irq(dev, gpio_irq_handle, self);
		gpio_free(wlan_irq_gpio);
		gpio_irq_handle = 0;
	}
#else// PLAT_ALLWINNER_SUN6I
	if(gpio_irq_handle) {
		sw_gpio_irq_free(gpio_irq_handle);
		gpio_irq_handle = 0;
	}
#endif//PLAT_ALLWINNER_SUNXI
}
#endif /* CONFIG_XRADIO_USE_GPIO_IRQ */

/******************************************************************************************/