/*
 * aat3174.c - aat3174 flash/torch kernel driver
 *
 * Copyright (C) 2011 MALATA Corp.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <media/aat3174.h>

struct aat3174_info {
	struct miscdevice *misc_dev;
	struct aat3174_platform_data *pdata;
	int open_cnt;
	int need_flash;
	int flash_torch; /* 0: flash, 1: torch. */
	int flash_level;
	int torch_level;
	int is_flashing;
	int is_torching;
	enum flash_mode_sel mode;
};

static struct aat3174_info *info;

static long aat3174_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	u8 level;
	struct aat3174_platform_data *pdata;
	pdata = (struct aat3174_platform_data *)(file->private_data);

	switch (cmd) {
	case AAT3174_IOCTL_MODE_SHUTDOWN:
		info->flash_level = 0;
		info->torch_level = 0;
		info->is_torching = 0;
		info->is_flashing = 0;
		gpio_direction_output(pdata->en_pin, 0);
		gpio_direction_output(pdata->rset_pin, 0);
		return 0;

	case AAT3174_IOCTL_MODE_STANDBY:
		info->flash_level = 0;
		info->torch_level = 0;
		info->is_torching = 0;
		info->is_flashing = 0;
		gpio_direction_output(pdata->en_pin, 0);
		gpio_direction_output(pdata->rset_pin, 0);
		return 0;

	case AAT3174_IOCTL_MODE_TORCH:
		if ((void *)arg != NULL) {
			if (copy_from_user(&(level), (const void __user *)arg,
							sizeof(level))) {
				printk(KERN_ERR"%s:copy_from_user failed.\n",
								__func__);
				return -EFAULT;
			}
			if ((info->is_torching) && (level == info->torch_level))
				break;
			info->torch_level = level;
		}
		if (info->need_flash != 0) {
			if (info->torch_level > 0) {
				gpio_direction_output(pdata->en_pin, 1);
				gpio_direction_output(pdata->rset_pin, 1);
				gpio_direction_output(pdata->en_pin, 1);
			} else {
				gpio_direction_output(pdata->en_pin, 0);
				gpio_direction_output(pdata->rset_pin, 0);
			}
		} else {
			gpio_direction_output(pdata->en_pin, 0);
			gpio_direction_output(pdata->rset_pin, 0);
		}
		return 0;

	case AAT3174_IOCTL_MODE_FLASH:
		if ((void *)arg != NULL) {
			if (copy_from_user(&(level), (const void __user *)arg,
							sizeof(level))) {
				printk(KERN_ERR"%s:copy_from_user failed.\n",
								__func__);
				return -EFAULT;
			}
			if ((info->is_flashing) && (level == info->flash_level))
				break;
			info->flash_level = level;
		}
		if (info->need_flash != 0) {
			if (info->flash_level > 0) {
				gpio_direction_output(pdata->en_pin, 1);
				gpio_direction_output(pdata->rset_pin, 1);
				gpio_direction_output(pdata->en_pin, 1);
				info->is_flashing = 1;
			} else {
				gpio_direction_output(pdata->en_pin, 0);
				gpio_direction_output(pdata->rset_pin, 0);
				info->is_flashing = 0;
			}
		} else {
			gpio_direction_output(pdata->en_pin, 0);
			gpio_direction_output(pdata->rset_pin, 0);
			info->is_flashing = 0;
		}
		return 0;

	case AAT3174_IOCTL_MODE_LED:
		gpio_direction_output(pdata->en_pin, 1);
		gpio_direction_output(pdata->rset_pin, 0);
		gpio_direction_output(pdata->en_pin, 1);
		return 0;

	case AAT3174_IOCTL_STRB:
		return 0;

	case AAT3174_IOCTL_SET_FLASH_MODE:
		if ((void *)arg == NULL) {
			printk(KERN_ERR"%s:AAT3174_IOCTL_SET_FLASH_MODE invalid"
						" argument.\n", __func__);
			return -EINVAL;
		}
		if (copy_from_user(&(info->mode), (const void __user *)arg,
						sizeof(enum flash_mode_sel))) {
			printk(KERN_ERR"%s:copy_from_user failed.\n", __func__);
			return -EFAULT;
		}
		return 0;

	case AAT3174_IOCTL_SET_FLASH_NECESSITY:
		if ((void *)arg == NULL) {
			printk(KERN_ERR"%s:AAT3174_IOCTL_SET_FLASH_NECESSITY "
					"invalid argument.\n", __func__);
			return -EINVAL;
		}
		if (copy_from_user(&(info->need_flash),
					(const void __user *)arg,
						sizeof(int))) {
			printk(KERN_ERR"%s:copy_from_user failed.\n", __func__);
			return -EFAULT;
		}
		return 0;

	case AAT3174_IOCTL_SET_FLASH_TORCH:
		if ((void *)arg == NULL) {
			printk(KERN_ERR"%s:AAT3174_IOCTL_SET_FLASH_TORCH "
					"invalid argument.\n", __func__);
			return -EINVAL;
		}
		if (copy_from_user(&(info->flash_torch),
					(const void __user *)arg,
						sizeof(__u32))) {
			printk(KERN_ERR"%s:copy_from_user failed.\n", __func__);
			return -EFAULT;
		}
		return 0;

	case AAT3174_IOCTL_TIMER:
		return 0;
	default:
		return -1;
	}

	return 0;
}

static int aat3174_open(struct inode *inode, struct file *file)
{
	struct aat3174_platform_data *pdata;
	pdata = info->pdata;
	file->private_data = pdata;

	info->open_cnt ++;
	if (info->open_cnt == 1) {
		gpio_direction_output(pdata->en_pin, 0);
		gpio_direction_output(pdata->rset_pin, 0);
		info->need_flash = 0;
		info->flash_level = 0;
		info->torch_level = 0;
		info->is_torching = 0;
		info->is_flashing = 0;
	}

	return 0;
}

int aat3174_release(struct inode *inode, struct file *file)
{
	struct aat3174_platform_data *pdata;
	pdata = (struct aat3174_platform_data *)(file->private_data);
	gpio_direction_output(pdata->en_pin, 0);
	gpio_direction_output(pdata->rset_pin, 0);
	tegra_gpio_enable(pdata->en_pin);
	tegra_gpio_enable(pdata->rset_pin);

	info->open_cnt -- ;
	if (0 == info->open_cnt) {
		gpio_direction_output(pdata->en_pin, 0);
		gpio_direction_output(pdata->rset_pin, 0);
		info->need_flash = 0;
		info->flash_level = 0;
		info->torch_level = 0;
		info->is_torching = 0;
		info->is_flashing = 0;
	}
	file->private_data = NULL;

	return 0;
}

static const struct file_operations aat3174_fileops = {
	.owner = THIS_MODULE,
	.open = aat3174_open,
	.unlocked_ioctl = aat3174_ioctl,
	.release = aat3174_release,
};

static struct miscdevice aat3174_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "aat3174",
	.fops = &aat3174_fileops,
};

static int __devinit aat3174_probe(struct platform_device *pdev)
{
	int err;
	struct aat3174_platform_data *pdata = pdev->dev.platform_data;

	printk(KERN_INFO"%s run!\n", __func__);

	if (!pdata) {
		printk(KERN_ERR"aat3174: Unable to get pdata!\n");
		return -ENOMEM;
	}
	gpio_direction_output(pdata->en_pin, 0);
	gpio_direction_output(pdata->rset_pin, 0);
	tegra_gpio_enable(pdata->en_pin);
	tegra_gpio_enable(pdata->rset_pin);

	info = kzalloc(sizeof(struct aat3174_info), GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
		goto fail;
	}
	info->pdata = pdata;
	info->misc_dev = &aat3174_device;
	err = misc_register(info->misc_dev);
	if (err) {
		printk(KERN_ERR"aat3174: Unable to register misc device!\n");
		goto fail_misc;
	}
	info->open_cnt = 0;
	info->mode = ImagerFlashMode_Invalid;
	info->need_flash = 0;
	info->flash_level = 3;
	info->flash_torch = 1;

	return 0;
fail_misc:
	kfree(info);
fail:
	return err;
}

static int __devexit aat3174_remove(struct platform_device *pdev)
{
	struct aat3174_platform_data *pdata = pdev->dev.platform_data;

	printk(KERN_INFO"%s run!", __func__);
	gpio_direction_output(pdata->en_pin, 0);
	gpio_direction_output(pdata->rset_pin, 0);
	tegra_gpio_disable(pdata->en_pin);
	tegra_gpio_disable(pdata->rset_pin);

	misc_deregister(info->misc_dev);
	info->open_cnt --;
	kfree(info);

	return 0;
}

static struct platform_driver aat3174_driver = {
	.probe	= aat3174_probe,
	.remove	= __devexit_p(aat3174_remove),
	.driver	= {
		.name	= "aat3174",
		.owner	= THIS_MODULE,
	}
};

static int __init aat3174_init(void)
{
	printk(KERN_INFO"%s run!\n", __func__);
	return platform_driver_register(&aat3174_driver);
}

static void __exit aat3174_exit(void)
{
	printk(KERN_INFO"%s run!\n", __func__);
	platform_driver_unregister(&aat3174_driver);
}

module_init(aat3174_init);
module_exit(aat3174_exit);
MODULE_DESCRIPTION("Charger Pump AAT3174 driver for Camera Flash");
