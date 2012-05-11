/*
 * kernel/drivers/media/video/tegra
 *
 * Aptina S5K6AA sensor driver
 *
 * Copyright (C) 2010 MALATA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <media/s5k6aa.h>


/*-------------------------------------------Important---------------------------------------
 * for changing the SENSOR_NAME, you must need to change the owner of the device. For example
 * Please add /dev/s5k6aa 0600 media camera in below file
 * ./device/nvidia/ventana/ueventd.ventana.rc
 * Otherwise, ioctl will get permission deny
 * -------------------------------------------Important--------------------------------------
*/

#define SENSOR_NAME	"s5k6aa"

#define SENSOR_WAIT_MS       0 /* special number to indicate this is wait time require */
#define SENSOR_TABLE_END     1 /* special number to indicate this is end of table */
#define SENSOR_MAX_RETRIES   3 /* max counter for retry I2C access */


struct sensor_reg {
	u16 addr;
	u16 val;
};


struct sensor_info {
	int mode;
	struct i2c_client *i2c_client;
	struct s5k6aa_platform_data *pdata;
};

static struct sensor_info *info;


enum {
      YUV_ColorEffect = 0,
      YUV_Whitebalance,
      YUV_SceneMode,
      YUV_Exposure
};

enum {
      YUV_ColorEffect_Invalid = 0,
      YUV_ColorEffect_Aqua,
      YUV_ColorEffect_Blackboard,
      YUV_ColorEffect_Mono,
      YUV_ColorEffect_Negative,
      YUV_ColorEffect_None,
      YUV_ColorEffect_Posterize,
      YUV_ColorEffect_Sepia,
      YUV_ColorEffect_Solarize,
      YUV_ColorEffect_Whiteboard
};

enum {
      YUV_Whitebalance_Invalid = 0,
      YUV_Whitebalance_Auto,
      YUV_Whitebalance_Incandescent,
      YUV_Whitebalance_Fluorescent,
      YUV_Whitebalance_WarmFluorescent,
      YUV_Whitebalance_Daylight,
      YUV_Whitebalance_CloudyDaylight,
      YUV_Whitebalance_Shade,
      YUV_Whitebalance_Twilight,
      YUV_Whitebalance_Custom
};

enum {
      YUV_SceneMode_Invalid = 0,
      YUV_SceneMode_Auto,
      YUV_SceneMode_Action,
      YUV_SceneMode_Portrait,
      YUV_SceneMode_Landscape,
      YUV_SceneMode_Beach,
      YUV_SceneMode_Candlelight,
      YUV_SceneMode_Fireworks,
      YUV_SceneMode_Night,
      YUV_SceneMode_NightPortrait,
      YUV_SceneMode_Party,
      YUV_SceneMode_Snow,
      YUV_SceneMode_Sports,
      YUV_SceneMode_SteadyPhoto,
      YUV_SceneMode_Sunset,
      YUV_SceneMode_Theatre,
      YUV_SceneMode_Barcode
};

enum {
      YUV_Exposure_Negative_2 = 0,
      YUV_Exposure_Negative_1,
      YUV_Exposure_0,
      YUV_Exposure_1,
      YUV_Exposure_2
};

static struct sensor_reg mode_1280x1024[] = {
{0xFCFC, 0xD000},  // change page to D000 page 
{0x0010, 0x0001},  // Reset 
{0x1030, 0x0000},  // Clear host interrupt so main will wait 
{0x0014, 0x0001},  // ARM go 
{SENSOR_WAIT_MS, 100},
{0x1000, 0x0001},
{SENSOR_WAIT_MS, 10},

{0x0028, 0x7000}, 
{0x002A, 0x0400},  //50h2
{0x0F12, 0x005f}, 
{0x002A, 0x03dc},  
{0x0F12, 0x0001}, 
{0x0F12, 0x0001}, 

{0x0028, 0xd000}, 	//add current
{0x002A, 0x1082}, 
{0x0F12, 0x03ff}, 
{0x0F12, 0x03ff}, 
{0x0F12, 0x0155}, 
{0x0F12, 0x0fff}, 
{0xFCFC, 0x7000},
{0x0488, 0x1A04},  
{0x048C, 0x56B8},
{0x0498, 0x0500},
{0x049A, 0x0500},
{0x049C, 0x0200},
{0x1000, 0x0050},
{0x0400, 0x007F},
{0x01B8, 0x4e20},
{0x01BA, 0x0000},
{0x01C6, 0x0001},
{0x01CC, 0x1388},
{0x01CE, 0x1388},
{0x01D0, 0x1388},
{0x01E0, 0x0001},
//PREVIEW CONFIGURATION 0
{0x0242, 0x0500},  // 1280
{0x0244, 0x0400},  // 1024 
{0x0246, 0x0005},  // FULL YUV 422
{0x024E, 0x0000},  // Use 1st system clock(0x01CC 01CE 01D0)
{0x0248, 0x1388},  // Max output rate, divided by 4 (12MHz)0bb8
{0x024A, 0x1388},  // Min output rate, divided by 4 (3MHz)
{0x024C, 0x0042},  // PVI configuration by default
{0x0252, 0x0002},  // Frame rate quality
{0x0250, 0x0000},  // Frame rate type
{0x0254, 0x0800},  // Required frame time ???
{0x0256, 0x0000},  // Minimal frame time for dynamic FR
//previer run
{0x021C, 0x0000},  // Index number of active preview configuration
{0x0220, 0x0001},  // Output is enabled after configuration change
{0x01F8, 0x0001},  // Start sending a new configuration
{0x021E, 0x0001},  // Synchronize FW with new preview configuration
{0x01F0, 0x0001},  // Enable preview output
{0x01F2, 0x0001},  // Synchronize FW with enable preview request
//input config 
{0x020A, 0x0500},  // Preview zoom window width
{0x020C, 0x0400},  // Preview zoom window heigh
{0x01FA, 0x0500},  // Preview window width
{0x01FC, 0x0400},  // Preview window heigh
{0x01FE, 0x0000},  // Preview input window X offset
{0x0200, 0x0000},  // Preview input window Y offset
{0x021A, 0x0001},  // Synchronize FW with input values

//Capture-B
{0xFCFC, 0x7000},
{0x0224, 0x0000},  // Index number of active preview configuration
{0x01F8, 0x0001},  // Start sending a new configuration
{0x0226, 0x0001},  // Index number of active preview configuration
{0x01F4, 0x0001},  // Start sending a new configuration
{0x01F6, 0x0001},  // Enable preview output
{SENSOR_WAIT_MS, 1000},
{SENSOR_TABLE_END, 0x00}
};


static struct sensor_reg mode_640x480[] = {

{0xFCFC, 0xD000},  // change page to D000 page 
{0x0010, 0x0001},  // Reset 
{0x1030, 0x0000},  // Clear host interrupt so main will wait 
{0x0014, 0x0001},  // ARM go 
{SENSOR_WAIT_MS, 50},
{0x1000, 0x0001},
{SENSOR_WAIT_MS, 10},
{0x0028, 0x7000}, 	// start add MSW
{0x002A, 0x1d60}, 	// start add LSW
{0x0F12, 0xb570}, 
{0x0F12, 0x4928}, 
{0x0F12, 0x4828}, 
{0x0F12, 0x2205}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf922}, 
{0x0F12, 0x4927}, 
{0x0F12, 0x2002}, 
{0x0F12, 0x83c8}, 
{0x0F12, 0x2001}, 
{0x0F12, 0x3120}, 
{0x0F12, 0x8088}, 
{0x0F12, 0x4925}, 
{0x0F12, 0x4826}, 
{0x0F12, 0x2204}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf917}, 
{0x0F12, 0x4925}, 
{0x0F12, 0x4825}, 
{0x0F12, 0x2206}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf912}, 
{0x0F12, 0x4924}, 
{0x0F12, 0x4825}, 
{0x0F12, 0x2207}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf90d}, 
{0x0F12, 0x4924}, 
{0x0F12, 0x4824}, 
{0x0F12, 0x2208}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf908}, 
{0x0F12, 0x4923}, 
{0x0F12, 0x4824}, 
{0x0F12, 0x2209}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf903}, 
{0x0F12, 0x4923}, 
{0x0F12, 0x4823}, 
{0x0F12, 0x60c1}, 
{0x0F12, 0x6882}, 
{0x0F12, 0x1a51}, 
{0x0F12, 0x8201}, 
{0x0F12, 0x4c22}, 
{0x0F12, 0x2607}, 
{0x0F12, 0x6821}, 
{0x0F12, 0x0736}, 
{0x0F12, 0x42b1}, 
{0x0F12, 0xda05}, 
{0x0F12, 0x4820}, 
{0x0F12, 0x22d8}, 
{0x0F12, 0x1c05}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf8fa}, 
{0x0F12, 0x6025}, 
{0x0F12, 0x68a1}, 
{0x0F12, 0x42b1}, 
{0x0F12, 0xda07}, 
{0x0F12, 0x481b}, 
{0x0F12, 0x2224}, 
{0x0F12, 0x3824}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf8f1}, 
{0x0F12, 0x4819}, 
{0x0F12, 0x3824}, 
{0x0F12, 0x60a0}, 
{0x0F12, 0x4d18}, 
{0x0F12, 0x6d29}, 
{0x0F12, 0x42b1}, 
{0x0F12, 0xda07}, 
{0x0F12, 0x4815}, 
{0x0F12, 0x228f}, 
{0x0F12, 0x00d2}, 
{0x0F12, 0x30d8}, 
{0x0F12, 0x1c04}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf8e3}, 
{0x0F12, 0x652c}, 
{0x0F12, 0xbc70}, 
{0x0F12, 0xbc08}, 
{0x0F12, 0x4718}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x1f53}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x127b}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x0398}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x1e4d}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x890d}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x1e73}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x27a9}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x1e91}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x27c5}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x1ef7}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x285f}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x1eb3}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x28ff}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x206c}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x04ac}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x06cc}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x23a4}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x0704}, 
{0x0F12, 0x7000}, 
{0x0F12, 0xb510}, 
{0x0F12, 0x1c04}, 
{0x0F12, 0x484d}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf8bb}, 
{0x0F12, 0x4a4d}, 
{0x0F12, 0x4b4d}, 
{0x0F12, 0x8811}, 
{0x0F12, 0x885b}, 
{0x0F12, 0x8852}, 
{0x0F12, 0x4359}, 
{0x0F12, 0x1889}, 
{0x0F12, 0x4288}, 
{0x0F12, 0xd800}, 
{0x0F12, 0x1c08}, 
{0x0F12, 0x6020}, 
{0x0F12, 0xbc10}, 
{0x0F12, 0xbc08}, 
{0x0F12, 0x4718}, 
{0x0F12, 0xb510}, 
{0x0F12, 0x1c04}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf8b1}, 
{0x0F12, 0x4944}, 
{0x0F12, 0x8989}, 
{0x0F12, 0x4348}, 
{0x0F12, 0x0200}, 
{0x0F12, 0x0c00}, 
{0x0F12, 0x2101}, 
{0x0F12, 0x0349}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf8b0}, 
{0x0F12, 0x6020}, 
{0x0F12, 0xe7ed}, 
{0x0F12, 0xb510}, 
{0x0F12, 0x1c04}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf8b2}, 
{0x0F12, 0x6821}, 
{0x0F12, 0x0409}, 
{0x0F12, 0x0c09}, 
{0x0F12, 0x1a40}, 
{0x0F12, 0x493a}, 
{0x0F12, 0x6849}, 
{0x0F12, 0x4281}, 
{0x0F12, 0xd800}, 
{0x0F12, 0x1c08}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf8af}, 
{0x0F12, 0x6020}, 
{0x0F12, 0xe7dc}, 
{0x0F12, 0xb570}, 
{0x0F12, 0x6801}, 
{0x0F12, 0x040d}, 
{0x0F12, 0x0c2d}, 
{0x0F12, 0x6844}, 
{0x0F12, 0x4833}, 
{0x0F12, 0x8981}, 
{0x0F12, 0x1c28}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf893}, 
{0x0F12, 0x8060}, 
{0x0F12, 0x4932}, 
{0x0F12, 0x69c9}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf8a6}, 
{0x0F12, 0x1c01}, 
{0x0F12, 0x80a0}, 
{0x0F12, 0x0228}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf8a9}, 
{0x0F12, 0x0400}, 
{0x0F12, 0x0c00}, 
{0x0F12, 0x8020}, 
{0x0F12, 0x492d}, 
{0x0F12, 0x2300}, 
{0x0F12, 0x5ec9}, 
{0x0F12, 0x4288}, 
{0x0F12, 0xda02}, 
{0x0F12, 0x20ff}, 
{0x0F12, 0x3001}, 
{0x0F12, 0x8020}, 
{0x0F12, 0xbc70}, 
{0x0F12, 0xbc08}, 
{0x0F12, 0x4718}, 
{0x0F12, 0xb570}, 
{0x0F12, 0x1c04}, 
{0x0F12, 0x4828}, 
{0x0F12, 0x4926}, 
{0x0F12, 0x7803}, 
{0x0F12, 0x6a8a}, 
{0x0F12, 0x2b00}, 
{0x0F12, 0xd100}, 
{0x0F12, 0x6a0a}, 
{0x0F12, 0x4d20}, 
{0x0F12, 0x2b00}, 
{0x0F12, 0x68a8}, 
{0x0F12, 0xd100}, 
{0x0F12, 0x6868}, 
{0x0F12, 0x6823}, 
{0x0F12, 0x8dc9}, 
{0x0F12, 0x434a}, 
{0x0F12, 0x0a12}, 
{0x0F12, 0x429a}, 
{0x0F12, 0xd30d}, 
{0x0F12, 0x4d20}, 
{0x0F12, 0x26ff}, 
{0x0F12, 0x8828}, 
{0x0F12, 0x3601}, 
{0x0F12, 0x43b0}, 
{0x0F12, 0x8028}, 
{0x0F12, 0x6820}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf884}, 
{0x0F12, 0x6020}, 
{0x0F12, 0x8828}, 
{0x0F12, 0x4330}, 
{0x0F12, 0x8028}, 
{0x0F12, 0xe7da}, 
{0x0F12, 0x1c0a}, 
{0x0F12, 0x4342}, 
{0x0F12, 0x0a12}, 
{0x0F12, 0x429a}, 
{0x0F12, 0xd304}, 
{0x0F12, 0x0218}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf871}, 
{0x0F12, 0x6020}, 
{0x0F12, 0xe7f4}, 
{0x0F12, 0x6020}, 
{0x0F12, 0xe7f2}, 
{0x0F12, 0xb510}, 
{0x0F12, 0x4913}, 
{0x0F12, 0x8fc8}, 
{0x0F12, 0x2800}, 
{0x0F12, 0xd007}, 
{0x0F12, 0x2000}, 
{0x0F12, 0x87c8}, 
{0x0F12, 0x8f88}, 
{0x0F12, 0x4c11}, 
{0x0F12, 0x2800}, 
{0x0F12, 0xd002}, 
{0x0F12, 0x2008}, 
{0x0F12, 0x8020}, 
{0x0F12, 0xe77e}, 
{0x0F12, 0x480d}, 
{0x0F12, 0x3060}, 
{0x0F12, 0x8900}, 
{0x0F12, 0x2800}, 
{0x0F12, 0xd103}, 
{0x0F12, 0x480c}, 
{0x0F12, 0x2101}, 
{0x0F12, 0xf000}, 
{0x0F12, 0xf864}, 
{0x0F12, 0x2010}, 
{0x0F12, 0x8020}, 
{0x0F12, 0xe7f2}, 
{0x0F12, 0x0000}, 
{0x0F12, 0xf4b0}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x2058}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x1554}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x0080}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x046c}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x0468}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x1100}, 
{0x0F12, 0xd000}, 
{0x0F12, 0x01b8}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x044e}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x0450}, 
{0x0F12, 0x7000}, 
{0x0F12, 0x4778}, 
{0x0F12, 0x46c0}, 
{0x0F12, 0xc000}, 
{0x0F12, 0xe59f}, 
{0x0F12, 0xff1c}, 
{0x0F12, 0xe12f}, 
{0x0F12, 0x9ce7}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x4778}, 
{0x0F12, 0x46c0}, 
{0x0F12, 0xf004}, 
{0x0F12, 0xe51f}, 
{0x0F12, 0x9fb8}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x4778}, 
{0x0F12, 0x46c0}, 
{0x0F12, 0xc000}, 
{0x0F12, 0xe59f}, 
{0x0F12, 0xff1c}, 
{0x0F12, 0xe12f}, 
{0x0F12, 0x88df}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x4778}, 
{0x0F12, 0x46c0}, 
{0x0F12, 0xc000}, 
{0x0F12, 0xe59f}, 
{0x0F12, 0xff1c}, 
{0x0F12, 0xe12f}, 
{0x0F12, 0x275d}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x4778}, 
{0x0F12, 0x46c0}, 
{0x0F12, 0xc000}, 
{0x0F12, 0xe59f}, 
{0x0F12, 0xff1c}, 
{0x0F12, 0xe12f}, 
{0x0F12, 0x1ed3}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x4778}, 
{0x0F12, 0x46c0}, 
{0x0F12, 0xc000}, 
{0x0F12, 0xe59f}, 
{0x0F12, 0xff1c}, 
{0x0F12, 0xe12f}, 
{0x0F12, 0x26f9}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x4778}, 
{0x0F12, 0x46c0}, 
{0x0F12, 0xc000}, 
{0x0F12, 0xe59f}, 
{0x0F12, 0xff1c}, 
{0x0F12, 0xe12f}, 
{0x0F12, 0x4027}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x4778}, 
{0x0F12, 0x46c0}, 
{0x0F12, 0xc000}, 
{0x0F12, 0xe59f}, 
{0x0F12, 0xff1c}, 
{0x0F12, 0xe12f}, 
{0x0F12, 0x9f03}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x4778}, 
{0x0F12, 0x46c0}, 
{0x0F12, 0xf004}, 
{0x0F12, 0xe51f}, 
{0x0F12, 0xa144}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x4778}, 
{0x0F12, 0x46c0}, 
{0x0F12, 0xc000}, 
{0x0F12, 0xe59f}, 
{0x0F12, 0xff1c}, 
{0x0F12, 0xe12f}, 
{0x0F12, 0x285f}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x4778}, 
{0x0F12, 0x46c0}, 
{0x0F12, 0xc000}, 
{0x0F12, 0xe59f}, 
{0x0F12, 0xff1c}, 
{0x0F12, 0xe12f}, 
{0x0F12, 0x2001}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x0000}, 
{0x0F12, 0xe848}, 
{0x0F12, 0x0001}, 
{0x0F12, 0xe848}, 
{0x0F12, 0x0001}, 
{0x0F12, 0x0500}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x0000}, 
{0x0F12, 0x0000}, 
{0x002A, 0x0e3a},  // #awbb_Alpha_Comp_Mode
{0x0F12, 0x02C2}, 
{0x002A, 0x112a},  //#senHal_SenRegsModes3_pSenModesRegsArray3[8]
{0x0F12, 0x0000}, 
{0x002A, 0x1132},  //#senHal_SenRegsModes3_pSenModesRegsArray3[12]
{0x0F12, 0x0000}, 
{0x002A, 0x113e},  //#senHal_SenRegsModes3_pSenModesRegsArray3[18]
{0x0F12, 0x0000}, 
{0x002A, 0x115c},  //#senHal_SenRegsModes3_pSenModesRegsArray3[33]
{0x0F12, 0x0000}, 
{0x002A, 0x1164},  //#senHal_SenRegsModes3_pSenModesRegsArray3[37]
{0x0F12, 0x0000}, 
{0x002A, 0x1174},  //#senHal_SenRegsModes3_pSenModesRegsArray3[45]
{0x0F12, 0x0000}, 
{0x002A, 0x1178},  //#senHal_SenRegsModes3_pSenModesRegsArray3[47]
{0x0F12, 0x0000}, 
{0x002A, 0x077a},  //#msm_uOffsetNoBin[0][0]
{0x0F12, 0x0000},  //#msm_uOffsetNoBin[0][1]
{0x0F12, 0x0000}, 	//#msm_uOffsetNoBin[1][0]
{0x0F12, 0x0000}, 	//#msm_uOffsetNoBin[1][1]
{0x0F12, 0x0000}, 	//#msm_uOffsetNoBin[2][0]
{0x0F12, 0x0000}, 	//#msm_uOffsetNoBin[2][1]
{0x0F12, 0x0000}, 	//#msm_uOffsetNoBin[3][0]
{0x0F12, 0x0000}, 	//#msm_uOffsetNoBin[3][1]
{0x0F12, 0x0000}, 
{0x002A, 0x07a2},  //#msm_sAnalogOffset[0]
{0x0F12, 0x0000}, 	 //#msm_sAnalogOffset[1]
{0x0F12, 0x0000}, 	 //#msm_sAnalogOffset[2]
{0x0F12, 0x0000}, 	 //#msm_sAnalogOffset[3]
{0x0F12, 0x0000}, 
{0x002A, 0x07b6},  //#msm_NonLinearOfsOutput[0]
{0x0F12, 0x0000}, 	 //#msm_NonLinearOfsOutput[1]
{0x0F12, 0x0002}, 	 //#msm_NonLinearOfsOutput[2]
{0x0F12, 0x0004}, 	 //#msm_NonLinearOfsOutput[3]
{0x0F12, 0x0004}, 	 //#msm_NonLinearOfsOutput[4]
{0x0F12, 0x0005}, 	 //#msm_NonLinearOfsOutput[5]
{0x0F12, 0x0005}, 
{0x002A, 0x0712}, 
{0x0F12, 0x0149}, 	  
{0x0F12, 0x011B}, 	  
{0x0F12, 0x0120}, 	  
{0x0F12, 0x00EF}, 	  
{0x0F12, 0x00C3}, 	  
{0x0F12, 0x00D2}, 	  
{0x0F12, 0x00DC}, 	  
{0x0F12, 0x00B8}, 	  
{0x0F12, 0x0106}, 	  
{0x0F12, 0x00DE}, 	  
{0x0F12, 0x00E3}, 	  
{0x0F12, 0x00CC}, 	  
{0x0F12, 0x00BD}, 	  
{0x0F12, 0x00E1}, 	  
{0x0F12, 0x00D8}, 	  
{0x0F12, 0x00D0}, 	  
{0x0F12, 0x00BE}, 	  
{0x0F12, 0x00EC}, 	  
{0x0F12, 0x00EA}, 	  
{0x0F12, 0x00F2}, 	  
{0x0F12, 0x00BE}, 	  
{0x0F12, 0x00EB}, 	  
{0x0F12, 0x00E5}, 	  
{0x0F12, 0x00F9}, 	  
{0x0F12, 0x0100},  //0x00BE	  
{0x0F12, 0x00D5}, 	  
{0x0F12, 0x00D8}, 	  
{0x0F12, 0x00E6},                 	
{0x0F12, 0x00FD}, 	    
{0x0F12, 0x00F7}, 	    
{0x0F12, 0x00F3}, 	    
{0x0F12, 0x00FF}, 
{0x002A, 0x075A}, 
{0x0F12, 0x0001}, 	
{0x0F12, 0x02A1}, 	
{0x0F12, 0x0225}, 	
{0x0F12, 0x000D}, 	
{0x0F12, 0x000D}, 
{0x0028, 0x7000},
{0x002A, 0x04C8}, //	GammaLutRGBIndoor
{0x0F12, 0x0000}, 	   
{0x0F12, 0x0004}, 	   
{0x0F12, 0x0009}, 	   
{0x0F12, 0x0015}, 	   
{0x0F12, 0x0034}, 	   
{0x0F12, 0x0088}, 	   
{0x0F12, 0x00ED}, 	   
{0x0F12, 0x0155}, 	   
{0x0F12, 0x0201}, 	   
{0x0F12, 0x0276}, 	   
{0x0F12, 0x0307}, 	   
{0x0F12, 0x0362}, 	   
{0x0F12, 0x03A9}, 	   
{0x0F12, 0x03D5}, 	   
{0x0F12, 0x03F4}, 	   
{0x0F12, 0x03FF}, 	   
{0x0F12, 0x0000}, 	   
{0x0F12, 0x0004}, 	   
{0x0F12, 0x0009}, 	   
{0x0F12, 0x0015}, 	   
{0x0F12, 0x0034}, 	   
{0x0F12, 0x0088}, 	   
{0x0F12, 0x00ED}, 	   
{0x0F12, 0x0155}, 	   
{0x0F12, 0x0201}, 	   
{0x0F12, 0x0276}, 	   
{0x0F12, 0x0307}, 	   
{0x0F12, 0x0362}, 	   
{0x0F12, 0x03A9}, 	   
{0x0F12, 0x03D5}, 	   
{0x0F12, 0x03F4}, 	   
{0x0F12, 0x03FF}, 	   
{0x0F12, 0x0000}, 	   
{0x0F12, 0x0004}, 	   
{0x0F12, 0x0009}, 	   
{0x0F12, 0x0015}, 	   
{0x0F12, 0x0034}, 	   
{0x0F12, 0x0088}, 	   
{0x0F12, 0x00ED}, 	   
{0x0F12, 0x0155}, 	   
{0x0F12, 0x0201}, 	   
{0x0F12, 0x0276}, 	   
{0x0F12, 0x0307}, 	   
{0x0F12, 0x0362}, 	   
{0x0F12, 0x03A9}, 	   
{0x0F12, 0x03D5}, 	   
{0x0F12, 0x03F4}, 	   
{0x0F12, 0x03FF},                      
{0x002A, 0x1000},	//	param_end	SARR_usGammaLutRGBIndoor          
{0x0F12, 0x0036},                   
{0x002A, 0x108E},   // SAAR_IllumType        
{0x0F12, 0x00C0}, 	    
{0x0F12, 0x00E7}, 	    
{0x0F12, 0x00F9}, 	    
{0x0F12, 0x0142}, 	    
{0x0F12, 0x0179}, 	    
{0x0F12, 0x01A4}, 	    
{0x0F12, 0x01B8},   //SAAR_IllumF	
{0x0F12, 0x0112}, 	
{0x0F12, 0x0122}, 	
{0x0F12, 0x0136}, 	
{0x0F12, 0x00F6}, 	
{0x0F12, 0x0100}, 	
{0x0F12, 0x00FE}, 	
{0x0F12, 0x0100}, 
{0x002A, 0x1AC8}, 
{0x0F12, 0x0000},   //	param_start	TVAR_wbt_pBaseCcms
{0x002A, 0x23A4}, 
{0x0F12, 0x0213}, 	
{0x0F12, 0xFF95}, 	
{0x0F12, 0xFFF6}, 	
{0x0F12, 0x0067}, 	
{0x0F12, 0x0118}, 	
{0x0F12, 0xFF1F}, 	
{0x0F12, 0xFF0A}, 	
{0x0F12, 0x01A9}, 	
{0x0F12, 0xFF6E}, 	
{0x0F12, 0xFECE}, 	
{0x0F12, 0x01C7}, 	
{0x0F12, 0x012E}, 	
{0x0F12, 0xFFE9}, 	
{0x0F12, 0x0009}, 	
{0x0F12, 0x01FD}, 	
{0x0F12, 0x015B}, 	
{0x0F12, 0xFF0C}, 	
{0x0F12, 0x014F}, 	
{0x0F12, 0x0213}, 	
{0x0F12, 0xFF95}, 	
{0x0F12, 0xFFF6}, 	
{0x0F12, 0x0067}, 	
{0x0F12, 0x0118}, 	
{0x0F12, 0xFF1F}, 	
{0x0F12, 0xFF0A}, 	
{0x0F12, 0x01A9}, 	
{0x0F12, 0xFF6E}, 	
{0x0F12, 0xFECE}, 	
{0x0F12, 0x01C7}, 	
{0x0F12, 0x012E}, 	
{0x0F12, 0xFFE9}, 	
{0x0F12, 0x0009}, 	
{0x0F12, 0x01FD}, 	
{0x0F12, 0x015B}, 	
{0x0F12, 0xFF0C}, 	
{0x0F12, 0x014F}, 	
{0x0F12, 0x0213}, 	
{0x0F12, 0xFF95}, 	
{0x0F12, 0xFFF6}, 	
{0x0F12, 0x0067}, 	
{0x0F12, 0x0118}, 	
{0x0F12, 0xFF1F}, 	
{0x0F12, 0xFF0A}, 	
{0x0F12, 0x01A9}, 	
{0x0F12, 0xFF6E}, 	
{0x0F12, 0xFECE}, 	
{0x0F12, 0x01C7}, 	
{0x0F12, 0x012E}, 	
{0x0F12, 0xFFE9}, 	
{0x0F12, 0x0009}, 	
{0x0F12, 0x01FD}, 	
{0x0F12, 0x015B}, 	
{0x0F12, 0xFF0C}, 	
{0x0F12, 0x014F}, 	
{0x0F12, 0x0213}, 	
{0x0F12, 0xFF95}, 	
{0x0F12, 0xFFF6}, 	
{0x0F12, 0x0067}, 	
{0x0F12, 0x0118}, 	
{0x0F12, 0xFF1F}, 	
{0x0F12, 0xFF0A}, 	
{0x0F12, 0x01A9}, 	
{0x0F12, 0xFF6E}, 	
{0x0F12, 0xFECE}, 	
{0x0F12, 0x01C7}, 	
{0x0F12, 0x012E}, 	
{0x0F12, 0xFFE9}, 	
{0x0F12, 0x0009}, 	
{0x0F12, 0x01FD}, 	
{0x0F12, 0x015B}, 	
{0x0F12, 0xFF0C}, 	
{0x0F12, 0x014F}, 	
{0x0F12, 0x0213}, 	
{0x0F12, 0xFF95}, 	
{0x0F12, 0xFFF6}, 	
{0x0F12, 0x0067}, 	
{0x0F12, 0x0118}, 	
{0x0F12, 0xFF1F}, 	
{0x0F12, 0xFF0A}, 	
{0x0F12, 0x01A9}, 	
{0x0F12, 0xFF6E}, 	
{0x0F12, 0xFECE}, 	
{0x0F12, 0x01C7}, 	
{0x0F12, 0x012E}, 	
{0x0F12, 0xFFE9}, 	
{0x0F12, 0x0009}, 	
{0x0F12, 0x01FD}, 	
{0x0F12, 0x015B}, 	
{0x0F12, 0xFF0C}, 	
{0x0F12, 0x014F}, 	
{0x0F12, 0x0213}, 	
{0x0F12, 0xFF95}, 	
{0x0F12, 0xFFF6}, 	
{0x0F12, 0x0067}, 	
{0x0F12, 0x0118}, 	
{0x0F12, 0xFF1F}, 	
{0x0F12, 0xFF0A}, 	
{0x0F12, 0x01A9}, 	
{0x0F12, 0xFF6E}, 	
{0x0F12, 0xFECE}, 	
{0x0F12, 0x01C7}, 	
{0x0F12, 0x012E}, 	
{0x0F12, 0xFFE9}, 	
{0x0F12, 0x0009}, 	
{0x0F12, 0x01FD}, 	
{0x0F12, 0x015B}, 	
{0x0F12, 0xFF0C}, 	
{0x0F12, 0x014F}, //	param_end	TVAR_wbt_pBaseCcms
{0x002A, 0x2380}, //	param_start	TVAR_wbt_pOutdoorCcm
{0x0F12, 0x01AF}, 	
{0x0F12, 0xFFD5}, 	
{0x0F12, 0x001D}, 	
{0x0F12, 0x0080}, 	
{0x0F12, 0x00BA}, 	
{0x0F12, 0xFF61}, 	
{0x0F12, 0xFF21}, 	
{0x0F12, 0x0164}, 	
{0x0F12, 0xFF96}, 	
{0x0F12, 0xFF0F}, 	
{0x0F12, 0x019A}, 	
{0x0F12, 0x0117}, 	
{0x0F12, 0xFFE8}, 	
{0x0F12, 0x0041}, 	
{0x0F12, 0x01C8}, 	
{0x0F12, 0x0173}, 	
{0x0F12, 0xFF35}, 	
{0x0F12, 0x013C},   //	param_end	TVAR_wbt_pOutdoorCcm
{0x002A, 0x06DA}, 
{0x0F12, 0x00BF}, 	
{0x0F12, 0x00E6}, 	
{0x0F12, 0x00F2}, 	
{0x0F12, 0x0143}, 	
{0x0F12, 0x0178}, 	
{0x0F12, 0x01A3},  //	param_start	SARR_uNormBrInDoor
{0x002A, 0x07E8}, 
{0x0F12, 0x000A}, 	
{0x0F12, 0x0019}, 	
{0x0F12, 0x007D}, 	
{0x0F12, 0x01F4}, 	
{0x0F12, 0x1388}, 


{0x0028, 0x7000}, 
{0x002A, 0x0400},  //50h2
{0x0F12, 0x005f}, 
{0x002A, 0x03dc},  
{0x0F12, 0x0001}, 
{0x0F12, 0x0001}, 
{0x0028, 0xd000}, 	//add current
{0x002A, 0x1082}, 
{0x0F12, 0x03ff}, 
{0x0F12, 0x03ff}, 
{0x0F12, 0x0155}, 
{0x0F12, 0x0fff}, 
{0xFCFC, 0x7000},
{0x0488, 0x1A04},  
{0x048C, 0x56B8},
{0x0498, 0x0500},
{0x049A, 0x0500},
{0x049C, 0x0200},
{0x1000, 0x0043},
{0x0400, 0x007F},
{0x01B8, 0x4e20},
{0x01BA, 0x0000},
{0x01C6, 0x0001},
{0x01CC, 0x1388},
{0x01CE, 0x1388},
{0x01D0, 0x1388},
{0x01E0, 0x0001},
//PREVIEW CONFIGURATION 0
{0x0242, 0x0280},  // 640
{0x0244, 0x01e0},  // 480 
{0x0246, 0x0005},  // FULL YUV 422
{0x024E, 0x0000},  // Use 1st system clock(0x01CC 01CE 01D0)
{0x0248, 0x1388},  // Max output rate, divided by 4 (12MHz)0bb8
{0x024A, 0x1388},  // Min output rate, divided by 4 (3MHz)
{0x024C, 0x0042},  // PVI configuration by default
{0x0252, 0x0001},  // Frame rate quality
{0x0250, 0x0002},  // Frame rate type
//{0x0254, 0x029a},  // Required frame time ???
{0x0254, 0x01b0},  // Required frame time ???
//{0x0256, 0x01b0},  // Minimal frame time for dynamic FR
{0x0256, 0x0021},  // Minimal frame time for dynamic FR
//previer run
{0x021C, 0x0000},  // Index number of active preview configuration
{0x0220, 0x0001},  // Output is enabled after configuration change
{0x01F8, 0x0001},  // Start sending a new configuration
{0x021E, 0x0001},  // Synchronize FW with new preview configuration
{0x01F0, 0x0001},  // Enable preview output
{0x01F2, 0x0001},  // Synchronize FW with enable preview request
//capture CONFIGURATION 0
{0x030c, 0x0000},   //1280 1024
{0x030e, 0x0500},   
{0x0310, 0x0400},   
{0x0312, 0x0005},   
{0x0314, 0x1388},   
{0x0316, 0x1388},   
{0x0318, 0x0042},   
{0x031a, 0x0000},   
{0x031c, 0x0000},//timetype   
{0x031e, 0x0002},   
{0x0320, 0x07d0},   
{0x0322, 0x03e8},   
//input config 
{0x020A, 0x0400},// 1024//x0500},  // Preview zoom window width
{0x020C, 0x0300},// 768//0x0400},  // Preview zoom window heigh
{0x01FA, 0x0500},  // Preview window width
{0x01FC, 0x0400},  // Preview window heigh
{0x01FE, 0x0000},  // Preview input window X offset
{0x0200, 0x0000},  // Preview input window Y offset
{0x021A, 0x0001},  // Synchronize FW with input values

//Capture-B
{0xFCFC, 0x7000},
{0x021C, 0x0000},  // Index number of active preview configuration
{0x0220, 0x0001},  // Output is enabled after configuration change
{0x01F8, 0x0001},  // Start sending a new configuration
{0x021E, 0x0001},  // Synchronize FW with new preview configuration

{SENSOR_WAIT_MS, 500},
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg SetCapture[] =
{
{0xFCFC, 0x7000},
{0x0224, 0x0000},  //REG_TC_GP_ActiveCapConfig
{0x01F8, 0x0001},  //Start sending a new configuration
{0x0226, 0x0001},  //REG_TC_GP_CapConfigChanged
{0x01F4, 0x0001},  // REG_TC_GP_EnableCapture
{0x01F6, 0x0001},  // REG_TC_GP_EnableCaptureChanged
{SENSOR_WAIT_MS, 200},
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg SetPreview[] =
{
{0xFCFC, 0x7000},
{0x021C, 0x0000},  // Index number of active preview configuration
{0x0220, 0x0001},  // Output is enabled after configuration change
{0x01F8, 0x0001},  // Start sending a new configuration
{0x021E, 0x0001},  // Synchronize FW with new preview configuration
{SENSOR_WAIT_MS, 200},
{SENSOR_TABLE_END, 0x00}
};


static struct sensor_reg ColorEffect_None[] = {
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg ColorEffect_Mono[] = {
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg ColorEffect_Sepia[] = {
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg ColorEffect_Negative[] = {
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg ColorEffect_Solarize[] = {

{SENSOR_TABLE_END, 0x00}
};

/* Sensor ISP Not Support this function */
static struct sensor_reg ColorEffect_Posterize[] = {
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg Whitebalance_Auto[] = {
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg Whitebalance_Incandescent[] = {
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg Whitebalance_Daylight[] = {
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg Whitebalance_Fluorescent[] = {
{SENSOR_TABLE_END, 0x00}
};

//static struct sensor_reg Whitebalance_Cloudy[] = {
//{SENSOR_TABLE_END, 0x00}
//};

static struct sensor_reg Exposure_2[] = {
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg Exposure_1[] = {
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg Exposure_0[] = {
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg Exposure_Negative_1[] = {
{SENSOR_TABLE_END, 0x00}
};

static struct sensor_reg Exposure_Negative_2[] = {
{SENSOR_TABLE_END, 0x00}
};

enum {
	SENSOR_MODE_1280x1024,
	SENSOR_MODE_640x480,
};

static struct sensor_reg *mode_table[] = {
	[SENSOR_MODE_1280x1024]  = mode_1280x1024,
	[SENSOR_MODE_640x480]   = mode_640x480,
};

static int sensor_read_reg(struct i2c_client *client, u16 addr, u16 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (addr >> 8);;
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	/* swap high and low byte to match table format */
	swap(*(data+2),*(data+3));

	return 0;
}

static int sensor_write_reg(struct i2c_client *client, u16 addr, u16 val)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[4];
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val >> 8);
	data[3] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 4;
	msg.buf = data;

	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry++;
		pr_err("yuv_sensor : i2c transfer failed, retrying %x %x\n",
		       addr, val);
		msleep(8);
	} while (retry <= SENSOR_MAX_RETRIES);

	return err;
}

static int sensor_write_table(struct i2c_client *client,
			      const struct sensor_reg table[])
{
	int err;
	const struct sensor_reg *next;
	u16 val;

	pr_info("s5k6aa %s\n",__func__);
	for (next = table; next->addr != SENSOR_TABLE_END; next++) {
		if (next->addr == SENSOR_WAIT_MS) {
			msleep(next->val);

			continue;
		}

		val = next->val;
		err = sensor_write_reg(client, next->addr, val);
		if (err)
			return err;
	}
	return 0;
}

static int sensor_set_mode(struct sensor_info *info, struct s5k6aa_mode *mode)
{
	int sensor_table;
	int err;

	pr_info("%s: xres %u yres %u\n",__func__, mode->xres, mode->yres);

	if (mode->xres == 1280 && mode->yres == 1024)
		sensor_table = SENSOR_MODE_1280x1024;
	else if (mode->xres == 640 && mode->yres == 480)
		sensor_table = SENSOR_MODE_640x480;
	else {
		pr_err("%s: invalid resolution supplied to set mode %d %d\n",
		       __func__, mode->xres, mode->yres);
               sensor_table = SENSOR_MODE_640x480;
	}

	if ((info->mode == SENSOR_MODE_640x480) && (sensor_table == SENSOR_MODE_1280x1024))
	{
	    pr_debug("%s: will send SetCapture\n",__func__);
	    err = sensor_write_table(info->i2c_client, SetCapture);
	}
	else if ((info->mode == SENSOR_MODE_1280x1024) && (sensor_table == SENSOR_MODE_640x480))
	{
	    pr_debug("%s: will send SetPreview\n",__func__);
	    err = sensor_write_table(info->i2c_client, SetPreview);
	}
	else
	{
	    pr_debug("%s: will send InitSeq\n",__func__);
	    err = sensor_write_table(info->i2c_client, mode_table[sensor_table]);
	}

	if (err)
	{
	    pr_info("sensor_write_table sensor_table =%d,err is %d\n", sensor_table,err);
	    if (info->pdata && info->pdata->power_off)
		    info->pdata->power_off();
		mdelay(20);
	    info->mode = -1;
		return err;
	}
	/* if no table is programming before and request set to 1600x1200, then
	 * we must use 1600x1200 table to fix CTS testing issue
	 */
	info->mode = sensor_table;
	return 0;
}

static long sensor_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	struct sensor_info *info = file->private_data;
        int err=0;

	pr_info("s5k6aa %s\n",__func__);

	switch (cmd) {
	case S5K6AA_IOCTL_SET_MODE:
	{
		struct s5k6aa_mode mode;
		if (copy_from_user(&mode,
				   (const void __user *)arg,
				   sizeof(struct s5k6aa_mode))) {
			return -EFAULT;
		}

		return sensor_set_mode(info, &mode);
	}
	case S5K6AA_IOCTL_GET_STATUS:
	{

		return 0;
	}
        case S5K6AA_IOCTL_SET_COLOR_EFFECT:
        {
                u8 coloreffect;

		if (copy_from_user(&coloreffect,
				   (const void __user *)arg,
				   sizeof(coloreffect))) {
			return -EFAULT;
		}

		pr_info("%s,IOCTL_SET_COLOR_EFFECT,coloreffect is %d\n",__func__, coloreffect);

		switch(coloreffect)
                {
                    case YUV_ColorEffect_None:
	                 err = sensor_write_table(info->i2c_client, ColorEffect_None);
                         break;
                    case YUV_ColorEffect_Mono:
	                 err = sensor_write_table(info->i2c_client, ColorEffect_Mono);
                         break;
                    case YUV_ColorEffect_Sepia:
	                 err = sensor_write_table(info->i2c_client, ColorEffect_Sepia);
                         break;
                    case YUV_ColorEffect_Negative:
	                 err = sensor_write_table(info->i2c_client, ColorEffect_Negative);
                         break;
                    case YUV_ColorEffect_Solarize:
	                 err = sensor_write_table(info->i2c_client, ColorEffect_Solarize);
                         break;
                    case YUV_ColorEffect_Posterize:
	                 err = sensor_write_table(info->i2c_client, ColorEffect_Posterize);
                         break;
                    default:
                         break;
                }

	        if (err)
		   return err;

                return 0;
        }
        case S5K6AA_IOCTL_SET_WHITE_BALANCE:
        {
                u8 whitebalance;

		if (copy_from_user(&whitebalance,
				   (const void __user *)arg,
				   sizeof(whitebalance))) {
			return -EFAULT;
		}

		pr_info("%s,IOCTL_SET_WHITE_BALANCE,whitebalance is %d\n",__func__, whitebalance);

                switch(whitebalance)
                {
                    case YUV_Whitebalance_Auto:
	                 err = sensor_write_table(info->i2c_client, Whitebalance_Auto);
                         break;
                    case YUV_Whitebalance_Incandescent:
	                 err = sensor_write_table(info->i2c_client, Whitebalance_Incandescent);
                         break;
                    case YUV_Whitebalance_Daylight:
	                 err = sensor_write_table(info->i2c_client, Whitebalance_Daylight);
                         break;
                    case YUV_Whitebalance_Fluorescent:
	                 err = sensor_write_table(info->i2c_client, Whitebalance_Fluorescent);
                         break;
                    default:
                         break;
                }

	        if (err)
		   return err;

                return 0;
        }
        case S5K6AA_IOCTL_SET_SCENE_MODE:
        {
                return 0;
        }
        case S5K6AA_IOCTL_SET_EXPOSURE:
        {
                u8 exposure;

		if (copy_from_user(&exposure,
				   (const void __user *)arg,
				   sizeof(exposure))) {
			return -EFAULT;
		}

		pr_info("%s, IOCTL_SET_EXPOSURE,exposure is %d\n",__func__, exposure);

                switch(exposure)
                {
                    case YUV_Exposure_0:
	                 err = sensor_write_table(info->i2c_client, Exposure_0);
                         break;
                    case YUV_Exposure_1:
	                 err = sensor_write_table(info->i2c_client, Exposure_1);
                         break;
                    case YUV_Exposure_2:
	                 err = sensor_write_table(info->i2c_client, Exposure_2);
                         break;
                    case YUV_Exposure_Negative_1:
	                 err = sensor_write_table(info->i2c_client, Exposure_Negative_1);
                         break;
                    case YUV_Exposure_Negative_2:
	                 err = sensor_write_table(info->i2c_client, Exposure_Negative_2);
                         break;
                    default:
                         break;
                }

	        if (err)
		   return err;

                return 0;
        }
	default:
		return -EINVAL;
	}
	return 0;
}

static int sensor_open(struct inode *inode, struct file *file)
{
	pr_info("s5k6aa %s\n",__func__);
	file->private_data = info;
	if (info->pdata && info->pdata->power_on)
		info->pdata->power_on();
	info->mode = -1;
	return 0;
}

static int sensor_release(struct inode *inode, struct file *file)
{
	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off();
	file->private_data = NULL;
	info->mode = -1;
	return 0;
}


static const struct file_operations sensor_fileops = {
	.owner = THIS_MODULE,
	.open = sensor_open,
	.unlocked_ioctl = sensor_ioctl,
	.release = sensor_release,
};

static struct miscdevice sensor_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = SENSOR_NAME,
	.fops = &sensor_fileops,
};

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;

	pr_info("s5k6aa %s\n",__func__);

	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);

	if (!info) {
		pr_err("yuv_sensor : Unable to allocate memory!\n");
		return -ENOMEM;
	}

	client->addr = 0x3C;

	err = misc_register(&sensor_device);
	if (err) {
		pr_err("yuv_sensor : Unable to register misc device!\n");
		kfree(info);
		return err;
	}

	info->pdata = client->dev.platform_data;
	info->i2c_client = client;

	i2c_set_clientdata(client, info);
	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct sensor_info *info;

	pr_info("yuv %s\n",__func__);
	info = i2c_get_clientdata(client);
	misc_deregister(&sensor_device);
	kfree(info);
	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{ SENSOR_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_i2c_driver = {
	.driver = {
		.name = SENSOR_NAME,
		.owner = THIS_MODULE,
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static int __init s5k6aa_sensor_init(void)
{
	pr_info("%s run\n",__func__);
	return i2c_add_driver(&sensor_i2c_driver);
}

static void __exit s5k6aa_sensor_exit(void)
{
	pr_info("%s run\n",__func__);
	i2c_del_driver(&sensor_i2c_driver);
}

module_init(s5k6aa_sensor_init);
module_exit(s5k6aa_sensor_exit);

