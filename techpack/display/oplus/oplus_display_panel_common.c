/***************************************************************
** Copyright (C),  2020,  OPLUS Mobile Comm Corp.,  Ltd
** File : oplus_display_panel_common.c
** Description : oplus display panel common feature
** Version : 1.0
** Date : 2020/06/13
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**  Li.Sheng       2020/06/13        1.0           Build this moudle
******************************************************************/
#include "oplus_display_panel_common.h"
#include <linux/notifier.h>
#include <linux/msm_drm_notify.h>
#include "oplus_display_private_api.h"
#include "oplus_display_panel.h"
#include "oplus_display_panel_seed.h"

int oplus_debug_max_brightness = 0;
#ifdef OPLUS_BUG_STABILITY
int oplus_dither_enable = 0;
int oplus_dre_status = 0;
#endif
EXPORT_SYMBOL(oplus_debug_max_brightness);
#ifdef OPLUS_BUG_STABILITY
EXPORT_SYMBOL(oplus_dither_enable);
#endif

uint64_t serial_number0 = 0x0;
uint64_t serial_number1 = 0x0;

extern int dsi_display_read_panel_reg(struct dsi_display *display, u8 cmd,
				      void *data, size_t len);
extern int oplus_display_audio_ready;
char oplus_rx_reg[PANEL_TX_MAX_BUF] = {0x0};
char serial_number_cache[30] = {0};
bool read_serial_num_done = false;
char oplus_rx_len = 0;
extern int lcd_closebl_flag;
extern int spr_mode;
extern int round_count_mode;
extern int dynamic_osc_clock;
int mca_mode = 1;
extern int oplus_dimlayer_hbm;
extern int oplus_dimlayer_bl;
extern int dither_enable;
extern int shutdown_flag;

enum {
	REG_WRITE = 0,
	REG_READ,
	REG_X,
};

extern int msm_drm_notifier_call_chain(unsigned long val, void *v);
extern int __oplus_display_set_spr(int mode);
extern int __oplus_display_set_dither(int mode);
extern int dsi_display_spr_mode(struct dsi_display *display, int mode);
extern int dsi_panel_spr_mode(struct dsi_panel *panel, int mode);
extern int dsi_panel_read_panel_reg(struct dsi_display_ctrl *ctrl,
			     struct dsi_panel *panel, u8 cmd, void *rbuf,  size_t len);

int oplus_display_panel_get_id(void *buf)
{
	struct dsi_display *display = get_main_display();
	int ret = 0;
	unsigned char read[30];
	struct panel_id *panel_rid = buf;
	int display_id = panel_rid->DA;

	if (get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON) {
		if (display == NULL) {
			printk(KERN_INFO "oplus_display_get_panel_id and main display is null");
			ret = -1;
			return ret;
		}

		if (0 == display_id && display->enabled == false) {
			pr_err("%s main panel is disabled", __func__);
			return -1;
		}

		if (1 == display_id) {
			display = get_sec_display();
			if (!display) {
				printk(KERN_INFO "oplus_display_panel_get_id and second display is null");
				return -1;
			}
			if (display->enabled == false) {
				pr_err("%s second panel is disabled", __func__);
				return -1;
			}
		}

		ret = dsi_display_read_panel_reg(display, 0xDA, read, 1);

		if (ret < 0) {
			pr_err("failed to read DA ret=%d\n", ret);
			return -EINVAL;
		}

		panel_rid->DA = (uint32_t)read[0];

		ret = dsi_display_read_panel_reg(display, 0xDB, read, 1);

		if (ret < 0) {
			pr_err("failed to read DB ret=%d\n", ret);
			return -EINVAL;
		}

		panel_rid->DB = (uint32_t)read[0];

		ret = dsi_display_read_panel_reg(display, 0xDC, read, 1);

		if (ret < 0) {
			pr_err("failed to read DC ret=%d\n", ret);
			return -EINVAL;
		}

		panel_rid->DC = (uint32_t)read[0];

	} else {
		printk(KERN_ERR	 "%s oplus_display_get_panel_id, but now display panel status is not on\n", __func__);
		return -EINVAL;
	}

	return ret;
}

int oplus_display_panel_get_max_brightness(void *buf)
{
	uint32_t *max_brightness = buf;
	int panel_id = (*max_brightness >> 12);
	struct dsi_display *display = get_main_display();
	if (panel_id == 1)
		display = get_sec_display();

	if (!display || !display->panel) {
		pr_err("%s failed to get display\n", __func__);
		return -EINVAL;
	}

	if (oplus_debug_max_brightness == 0) {
		(*max_brightness) = display->panel->bl_config.bl_normal_max_level;
	} else {
		(*max_brightness) = oplus_debug_max_brightness;
	}

	return 0;
}

int oplus_display_panel_set_max_brightness(void *buf)
{
	uint32_t *max_brightness = buf;

	oplus_debug_max_brightness = (*max_brightness);

	return 0;
}

int oplus_display_panel_get_oplus_max_brightness(void *buf)
{
	uint32_t *max_brightness = buf;
	int panel_id = (*max_brightness >> 12);
	struct dsi_display *display = get_main_display();
	if (panel_id == 1)
		display = get_sec_display();

	if (!display || !display->panel) {
		pr_err("%s failed to get display\n", __func__);
		return -EINVAL;
	}

	(*max_brightness) = display->panel->bl_config.bl_normal_max_level;

	return 0;
}

int oplus_display_panel_get_lcd_max_brightness(void *buf)
{
	uint32_t *lcd_max_backlight = buf;
	int panel_id = (*lcd_max_backlight >> 12);
	struct dsi_display *display = get_main_display();
	if (panel_id == 1)
		display = get_sec_display();

	(*lcd_max_backlight) = display->panel->bl_config.bl_max_level;

	DSI_INFO("[%s] get lcd max backlight: %d\n",
			display->panel->oplus_priv.vendor_name,
			*lcd_max_backlight);

	return 0;
}

int oplus_display_panel_get_brightness(void *buf)
{
	uint32_t *brightness = buf;
	int panel_id = (*brightness >> 12);
	struct dsi_display *display = get_main_display();
	if (panel_id == 1)
		display = get_sec_display();

	if (!display || !display->panel) {
		pr_err("%s failed to get display\n", __func__);
		return -EINVAL;
	}

	if(!strcmp(display->panel->oplus_priv.vendor_name, "AMS643YE01")) {
		(*brightness) = display->panel->bl_config.oplus_raw_bl;
	} else {
		(*brightness) = display->panel->bl_config.bl_level;
	}
	return 0;
}

int oplus_display_panel_set_brightness(void *buf)
{
	int rc = 0;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;
	uint32_t *backlight = buf;

	if (!display || !display->drm_conn || !display->panel) {
		DSI_ERR("Invalid display params\n");
		return -EINVAL;
	}
	panel = display->panel;

	if (*backlight > panel->bl_config.bl_max_level ||
			*backlight < 0) {
		DSI_WARN("[%s] falied to set backlight: %d, it is out of range!\n",
				__func__, *backlight);
		return -EFAULT;
	}

	DSI_INFO("[%s] set backlight: %d\n", panel->oplus_priv.vendor_name, *backlight);

	rc = dsi_display_set_backlight(display->drm_conn, display, *backlight);

	return rc;
}

int oplus_display_panel_get_vendor(void *buf)
{
	struct panel_info *p_info = buf;
	struct dsi_display *display = NULL;
	char *vendor = NULL;
	char *manu_name = NULL;
	int panel_id = p_info->version[0];

	display = get_main_display();
	if (1 == panel_id)
		display = get_sec_display();
	if (!display || !display->panel ||
	    !display->panel->oplus_priv.vendor_name ||
	    !display->panel->oplus_priv.manufacture_name) {
		pr_err("failed to config lcd proc device");
		return -EINVAL;
	}

	vendor = (char *)display->panel->oplus_priv.vendor_name;
	manu_name = (char *)display->panel->oplus_priv.manufacture_name;

	memcpy(p_info->version, vendor, strlen(vendor) >= 31?31:(strlen(vendor)+1));
	memcpy(p_info->manufacture, manu_name, strlen(manu_name) >= 31?31:(strlen(manu_name)+1));

	return 0;
}

int oplus_display_panel_get_panel_name(void *buf)
{
	struct panel_name *p_name = buf;
	struct dsi_display *display = NULL;
	char *name = NULL;
	int panel_id = p_name->name[0];

	display = get_main_display();
	if (1 == panel_id)
		display = get_sec_display();

	if (!display || !display->panel ||
			!display->panel->name) {
		pr_err("failed to config lcd panel name\n");
		return -EINVAL;
	}

#if defined(CONFIG_PXLW_IRIS)
	if (iris_is_chip_supported() && (!strcmp(display->panel->type, "secondary"))) {
		pr_info("iris secondary panel no need config\n");
		return 0;
	}
#endif

	name = (char *)display->panel->name;

	memcpy(p_name->name, name,
			strlen(name) >= (PANEL_NAME_LENS - 1) ? (PANEL_NAME_LENS - 1) : (strlen(name) + 1));

	return 0;
}

int oplus_display_panel_get_panel_bpp(void *buf)
{
	uint32_t *panel_bpp = buf;
	int bpp = 0;
	int rc = 0;
	int panel_id = (*panel_bpp >> PANEL_ID_BIT);
	struct dsi_display *display = get_main_display();
	struct dsi_parser_utils *utils = NULL;

	if (panel_id == 1)
		display = get_sec_display();

	if (!display || !display->panel) {
		pr_err("display or panel is null\n");
		return -EINVAL;
	}

#if defined(CONFIG_PXLW_IRIS)
	if (iris_is_chip_supported() && (!strcmp(display->panel->type, "secondary"))) {
		pr_info("iris secondary panel no need config\n");
		return 0;
	}
#endif

	utils = &display->panel->utils;
	if (!utils) {
		pr_err("utils is null\n");
		return -EINVAL;
	}

	rc = utils->read_u32(utils->data, "qcom,mdss-dsi-bpp", &bpp);
	if (rc) {
		pr_info("failed to read qcom,mdss-dsi-bpp, rc=%d\n", rc);
		return -EINVAL;
	}

	*panel_bpp = bpp / RGB_COLOR_WEIGHT;

	return 0;
}

int oplus_display_panel_get_ccd_check(void *buf)
{
	struct dsi_display *display = get_main_display();
	struct mipi_dsi_device *mipi_device;
	int rc = 0;
	unsigned int *ccd_check = buf;

	if (!display || !display->panel) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (get_oplus_display_power_status() != OPLUS_DISPLAY_POWER_ON) {
		printk(KERN_ERR"%s display panel in off status\n", __func__);
		return -EFAULT;
	}

	if (display->panel->panel_mode != DSI_OP_CMD_MODE) {
		pr_err("only supported for command mode\n");
		return -EFAULT;
	}

	if (true) {
		/* all new project does not have this function, ccd_check = 0 */
		(*ccd_check) = 0;
		goto end;
	}

	mipi_device = &display->panel->mipi_device;

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	rc = dsi_display_cmd_engine_enable(display);

	if (rc) {
		pr_err("%s, cmd engine enable failed\n", __func__);
		goto unlock;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				     DSI_CORE_CLK, DSI_CLK_ON);
	}

	if (!strcmp(display->panel->oplus_priv.vendor_name, "AMB655UV01")) {
		 {
			char value[] = { 0x5A, 0x5A };
			rc = mipi_dsi_dcs_write(mipi_device, 0xF0, value, sizeof(value));
		 }
		 {
			char value[] = { 0x44, 0x50 };
			rc = mipi_dsi_dcs_write(mipi_device, 0xE7, value, sizeof(value));
		 }
		usleep_range(1000, 1100);
		 {
			char value[] = { 0x03 };
			rc = mipi_dsi_dcs_write(mipi_device, 0xB0, value, sizeof(value));
		 }

	} else {
		 {
			char value[] = { 0x5A, 0x5A };
			rc = mipi_dsi_dcs_write(mipi_device, 0xF0, value, sizeof(value));
		 }
		 {
			char value[] = { 0x02 };
			rc = mipi_dsi_dcs_write(mipi_device, 0xB0, value, sizeof(value));
		 }
		 {
			char value[] = { 0x44, 0x50 };
			rc = mipi_dsi_dcs_write(mipi_device, 0xCC, value, sizeof(value));
		 }
		usleep_range(1000, 1100);
		 {
			char value[] = { 0x05 };
			rc = mipi_dsi_dcs_write(mipi_device, 0xB0, value, sizeof(value));
		 }
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
					  DSI_CORE_CLK, DSI_CLK_OFF);
	}

	dsi_display_cmd_engine_disable(display);

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	if (!strcmp(display->panel->oplus_priv.vendor_name, "AMB655UV01")) {
		 {
			unsigned char read[10];

			rc = dsi_display_read_panel_reg(display, 0xE1, read, 1);

			pr_err("read ccd_check value = 0x%x rc=%d\n", read[0], rc);
			(*ccd_check) = read[0];
		 }

	} else {
		 {
			unsigned char read[10];

			rc = dsi_display_read_panel_reg(display, 0xCC, read, 1);

			pr_err("read ccd_check value = 0x%x rc=%d\n", read[0], rc);
			(*ccd_check) = read[0];
		 }
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	rc = dsi_display_cmd_engine_enable(display);

	if (rc) {
		pr_err("%s, cmd engine enable failed\n", __func__);
		goto unlock;
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				     DSI_CORE_CLK, DSI_CLK_ON);
	}

	 {
		char value[] = { 0xA5, 0xA5 };
		rc = mipi_dsi_dcs_write(mipi_device, 0xF0, value, sizeof(value));
	 }

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
					  DSI_CORE_CLK, DSI_CLK_OFF);
	}

	dsi_display_cmd_engine_disable(display);
unlock:

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);
end:
	pr_err("[%s] ccd_check = %d\n",  display->panel->oplus_priv.vendor_name, (*ccd_check));
	return 0;
}

int oplus_display_panel_get_serial_number(void *buf) {
	int ret = 0;
	int read_index = 0;
	int len = 0;
	unsigned char read[30];
	unsigned char ret_val[1];
	PANEL_SERIAL_INFO panel_serial_info;
	struct panel_serial_number *panel_rnum = buf;
	uint64_t serial_number;
	struct dsi_display *display = get_main_display();
	int i;
	int number;
	int panel_id = panel_rnum->serial_number[0];

	pr_info("%s panel_id = %d\n", __func__, panel_id);
	if (!display || !display->panel) {
		printk(KERN_INFO
			"oplus_display_get_panel_serial_number and main display is null");
		return -1;
	}

	if (0 == panel_id && display->enabled == false) {
		pr_err("%s main panel is disabled", __func__);
		return -1;
	}

	if (1 == panel_id) {
		display = get_sec_display();
		if (!display) {
			printk(KERN_INFO "oplus_display_get_panel_serial_number and main display is null");
			return -1;
		}
		if (display->enabled == false) {
			pr_err("%s second panel is disabled", __func__);
			return -1;
		}
	}

	/*
	* To fix bug id 5552142, we do not read serial number frequently.
	* First read, then return the saved value.
	*/
	if (1 == panel_id) {
		if (serial_number1 != 0) {
			ret = scnprintf(panel_rnum->serial_number, PAGE_SIZE, "Get panel serial number: %llx\n",
							serial_number1);
			pr_info("%s read serial_number1 0x%llx\n", __func__, serial_number1);
			return ret;
		}
	} else {
		if (serial_number0 != 0) {
			ret = scnprintf(panel_rnum->serial_number, PAGE_SIZE, "Get panel serial number: %llx\n",
							serial_number0);
			pr_info("%s read serial_number0 0x%llx\n", __func__, serial_number0);
			return ret;
		}
	}

	/*
	 * for some unknown reason, the panel_serial_info may read dummy,
	 * retry when found panel_serial_info is abnormal.
	 */
	for (i = 0;i < 5; i++) {
		if (get_oplus_display_power_status() != OPLUS_DISPLAY_POWER_ON) {
			printk(KERN_ERR"%s display panel in off status\n", __func__);
			return ret;
		}

		if (!display->panel->panel_initialized) {
			printk(KERN_ERR"%s  panel initialized = false\n", __func__);
			return ret;
		}

		if(!strcmp(display->panel->oplus_priv.vendor_name, "S6E3XA1")
			|| !strcmp(display->panel->name, "samsung AMS643YE01 dsc cmd mode panel")
			|| !strcmp(display->panel->name, "samsung ams662zs01 dvt dsc cmd mode panel")) {
			mutex_lock(&display->display_lock);
			mutex_lock(&display->panel->panel_lock);

			if (display->config.panel_mode == DSI_OP_CMD_MODE) {
				dsi_display_clk_ctrl(display->dsi_clk_handle,
						DSI_ALL_CLKS, DSI_CLK_ON);
			}
			 {
				char value[] = { 0x5A, 0x5A };
				ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xF0, value, sizeof(value));
			 }
			if (display->config.panel_mode == DSI_OP_CMD_MODE) {
				dsi_display_clk_ctrl(display->dsi_clk_handle,
						DSI_ALL_CLKS, DSI_CLK_OFF);
			}

			mutex_unlock(&display->panel->panel_lock);
			mutex_unlock(&display->display_lock);

			if(ret < 0) {
				ret = scnprintf(buf, PAGE_SIZE,
						"Get panel serial number failed, reason:%d", ret);
				msleep(20);
				continue;
			}
			pr_info("%s read panel:%s\n", __func__, display->panel->oplus_priv.vendor_name);
			ret = dsi_display_read_panel_reg(display, 0xD8, read, 22);
		} else {
			if (!strcmp(display->panel->oplus_priv.vendor_name, "S6E3HC3")) {
				ret = dsi_display_read_panel_reg(display, 0xA1, read, 11);
			} else if (!strcmp(display->panel->oplus_priv.vendor_name, "NT37701")) {
				pr_info("%s read panel:%s\n", __func__, display->panel->oplus_priv.vendor_name);
				ret = dsi_display_read_panel_reg(display, 0xA3, read, 8);
			} else if (!strcmp(display->panel->name, "samsung amb655x fhd cmd mode dsc dsi panel")
				|| !strcmp(display->panel->name, "samsung SOFE03F dsc cmd mode panel")) {
				ret = dsi_display_read_panel_reg(display, 0xA1, read, 18);
			} else if (!strcmp(display->panel->oplus_priv.vendor_name, "NT37705")) {
				char panel_info_page[] = { 0x55, 0xAA, 0x52, 0x8, 0x1 };

				mutex_lock(&display->display_lock);
				mutex_lock(&display->panel->panel_lock);

				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_ON);
				}

				ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xF0, panel_info_page, sizeof(panel_info_page));
				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_OFF);
				}

				mutex_unlock(&display->panel->panel_lock);
				mutex_unlock(&display->display_lock);

				ret = dsi_display_read_panel_reg(display, 0xD7, read, 8);
			} else if (!strcmp(display->panel->oplus_priv.vendor_name, "A0004")) {
				char panel_date_page[] = { 0x08, 0x38, 0x1D };

				mutex_lock(&display->display_lock);
				mutex_lock(&display->panel->panel_lock);

				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_ON);
				}

				ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xFF, panel_date_page, sizeof(panel_date_page));
				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_OFF);
				}

				mutex_unlock(&display->panel->panel_lock);
				mutex_unlock(&display->display_lock);

				ret = dsi_display_read_panel_reg(display, 0x81, read, 7);

				panel_date_page[2] = 0x00;
				mutex_lock(&display->display_lock);
				mutex_lock(&display->panel->panel_lock);
				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_ON);
				}

				mipi_dsi_dcs_write(&display->panel->mipi_device, 0xFF, panel_date_page, sizeof(panel_date_page));
				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_OFF);
				}

				mutex_unlock(&display->panel->panel_lock);
				mutex_unlock(&display->display_lock);
			} else {
				if (display->panel->oplus_ser.is_switch_page) {
					len = sizeof(display->panel->oplus_ser.serial_number_multi_regs) - 1;
					for (read_index = 0; read_index < len; read_index++) {
						if(read_serial_num_done) {
							read[read_index] = serial_number_cache[read_index];
						} else {
							ret = dsi_display_read_panel_reg_switch_page(display, display->panel->oplus_ser.serial_number_multi_regs[read_index],
								ret_val, 1);
							read[read_index] = ret_val[0];
							serial_number_cache[read_index] = ret_val[0];
							if (ret < 0) {
								ret = scnprintf(buf, PAGE_SIZE,
									"Get panel serial number failed, reason:%d", ret);
								msleep(20);
								break;
							}
						}
					}
				} else
					ret = dsi_display_read_panel_reg(display, 0xA1, read, 17);
			}
		}
		if(ret < 0) {
			ret = scnprintf(buf, PAGE_SIZE,
					"Get panel serial number failed, reason:%d", ret);
			msleep(20);
			continue;
		}
		read_serial_num_done = true;

		/*  0xA1			   11th		12th	13th	14th	15th
		 *  HEX				0x32		0x0C	0x0B	0x29	0x37
		 *  Bit		   [D7:D4][D3:D0] [D5:D0] [D5:D0] [D5:D0] [D5:D0]
		 *  exp			  3	  2	   C	   B	   29	  37
		 *  Yyyy,mm,dd	  2014   2m	  12d	 11h	 41min   55sec
		 *  panel_rnum.data[24:21][20:16] [15:8]  [7:0]
		 *  panel_rnum:precise_time					  [31:24] [23:16] [reserved]
		*/
		if (!strcmp(display->panel->name, "samsung amb655xl08 amoled fhd+ panel")) {
			panel_serial_info.reg_index = 11;
		} else if (!strcmp(display->panel->name, "samsung ams643ye01 amoled fhd+ panel")
			|| !strcmp(display->panel->name, "samsung AMS643YE01 dsc cmd mode panel")
			|| !strcmp(display->panel->name, "samsung ams662zs01 dvt dsc cmd mode panel")) {
			panel_serial_info.reg_index = 7;
		} else if (!strcmp(display->panel->oplus_priv.vendor_name, "S6E3HC3")) {
			panel_serial_info.reg_index = 4;
		} else if (!strcmp(display->panel->oplus_priv.vendor_name, "NT37701")
			|| !strcmp(display->panel->oplus_priv.vendor_name, "NT37705")) {
			panel_serial_info.reg_index = 0;
		} else if (!strcmp(display->panel->oplus_priv.vendor_name, "A0004")) {
			panel_serial_info.reg_index = 0;
		} else if (!strcmp(display->panel->oplus_priv.vendor_name, "S6E3XA1")) {
			panel_serial_info.reg_index = 15;
		} else if (!strcmp(display->panel->name, "samsung amb655x fhd cmd mode dsc dsi panel")
			|| !strcmp(display->panel->name, "samsung SOFE03F dsc cmd mode panel")) {
			panel_serial_info.reg_index = 11;
		} else {
			 if (display->panel->oplus_ser.is_switch_page)
				panel_serial_info.reg_index = display->panel->oplus_ser.serial_number_index;
			 else
				panel_serial_info.reg_index = 10;
		}

		if (!strcmp(display->panel->name, "21075 ds ili7807s fhd tft lcd panel with dsc")) {
			number = (((read[panel_serial_info.reg_index] & 0xF0) << 20)
				| (read[panel_serial_info.reg_index] & 0x00));
			if (!read[panel_serial_info.reg_index]) {
				msleep(20);
				continue;
			}
			ret = scnprintf(panel_rnum->serial_number,
				PAGE_SIZE, "Get panel serial number: %llx\n", number);
			/*Save serial_number value.*/
			if (1 == panel_id) {
				serial_number1 = serial_number;
			} else {
				serial_number0 = serial_number;
			}
			break;
		} else {
			panel_serial_info.year		= (read[panel_serial_info.reg_index] & 0xF0) >> 0x4;
			if (!strcmp(display->panel->oplus_priv.vendor_name, "NT37701")) {
				if (strcmp(display->panel->name, "20085 boe nt37701 amoled fhd+ panel")) {
					panel_serial_info.year += 1;
				}
			} else if (!strcmp(display->panel->name, "AA254 p 3 A0004 dsc cmd mode panel")) {
				panel_serial_info.year += 11;
			}
			panel_serial_info.month		= read[panel_serial_info.reg_index]	& 0x0F;
			panel_serial_info.day		= read[panel_serial_info.reg_index + 1]	& 0x1F;
			panel_serial_info.hour		= read[panel_serial_info.reg_index + 2]	& 0x1F;
			panel_serial_info.minute	= read[panel_serial_info.reg_index + 3]	& 0x3F;
			panel_serial_info.second	= read[panel_serial_info.reg_index + 4]	& 0x3F;
			panel_serial_info.reserved[0] = read[panel_serial_info.reg_index + 5];
			panel_serial_info.reserved[1] = read[panel_serial_info.reg_index + 6];

			serial_number = (panel_serial_info.year		<< 56)\
				+ (panel_serial_info.month		<< 48)\
				+ (panel_serial_info.day		<< 40)\
				+ (panel_serial_info.hour		<< 32)\
				+ (panel_serial_info.minute	<< 24)\
				+ (panel_serial_info.second	<< 16)\
				+ (panel_serial_info.reserved[0] << 8)\
				+ (panel_serial_info.reserved[1]);

			if (!panel_serial_info.year) {
				/*
				* the panel we use always large than 2011, so
				* force retry when year is 2011
				*/
				msleep(20);
				continue;
			}
			ret = scnprintf(panel_rnum->serial_number, PAGE_SIZE, "Get panel serial number: %llx\n", serial_number);
			/*Save serial_number value.*/
			if (1 == panel_id) {
				serial_number1 = serial_number;
			} else {
				serial_number0 = serial_number;
			}
			break;
		}
	}

	return ret;
}


int oplus_display_panel_set_audio_ready(void *data) {
	uint32_t *audio_ready = data;

	oplus_display_audio_ready = (*audio_ready);
	printk("%s oplus_display_audio_ready = %d\n", __func__, oplus_display_audio_ready);

	return 0;
}

int oplus_display_panel_dump_info(void *data) {
	int ret = 0;
	struct dsi_display * temp_display;
	struct display_timing_info *timing_info = data;

	temp_display = get_main_display();

	if (temp_display == NULL) {
		printk(KERN_INFO "oplus_display_dump_info and main display is null");
		ret = -1;
		return ret;
	}

	if(temp_display->modes == NULL) {
		printk(KERN_INFO "oplus_display_dump_info and display modes is null");
		ret = -1;
		return ret;
	}

	timing_info->h_active = temp_display->modes->timing.h_active;
	timing_info->v_active = temp_display->modes->timing.v_active;
	timing_info->refresh_rate = temp_display->modes->timing.refresh_rate;
	timing_info->clk_rate_hz_l32 = (uint32_t)(temp_display->modes->timing.clk_rate_hz & 0x00000000FFFFFFFF);
	timing_info->clk_rate_hz_h32 = (uint32_t)(temp_display->modes->timing.clk_rate_hz >> 32);

	return 0;
}

int oplus_display_panel_get_dsc(void *data) {
	int ret = 0;
	uint32_t *reg_read = data;
	unsigned char read[30];

	if(get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON) {
		if(get_main_display() == NULL) {
			printk(KERN_INFO "oplus_display_get_panel_dsc and main display is null");
			ret = -1;
			return ret;
		}

		ret = dsi_display_read_panel_reg(get_main_display(), 0x03, read, 1);
		if (ret < 0) {
			printk(KERN_ERR  "%s read panel dsc reg error = %d!\n", __func__, ret);
			ret = -1;
		} else {
			(*reg_read) = read[0];
			ret = 0;
		}
	} else {
		printk(KERN_ERR	 "%s but now display panel status is not on\n", __func__);
		ret = -1;
	}

	return ret;
}

int oplus_display_panel_get_closebl_flag(void *data)
{
	uint32_t *closebl_flag = data;

	(*closebl_flag) = lcd_closebl_flag;
	printk(KERN_INFO "oplus_display_get_closebl_flag = %d\n", lcd_closebl_flag);

	return 0;
}

int oplus_display_panel_set_closebl_flag(void *data)
{
	uint32_t *closebl = data;

	pr_err("lcd_closebl_flag = %d\n", (*closebl));
	if (1 != (*closebl))
		lcd_closebl_flag = 0;
	pr_err("oplus_display_set_closebl_flag = %d\n", lcd_closebl_flag);

	return 0;
}
int oplus_big_endian_copy(void *dest, void *src, int count)
{
	int index = 0, knum = 0, rc = 0;
	uint32_t *u_dest = (uint32_t*) dest;
	char *u_src = (char*) src;

	if (dest == NULL || src == NULL) {
		printk("%s null pointer\n", __func__);
		return -EINVAL;
	}

	if (dest == src) {
		return rc;
	}

	while (count > 0) {
		u_dest[index] = ((u_src[knum] << 24) | (u_src[knum+1] << 16) | (u_src[knum+2] << 8) | u_src[knum+3]);
		index += 1;
		knum += 4;
		count = count - 1;
	}

	return rc;
}

int oplus_display_panel_get_reg(void *data)
{
	struct dsi_display *display = get_main_display();
	struct panel_reg_get *panel_reg = data;
	uint32_t u32_bytes = sizeof(uint32_t)/sizeof(char);

	if (!display) {
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	u32_bytes = oplus_rx_len%u32_bytes ? (oplus_rx_len/u32_bytes + 1) : oplus_rx_len/u32_bytes;
	oplus_big_endian_copy(panel_reg->reg_rw, oplus_rx_reg, u32_bytes);
	panel_reg->lens = oplus_rx_len;

	mutex_unlock(&display->display_lock);

	return 0;
}

int oplus_display_panel_set_reg(void *data)
{
	char reg[PANEL_TX_MAX_BUF] = {0x0};
	char payload[PANEL_TX_MAX_BUF] = {0x0};
	u32 index = 0, value = 0;
	int ret = 0;
	int len = 0;
	struct dsi_display *display = get_main_display();
	struct panel_reg_rw *reg_rw = data;

	if (!display || !display->panel) {
		pr_err("debug for: %s %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (reg_rw->lens > PANEL_REG_MAX_LENS) {
		pr_err("error: wrong input reg len\n");
		return -EINVAL;
	}

	if (reg_rw->rw_flags == REG_READ) {
		value = reg_rw->cmd;
		len = reg_rw->lens;
		dsi_display_read_panel_reg(get_main_display(), value, reg, len);

		for (index=0; index < len; index++) {
			printk("reg[%d] = %x ", index, reg[index]);
		}
		mutex_lock(&display->display_lock);
		memcpy(oplus_rx_reg, reg, PANEL_TX_MAX_BUF);
		oplus_rx_len = len;
		mutex_unlock(&display->display_lock);
		return 0;
	}

	if (reg_rw->rw_flags == REG_WRITE) {
		memcpy(payload, reg_rw->value, reg_rw->lens);
		reg[0] = reg_rw->cmd;
		len = reg_rw->lens;
		for (index=0; index < len; index++) {
			reg[index + 1] = payload[index];
		}

		if(get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON) {
				/* enable the clk vote for CMD mode panels */
			mutex_lock(&display->display_lock);
			mutex_lock(&display->panel->panel_lock);

			if (display->panel->panel_initialized) {
				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle,
							DSI_ALL_CLKS, DSI_CLK_ON);
				}
				ret = mipi_dsi_dcs_write(&display->panel->mipi_device, reg[0],
							 payload, len);

				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle,
							DSI_ALL_CLKS, DSI_CLK_OFF);
				}
			}

			mutex_unlock(&display->panel->panel_lock);
			mutex_unlock(&display->display_lock);

			if (ret < 0) {
				return ret;
			}
		}
		return 0;
	}
	printk("%s error: please check the args!\n", __func__);
	return -1;
}

int oplus_display_panel_notify_blank(void *data)
{
	struct msm_drm_notifier notifier_data;
	int blank;
	uint32_t *temp_save_user = data;
	int temp_save = (*temp_save_user);

	printk(KERN_INFO "%s oplus_display_notify_panel_blank = %d\n", __func__, temp_save);

	if(temp_save == 1) {
		blank = MSM_DRM_BLANK_UNBLANK;
		notifier_data.data = &blank;
		notifier_data.id = 0;
		msm_drm_notifier_call_chain(MSM_DRM_EARLY_EVENT_BLANK,
						   &notifier_data);
		msm_drm_notifier_call_chain(MSM_DRM_EVENT_BLANK,
						   &notifier_data);
	} else if (temp_save == 0) {
		blank = MSM_DRM_BLANK_POWERDOWN;
		notifier_data.data = &blank;
		notifier_data.id = 0;
		msm_drm_notifier_call_chain(MSM_DRM_EARLY_EVENT_BLANK,
						   &notifier_data);
	}
	return 0;
}

int oplus_display_panel_get_spr(void *data)
{
	uint32_t *spr_mode_user = data;

	printk(KERN_INFO "oplus_display_get_spr = %d\n", spr_mode);
	*spr_mode_user = spr_mode;

	return 0;
}

int oplus_display_panel_set_spr(void *data)
{
	uint32_t *temp_save_user = data;
	int temp_save = (*temp_save_user);

	printk(KERN_INFO "%s oplus_display_set_spr = %d\n", __func__, temp_save);

	__oplus_display_set_spr(temp_save);
	if(get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON) {
		if(get_main_display() == NULL) {
			printk(KERN_INFO "oplus_display_set_spr and main display is null");
			return 0;
		}

		dsi_display_spr_mode(get_main_display(), spr_mode);
	} else {
		printk(KERN_ERR	 "%s oplus_display_set_spr = %d, but now display panel status is not on\n", __func__, temp_save);
	}
	return 0;
}

int oplus_display_panel_get_dither(void *data)
{
	uint32_t *dither_mode_user = data;
	*dither_mode_user = dither_enable;
	printk(KERN_INFO "oplus_display_panel_get_dither = %d\n", dither_enable);

	return 0;
}

int oplus_display_panel_set_dither(void *data)
{
	uint32_t *temp_save_user = data;
	int temp_save = (*temp_save_user);
	__oplus_display_set_dither(temp_save);
	return 0;
}

int oplus_display_panel_get_roundcorner(void *data)
{
	uint32_t *round_corner = data;
	struct dsi_display *display = get_main_display();
	bool roundcorner = true;

	if (display && display->name &&
	    !strcmp(display->name, "qcom,mdss_dsi_oplus19101boe_nt37800_1080_2400_cmd"))
		roundcorner = false;

	*round_corner = roundcorner;

	return 0;
}

int oplus_display_panel_get_dynamic_osc_clock(void *data)
{
	struct dsi_display *display = get_main_display();
	uint32_t *osc_clock = data;

	if (!display) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	*osc_clock = dynamic_osc_clock;
	pr_debug("%s: read dsi clk rate %d\n", __func__,
			dynamic_osc_clock);

	mutex_unlock(&display->display_lock);

	return 0;
}

int oplus_display_panel_set_dynamic_osc_clock(void *data)
{
	struct dsi_display *display = get_main_display();
	uint32_t *osc_clk_user = data;
	int osc_clk = *osc_clk_user;
	int rc = 0;

	if (!display) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if(get_oplus_display_power_status() != OPLUS_DISPLAY_POWER_ON) {
		printk(KERN_ERR"%s display panel in off status\n", __func__);
		return -EFAULT;
	}

	if (display->panel->panel_mode != DSI_OP_CMD_MODE) {
		pr_err("only supported for command mode\n");
		return -EFAULT;
	}

	pr_info("%s: osc clk param value: '%d'\n", __func__, osc_clk);

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	if (osc_clk == 139600) {
		rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_OSC_CLK_MODEO0);
	} else {
		rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_OSC_CLK_MODEO1);
	}
	if (rc)
		pr_err("Failed to configure osc dynamic clk\n");

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}
	dynamic_osc_clock = osc_clk;

unlock:
	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return rc;
}

int oplus_display_get_softiris_color_status(void *data)
{
	struct softiris_color *iris_color_status = data;
	bool color_vivid_status = false;
	bool color_srgb_status = false;
	bool color_softiris_status = false;
	bool color_dual_panel_status = false;
	bool color_dual_brightness_status = false;
	struct dsi_parser_utils *utils = NULL;
	struct dsi_panel *panel = NULL;

	struct dsi_display *display = get_main_display();
	if (!display) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	panel = display->panel;
	if (!panel) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	utils = &panel->utils;
	if (!utils) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	color_vivid_status = utils->read_bool(utils->data, "oplus,color_vivid_status");
	DSI_INFO("oplus,color_vivid_status: %s", color_vivid_status ? "true" : "false");

	color_srgb_status = utils->read_bool(utils->data, "oplus,color_srgb_status");
	DSI_INFO("oplus,color_srgb_status: %s", color_srgb_status ? "true" : "false");

	color_softiris_status = utils->read_bool(utils->data, "oplus,color_softiris_status");
	DSI_INFO("oplus,color_softiris_status: %s", color_softiris_status ? "true" : "false");

	color_dual_panel_status = utils->read_bool(utils->data, "oplus,color_dual_panel_status");
	DSI_INFO("oplus,color_dual_panel_status: %s", color_dual_panel_status ? "true" : "false");

	color_dual_brightness_status = utils->read_bool(utils->data, "oplus,color_dual_brightness_status");
	DSI_INFO("oplus,color_dual_brightness_status: %s", color_dual_brightness_status ? "true" : "false");

	iris_color_status->color_vivid_status = (uint32_t)color_vivid_status;
	iris_color_status->color_srgb_status = (uint32_t)color_srgb_status;
	iris_color_status->color_softiris_status = (uint32_t)color_softiris_status;
	iris_color_status->color_dual_panel_status = (uint32_t)color_dual_panel_status;
	iris_color_status->color_dual_brightness_status = (uint32_t)color_dual_brightness_status;
	return 0;
}

int oplus_display_panel_get_id2(void)
{
	struct dsi_display *display = get_main_display();
	int ret = 0;
	unsigned char read[30];

	if(get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON) {
		if(display == NULL) {
			printk(KERN_INFO "oplus_display_get_panel_id and main display is null");
			return 0;
		}

		if ((!strcmp(display->panel->oplus_priv.vendor_name, "S6E3HC3"))
			|| (!strcmp(display->panel->oplus_priv.vendor_name, "AMB670YF01"))
			|| (!strcmp(display->panel->oplus_priv.vendor_name, "S6E3XA1"))) {
			ret = dsi_display_read_panel_reg(display, 0xDB, read, 1);
			if (ret < 0) {
				pr_err("failed to read DB ret=%d\n", ret);
				return -EINVAL;
			}
			ret = (int)read[0];
		}
	} else {
		printk(KERN_ERR	 "%s oplus_display_get_panel_id, but now display panel status is not on\n", __func__);
		return 0;
	}

	return ret;
}

int oplus_display_panel_hbm_lightspot_check(void)
{
	int rc = 0;
	char value[] = { 0xE0 };
	char value1[] = { 0x0F, 0xFF };
	struct dsi_display *display = get_main_display();
	struct mipi_dsi_device *mipi_device;

	if (!display || !display->panel) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (get_oplus_display_power_status() != OPLUS_DISPLAY_POWER_ON) {
		printk(KERN_ERR"%s display panel in off status\n", __func__);
		return -EFAULT;
	}

	mipi_device = &display->panel->mipi_device;

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		pr_err("%s, dsi_panel_initialized failed\n", __func__);
		rc = -EINVAL;
		goto unlock;
	}

	rc = dsi_display_cmd_engine_enable(display);

	if (rc) {
		pr_err("%s, cmd engine enable failed\n", __func__);
		rc = -EINVAL;
		goto unlock;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				     DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = mipi_dsi_dcs_write(mipi_device, 0x53, value, sizeof(value));
	usleep_range(1000, 1100);
	rc = mipi_dsi_dcs_write(mipi_device, 0x51, value1, sizeof(value1));
	usleep_range(1000, 1100);
	pr_err("[%s] hbm_lightspot_check successfully\n",  display->panel->oplus_priv.vendor_name);

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
					  DSI_CORE_CLK, DSI_CLK_OFF);
	}

	dsi_display_cmd_engine_disable(display);

unlock:

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);
	return 0;
}

#ifdef OPLUS_BUG_STABILITY
int oplus_display_get_dither_status(void *buf)
{
	uint32_t *dither_enable = buf;
	*dither_enable = oplus_dither_enable;

	return 0;
}

int oplus_display_set_dither_status(void *buf)
{
	uint32_t *dither_enable = buf;
	oplus_dither_enable = *dither_enable;
	pr_err("debug for %s, buf = [%s], oplus_dither_enable = %d\n",
	       __func__, buf, oplus_dither_enable);

	return 0;
}
#endif

int oplus_display_panel_opec_control(bool enable)
{
	int rc = 0;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = display->panel;

	if (!display || !panel) {
		DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (enable) {
		if (!strcmp(panel->oplus_priv.vendor_name, "AMB670YF01")) {
			if (panel->panel_id2 == 0x07 || panel->panel_id2 == 0x08) {
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_OPEC_ON_ID7);
				if (rc) {
					DSI_ERR("[%s] failed to send DSI_CMD_SET_OPEC_ON_ID7 cmds, rc=%d\n",
						panel->name, rc);
				}
			} else if (panel->panel_id2 >= 0x09 && panel->panel_id2 != 0x15) {
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_OPEC_ON);
				if (rc) {
					DSI_ERR("[%s] failed to send DSI_CMD_SET_OPEC_ON cmds, rc=%d\n",
						panel->name, rc);
				}
			}
		}
	}

	return rc;
}

void oplus_display_panel_enable(void)
{
	bool opec_enable = true;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = display->panel;

	if (!display || !panel) {
		DSI_ERR("Invalid params\n");
		return;
	}

	if (!strcmp(panel->oplus_priv.vendor_name, "AMB670YF01")) {
		oplus_display_panel_opec_control(opec_enable);
		dsi_panel_loading_effect_mode_unlock(panel, PANEL_LOADING_EFFECT_OFF);
	}

	return;
}

int oplus_display_get_dp_support(void *buf)
{
	struct dsi_display *display = NULL;
	struct dsi_panel *d_panel = NULL;
	uint32_t *dp_support = buf;

	display = get_main_display();
	if (!display) {
		printk(KERN_INFO "oplus_display_get_dp_support error get main display is null");
		return -EINVAL;
	}

	d_panel = display->panel;
	if (!d_panel) {
		printk(KERN_INFO "oplus_display_get_dp_support error get main panel is null");
		return -EINVAL;
	}

	if ((!strcmp(d_panel->oplus_priv.vendor_name, "S6E3HC3"))
		|| (!strcmp(d_panel->oplus_priv.vendor_name, "AMB670YF01"))
		|| (!strcmp(d_panel->oplus_priv.vendor_name, "NT37701"))) {
		*dp_support = true;
	} else {
		*dp_support = false;
	}

	return 0;
}

int oplus_display_get_panel_round_corner(void *data)
{
	uint32_t *temp_save_user = data;
	printk(KERN_INFO "oplus_display_get_panel_round_corner = %d\n", round_count_mode);
	*temp_save_user = round_count_mode;

	return 0;
}

int oplus_display_set_panel_round_corner(void *data)
{
	uint32_t *temp_save_user = data;
	struct dsi_display *display = get_main_display();
	round_count_mode = *temp_save_user;

	if (!display || !display->panel) {
		pr_err("failed for: %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	printk(KERN_INFO "%s set_round_corner = %d\n", __func__, round_count_mode);

	/* if(get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON) { */
	if (display->panel->power_mode == SDE_MODE_DPMS_ON) {
		dsi_display_round_corner_mode(display, round_count_mode);
	} else {
		printk(KERN_ERR	 "%s set_round_corner = %d, but now display panel status is not on\n", __func__, round_count_mode);
	}
	return 0;
}

/*  drm_debug message:
   "Bit 0 (0x01)  will enable CORE messages (drm core code)\n"
   "Bit 1 (0x02)  will enable DRIVER messages (drm controller code)\n"
   "Bit 2 (0x04)  will enable KMS messages (modesetting code)\n"
   "Bit 3 (0x08)  will enable PRIME messages (prime code)\n"
   "Bit 4 (0x10)  will enable ATOMIC messages (atomic code)\n"
   "Bit 5 (0x20)  will enable VBL messages (vblank code)\n"
   "Bit 7 (0x80)  will enable LEASE messages (leasing code)\n"
   "Bit 8 (0x100) will enable DP messages (displayport code)"); */
extern unsigned int drm_debug;
extern int oplus_dsi_log_type;

int oplus_display_set_qcom_loglevel(void *data)
{
	struct kernel_loglevel *k_loginfo = data;
	if (k_loginfo == NULL) {
		printk(KERN_ERR "%s null args points\n", __func__);
		return -1;
	}

	if (k_loginfo->enable) {
		if (k_loginfo->log_level < 0x1F) {
			oplus_dsi_log_type |= OPLUS_DEBUG_LOG_BACKLIGHT;
		} else {
			drm_debug = 0x1F;
		}
	} else {
		drm_debug = 0x0;
		oplus_dsi_log_type &= ~OPLUS_DEBUG_LOG_BACKLIGHT;
	}

	printk("%s drm_debug level = 0x%2x\n", __func__, drm_debug);
	return 0;
}

int oplus_display_get_cabc_status(void *buf)
{
	uint32_t *cabc_status = buf;
	struct dsi_display *display = NULL;
	struct dsi_panel *panel = NULL;

	display = get_main_display();
	if (!display) {
		DSI_ERR("No display device\n");
		return -ENODEV;
	}

	panel = display->panel;
	if (!panel) {
		DSI_ERR("No panel device\n");
		return -ENODEV;
	}

	if(!panel->oplus_priv.cabc_enabled) {
		DSI_ERR("Get cabc failed, due to this project don't support cabc\n");
		return -EFAULT;
	}

	*cabc_status = panel->oplus_priv.cabc_status;

	return 0;
}

int oplus_display_set_cabc_status(void *buf)
{
	int rc = 0;
	uint32_t *cabc_status = buf;
	struct dsi_display *display = NULL;
	struct dsi_panel *panel = NULL;
	static int cabc_lock_flag = 0;
	static uint32_t cabc_status_backup = 1;

	display = get_main_display();
	if (!display) {
		DSI_ERR("[CABC] No display device\n");
		return -ENODEV;
	}

	panel = display->panel;
	if (!panel) {
		DSI_ERR("[CABC] No panel device\n");
		return -ENODEV;
	}

	if(!panel->oplus_priv.cabc_enabled) {
		DSI_ERR("[CABC] Set cabc failed, due to this project don't support cabc\n");
		return -EFAULT;
	}

	if (*cabc_status >= OPLUS_DISPLAY_CABC_UNKNOW) {
		DSI_ERR("[CABC] Unknow cabc status = [%d]\n", *cabc_status);
		return -EINVAL;
	}

	switch (*cabc_status) {
	case OPLUS_DISPLAY_CABC_UNLOCK:
		cabc_lock_flag = 0;
		DSI_INFO("[CABC] Unlock cabc status = [%d]\n", *cabc_status);
		break;
	case OPLUS_DISPLAY_CABC_LOCK:
		cabc_lock_flag = 1;
		DSI_INFO("[CABC] Lock cabc status = [%d]\n", *cabc_status);
		break;
	default:
		cabc_status_backup = *cabc_status;
		break;
	}

	if (cabc_lock_flag) {
		DSI_WARN("[CABC] Locked! cabc_status_backup = [%d]\n", cabc_status_backup);
		return -EFAULT;
	}

	if(get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON && 0 != panel->bl_config.bl_level) {
		mutex_lock(&display->panel->panel_lock);
		switch (cabc_status_backup) {
		case OPLUS_DISPLAY_CABC_OFF:
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_CABC_OFF);
			if (rc) {
				DSI_ERR("[CABC] [%s] failed to send DSI_CMD_CABC_OFF cmds, rc=%d\n",
							panel->name, rc);
			}
			break;
		case OPLUS_DISPLAY_CABC_UI:
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_CABC_UI);
			if (rc) {
				DSI_ERR("[CABC] [%s] failed to send DSI_CMD_CABC_UI cmds, rc=%d\n",
						panel->name, rc);
			}
			break;
		case OPLUS_DISPLAY_CABC_IMAGE:
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_CABC_IMAGE);
			if (rc) {
				DSI_ERR("[CABC] [%s] failed to send DSI_CMD_CABC_IMAGE cmds, rc=%d\n",
						panel->name, rc);
			}
			break;
		case OPLUS_DISPLAY_CABC_VIDEO:
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_CABC_VIDEO);
			if (rc) {
				DSI_ERR("[CABC] [%s] failed to send DSI_CMD_CABC_VIDEO cmds, rc=%d\n",
						panel->name, rc);
			}
			break;
		default:
			DSI_ERR("[CABC] Unknow cabc status = [%d]\n", cabc_status_backup);
			rc = -EINVAL;
			break;
		}
		panel->oplus_priv.cabc_status = cabc_status_backup;
		DSI_INFO("[CABC] buf = [%s], oplus_priv.cabc_status = [%d]\n",
				buf, panel->oplus_priv.cabc_status);
		mutex_unlock(&display->panel->panel_lock);
	} else {
		DSI_WARN("[CABC] buf = [%s], oplus_priv.cabc_status = [%d]. Skiped because of display panel status is not on!\n",
				buf, panel->oplus_priv.cabc_status);
	}
	return rc;
}

int oplus_display_get_dre_status(void *buf)
{
	uint32_t *dre_status = buf;
	struct dsi_display *display = NULL;
	struct dsi_panel *panel = NULL;

	display = get_main_display();
	if (!display) {
		DSI_ERR("No display device\n");
		return -ENODEV;
	}

	panel = display->panel;
	if (!panel) {
		DSI_ERR("No panel device\n");
		return -ENODEV;
	}

	if(panel->oplus_priv.dre_enabled) {
		*dre_status = oplus_dre_status;
	} else {
		*dre_status = OPLUS_DISPLAY_DRE_OFF;
	}
	return 0;
}

int oplus_display_set_dre_status(void *buf)
{
	int rc = 0;
	uint32_t *dre_status = buf;
	struct dsi_display *display = NULL;
	struct dsi_panel *panel = NULL;

	display = get_main_display();
	if (!display) {
		DSI_ERR("No display device\n");
		return -ENODEV;
	}

	panel = display->panel;
	if (!panel) {
		DSI_ERR("No panel device\n");
		return -ENODEV;
	}

	if(!panel->oplus_priv.dre_enabled) {
		DSI_ERR("This project don't support dre\n");
		return -EFAULT;
	}

	if (*dre_status >= OPLUS_DISPLAY_DRE_UNKNOW) {
		DSI_ERR("Unknow DRE status = [%d]\n", *dre_status);
		return -EINVAL;
	}

	if(get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON) {
		if (*dre_status == OPLUS_DISPLAY_DRE_ON) {
			/* if(mtk)  */
			/*	disp_aal_set_dre_en(0);   MTK AAL api */
		} else {
			/* if(mtk) */
			/*	disp_aal_set_dre_en(1);  MTK AAL api */
		}
		oplus_dre_status = *dre_status;
		pr_err("debug for %s, buf = [%s], oplus_dre_status = %d\n",
				__func__, buf, oplus_dre_status);
	} else {
		pr_err("debug for %s, buf = [%s], but display panel status is not on!\n",
				__func__, *dre_status);
	}
	return rc;
}

int oplus_display_set_shutdown_flag(void *buf)
{
	shutdown_flag = 1;
	pr_err("debug for %s, buf = [%s], shutdown_flag = %d\n",
			__func__, buf, shutdown_flag);

	return 0;
}

/* Apollo DC backlight */
int oplus_display_panel_set_dc_real_brightness(void *data)
{
	struct dsi_display *display = get_main_display();
	uint32_t *temp_save = data;
	int rc = 0;

	if (!display || !display->panel) {
		pr_err("%s: display or display->panel is null\n", __func__);
		return -EINVAL;
	}

	display->panel->bl_config.bl_dc_real = *temp_save;

	return rc;
}

