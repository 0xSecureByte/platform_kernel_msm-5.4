/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#ifndef __ILITEK_H
#define __ILITEK_H

#include "../ilitek_common.h"

#include <linux/version.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/wait.h>
#include <linux/vmalloc.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/gpio.h>

#ifdef TPD_DEVICE
#undef TPD_DEVICE
#define TPD_DEVICE "ilitek,ili9881h"
#else
#define TPD_DEVICE "ilitek,ili9881h"
#endif


#define DRIVER_VERSION            "2.4.0.0"

/* Options */
#define TDDI_INTERFACE            BUS_SPI /* BUS_I2C(0x18) or BUS_SPI(0x1C) */
#define VDD_VOLTAGE               1800000
#define VCC_VOLTAGE               1800000
#define SPI_CLK                   12000000
#define SPI_RETRY                 5
#define WQ_ESD_DELAY              2000
#define WQ_BAT_DELAY              2000
#define MP_TMEOUT                 3000
#define MT_B_TYPE                 ENABLE
#define TDDI_RST_BIND             DISABLE
#define MT_PRESSURE               DISABLE
#define ENABLE_WQ_ESD             DISABLE
#define ENABLE_WQ_BAT             DISABLE
#define ENABLE_GESTURE            DISABLE
#define REGULATOR_POWER           DISABLE
#define TP_SUSPEND_PRIO           ENABLE
#define DEBUG_OUTPUT              1 /* DEBUG_ALL or DEBUG_NONE */

/* These is define for time*/
#define RST_EDGE_DELAY              5  /* 5ms*/

/* Plaform compatibility */
/* #define CONFIG_PLAT_SPRD*/

/* Path */
#define DEBUG_DATA_FILE_SIZE        (10*K)
#define DEBUG_DATA_FILE_PATH        "/sdcard/ILITEK_log.csv"
#define INI_NAME_PATH               "/sdcard/mp.ini"
#define UPDATE_FW_PATH              "/sdcard/ILITEK_FW"
#define POWER_STATUS_PATH           "/sys/class/power_supply/battery/status"
#define DUMP_FLASH_PATH             "/sdcard/flash_dump"
#define DUMP_IRAM_PATH              "/sdcard/iram_dump"

/* Debug messages */
#define USER_STR_BUFF        PAGE_SIZE

#ifdef BIT
#undef BIT
#endif
#define BIT(x)    (1 << (x))

#define ERR_ALLOC_MEM(X)    ((IS_ERR(X) || X == NULL) ? 1 : 0)
#define K            (1024)
#define M            (K * K)
#define ENABLE            1
#define START            1
#define ON            1
#define ILI_WRITE        1
#define ILI_READ        0
#define DISABLE            0
#define END            0
#define OFF            0
#define NONE            -1
#define DO_SPI_RECOVER        -2

enum TP_PLAT_TYPE {
	TP_PLAT_MTK = 0,
	TP_PLAT_QCOM
};

enum TP_RST_METHOD {
	TP_IC_WHOLE_RST = 0,
	TP_IC_CODE_RST,
	TP_HW_RST_ONLY,
};

#if (TDDI_RST_BIND)
#define ILITEK_RESET_METHOD TP_IC_WHOLE_RST
#else
#define ILITEK_RESET_METHOD TP_HW_RST_ONLY
#endif


enum TP_FW_UPGRADE_TYPE {
	UPGRADE_FLASH = 0,
	UPGRADE_IRAM
};

enum TP_FW_UPGRADE_TARGET {
	ILI_FILE = 0,
	HEX_FILE
};

enum TP_FW_OPEN_METHOD {
	REQUEST_FIRMWARE = 0,
	FILP_OPEN
};
#define IITEK_FW_OPEN FILP_OPEN


enum TP_SLEEP_STATUS {
	TP_SUSPEND = 0,
	TP_DEEP_SLEEP = 1,
	TP_RESUME = 2
};

enum TP_SLEEP_CTRL {
	SLEEP_IN = 0x0,
	SLEEP_OUT = 0x1,
	DEEP_SLEEP_IN = 0x3,
	SLEEP_IN_FTM_BEGIN = 0x04,
	SLEEP_IN_FTM_END = 0x05,
	NOT_SLEEP_MODE
};

enum TP_FW_BLOCK_NUM {
	AP = 1,
	DATA = 2,
	TUNING = 3,
	GESTURE = 4,
	MP = 5,
	DDI = 6
};

enum TP_FW_BLOCK_TAG {
	BLOCK_TAG_AE = 0xAE,
	BLOCK_TAG_AF = 0xAF,
	BLOCK_TAG_B0 = 0xB0
};

enum ili_model_id {
	INX_MODULE = 0,
	AUO_MODULE = 1,
	TM_MODULE = 2,
};


#define TDDI_I2C_ADDR              0x41
#define TDDI_DEV_ID                "ILITEK_TDDI"

/* define the width and heigth of a screen. */
#define TOUCH_SCREEN_X_MIN            0
#define TOUCH_SCREEN_Y_MIN            0
#define TOUCH_SCREEN_X_MAX            720
#define TOUCH_SCREEN_Y_MAX            1520
#define MAX_TOUCH_NUM                 10

/* define the range on panel */
#define TPD_HEIGHT                    2048
#define TPD_WIDTH                     2048

/* Firmware upgrade */
#define MAX_FW_BUF_SIZE                    (128*K)
#define MAX_HEX_FILE_SIZE                  (256*K)
#define MAX_FLASH_FIRMWARE_SIZE            (256*K)
#define MAX_IRAM_FIRMWARE_SIZE             (60*K)
#define ILI_FILE_HEADER                     64
#define MAX_AP_FIRMWARE_SIZE               (64*K)
#define MAX_DLM_FIRMWARE_SIZE              (8*K)
#define MAX_MP_FIRMWARE_SIZE               (64*K)
#define MAX_GESTURE_FIRMWARE_SIZE          (8*K)
#define MAX_TUNING_FIRMWARE_SIZE           (4*K)
#define MAX_DDI_FIRMWARE_SIZE              (4*K)
#define DLM_START_ADDRESS                  0x20610
#define DLM_HEX_ADDRESS                    0x10000
#define MP_HEX_ADDRESS                     0x13000
#define RESERVE_BLOCK_START_ADDR           0x1D000
#define RESERVE_BLOCK_END_ADDR             0x1DFFF
#define FW_VER_ADDR                        0xFFE0
#define SPI_UPGRADE_LEN                    2048
#define SPI_READ_LEN                       2048
#define FW_BLOCK_INFO_NUM                  7
#define TIMING_INFO_STR_SIZE               (256)
#define TIMING_INFO_INFO_SIZE              (64)


/* The example for the gesture virtual keys */
#define GESTURE_DOUBLECLICK                0x58
#define GESTURE_UP                         0x60
#define GESTURE_DOWN                       0x61
#define GESTURE_LEFT                       0x62
#define GESTURE_RIGHT                      0x63
#define GESTURE_M                          0x64
#define GESTURE_W                          0x65
#define GESTURE_C                          0x66
#define GESTURE_E                          0x67
#define GESTURE_V                          0x68
#define GESTURE_O                          0x69
#define GESTURE_S                          0x6A
#define GESTURE_Z                          0x6B
#define GESTURE_V_DOWN                     0x6C
#define GESTURE_V_LEFT                     0x6D
#define GESTURE_V_RIGHT                    0x6E
#define GESTURE_TWOLINE_DOWN               0x6F
#define GESTURE_F                          0x70
#define GESTURE_AT                         0x71

#define ESD_GESTURE_PWD             0xF38A94EF
#define SPI_ESD_GESTURE_RUN         0x5B92E7F4
#define I2C_ESD_GESTURE_RUN         0xA67C9DFE
#define SPI_ESD_GESTURE_PWD_ADDR    0x25FF8
#define I2C_ESD_GESTURE_PWD_ADDR    0x40054

/* Protocol */
#define PROTOCOL_VER_500           0x050000
#define PROTOCOL_VER_510           0x050100
#define PROTOCOL_VER_520           0x050200
#define PROTOCOL_VER_530           0x050300
#define PROTOCOL_VER_540           0x050400
#define PROTOCOL_VER_550           0x050500
#define PROTOCOL_VER_560           0x050600
#define P5_X_READ_DATA_CTRL        0xF6
#define P5_X_GET_TP_INFORMATION    0x20
#define P5_X_GET_KEY_INFORMATION   0x27
#define P5_X_GET_PANEL_INFORMATION 0x29
#define P5_X_GET_FW_VERSION        0x21
#define P5_X_GET_PROTOCOL_VERSION  0x22
#define P5_X_GET_CORE_VERSION      0x23
#define P5_X_MODE_CONTROL          0xF0
#define P5_X_SET_CDC_INIT          0xF1
#define P5_X_GET_CDC_DATA          0xF2
#define P5_X_CDC_BUSY_STATE        0xF3
#define P5_X_MP_TEST_MODE_INFO     0xFE
#define P5_X_I2C_UART              0x40
#define P5_X_FW_UNKNOWN_MODE       0xFF
#define P5_X_FW_DEMO_MODE          0x00
#define P5_X_FW_TEST_MODE          0x01
#define P5_X_FW_DEBUG_MODE         0x02
#define P5_X_FW_I2CUART_MODE       0x03
#define P5_X_FW_DEMO_DEBUG_INFO_MODE    0x04
#define P5_X_FW_SOP_FLOW_MODE        0xE0
#define P5_X_FW_ESD_MODE            0xFA
#define P5_X_FW_GESTURE_MODE        0x0F
#define P5_X_FW_GESTURE_NORMAL_MODE    0x01
#define P5_X_FW_GESTURE_INFO_MODE    0x02
#define P5_X_FW_DELTA_DATA_MODE        0x03
#define P5_X_FW_RAW_DATA_MODE        0x08
#define P5_X_INFO_HEADER_LENGTH        3

#define P5_X_DEMO_MODE_PACKET_LENGTH    43
#define P5_X_DEBUG_MODE_PACKET_LENGTH    1280
#define P5_X_TEST_MODE_PACKET_LENGTH    1180
#define P5_X_GESTURE_NORMAL_LENGTH    8
#define P5_X_GESTURE_INFO_LENGTH    180
#define P5_X_DEMO_PACKET_ID        0x5A
#define P5_X_DEBUG_PACKET_ID        0xA7
#define P5_X_TEST_PACKET_ID        0xF2
#define P5_X_GESTURE_PACKET_ID        0xAA
#define P5_X_GESTURE_FAIL_ID        0xAE
#define P5_X_DEMO_DEBUG_INFO_PACKET_ID    0x5C
#define P5_X_I2CUART_PACKET_ID        0x7A
#define P5_X_INFO_HEADER_PACKET_ID    0xB7

#define P5_X_EDGE_PLAM_CTRL_1        0x01
#define P5_X_EDGE_PLAM_CTRL_2        0x12
#define SPI_WRITE            0x82
#define SPI_READ            0x83
#define SPI_ACK                0xA3
#define TDDI_WDT_ON            0xA5
#define TDDI_WDT_OFF            0x5A

/* Chipes */
#define TDDI_PID_ADDR            0x4009C
#define TDDI_OTP_ID_ADDR        0x400A0
#define TDDI_ANA_ID_ADDR        0x400A4
#define TDDI_PC_COUNTER_ADDR        0x44008
#define TDDI_WDT_ADDR            0x5100C
#define TDDI_WDT_ACTIVE_ADDR        0x51018
#define TDDI_CHIP_RESET_ADDR        0x40050
#define ILI9881_CHIP            0x9881
#define ILI9881F_AA            0x98810F00
#define ILI9881H_AD            0x98811103
#define ILI9881H_AE            0x98811104

#define RAWDATA_NO_BK_SHIFT_9881H    8192
#define RAWDATA_NO_BK_SHIFT_9881F    4096
#define DEBUG_DATA_SAVE_MAX_FRAME    1024
#define DEBUG_DATA_MAX_LENGTH        2048
#define DEBUG_MESSAGE_MAX_LENGTH     (4096)


/* ilitek mp test  begin*/
struct open_test_c_spec {
	u8 cap1_dac_cmd[16];/*OPEN CAP1 DAC*/
	u8 cap1_raw_cmd[16];/*OPEN CAP1 Raw*/
	int tvch;
	int tvcl;
	int gain;
};

struct core_mp_test_data {
	u32 chip_pid;
	u32 fw_ver;
	u32 core_ver;
	u32 protocol_ver;
	int no_bk_shift;
	bool retry;
	bool m_signal;
	bool m_dac;
	bool s_signal;
	bool s_dac;
	bool key_dac;
	bool st_dac;
	bool p_no_bk;
	bool p_has_bk;
	bool open_integ;
	bool open_cap;
	bool is_longv;

	int cdc_len;
	int xch_len;
	int ych_len;
	int stx_len;
	int srx_len;
	int key_len;
	int st_len;
	int frame_len;
	int mp_items;
	int final_result;

	u32 overlay_start_addr;
	u32 overlay_end_addr;
	u32 mp_flash_addr;
	u32 mp_size;
	u8 dma_trigger_enable;

	/* Tx/Rx threshold & buffer */
	int tx_delta_max;
	int tx_delta_min;
	int rx_delta_max;
	int rx_delta_min;
	s32 *tx_delta_buf;
	s32 *rx_delta_buf;
	s32 *tx_max_buf;
	s32 *tx_min_buf;
	s32 *rx_max_buf;
	s32 *rx_min_buf;

	int tdf;
	int busy_cdc;
	bool ctrl_lcm;
	struct open_test_c_spec open_c_spec;

	s32 *frame_buf;
	s32 *cap_dac;
	s32 *cap_raw;
};
/* ilitek mp test  end*/

/*ilitek fw update*/
struct touch_fw_data {
	u8 block_number;
	u32 start_addr;
	u32 end_addr;
	u32 new_fw_cb;
	int hex_tag;
};

struct flash_block_info {
	char *name;
	u32 start;
	u32 end;
	u32 len;
	u32 mem_start;
	u32 fix_mem_start;
	u8 mode;
};


struct ilitek_tddi_dev {
	struct spi_device *spi;
	struct device *dev;

	struct ilitek_ic_info *chip;
	struct ilitek_protocol_info *protocol;
	tp_dev tp_type;

	struct mutex touch_mutex;
	struct mutex debug_mutex;/*for debug apk*/
	struct mutex debug_read_mutex;/*for debug apk*/
	spinlock_t irq_spin;

	u16 max_x;
	u16 max_y;
	u16 min_x;
	u16 min_y;
	u16 panel_wid;
	u16 panel_hei;
	u8 xch_num;
	u8 ych_num;
	u8 stx;
	u8 srx;

	int actual_tp_mode;
	int irq_num;

	int fw_retry;
	int fw_update_stat;
	bool wq_esd_ctrl;
	bool wq_bat_ctrl;

	bool netlink;
	bool report;
	bool gesture;
	bool gesture_demo_en;
	int gesture_mode;

	/* Sending report data to users for the debug */
	bool debug_node_open;
	int debug_data_frame;
	wait_queue_head_t inq;
	unsigned char **debug_buf;
	int raw_count;
	int delta_count;
	int bg_count;

	int reset;
	/*int fw_upgrade_mode;*/
	bool do_otp_check;

	atomic_t irq_stat;
	atomic_t tp_reset;
	atomic_t ice_stat;
	atomic_t fw_stat;
	atomic_t mp_stat;
	atomic_t tp_sleep;
	atomic_t tp_sw_mode;
	atomic_t mp_int_check;
	atomic_t esd_stat;

	int (*write)(void *chip_data, void *data, int len);
	int (*read)(void *chip_data, void *data, int len);
	int (*spi_write_then_read)(struct spi_device *spi, const void *txbuf,
				   unsigned n_tx, void *rxbuf, unsigned n_rx);
	/*int (*gesture_move_code)(int mode);*/
	/*int (*esd_check)(void);*/
	/*void (*spi_speed)(bool enable);*/
	/*void (*ges_recover)(void);*/
	/*int (*spi_check_read_size)(void);*/
	/*int (*spi_read_after_check_size)(uint8_t *data, int size);*/
	void (*demo_debug_info[5])(void *chip_data, u8 *, int);

	struct touchpanel_data *ts;
	struct hw_resource *hw_res;
	char *fw_name;
	char *test_limit_name;
	int sleep_type;
	char *fw_version;
	int resolution_x;
	int resolution_y;
	u8 touch_direction;
	struct wakeup_source *gesture_process_ws;
	int pointid_info;
	struct point_info points[10];
	uint8_t gesture_data[P5_X_GESTURE_INFO_LENGTH];
	struct firmware tp_firmware;
	struct firmware_headfile *p_firmware_headfile;/*for ini firmware*/
	bool esd_check_enabled;
	bool already_upgrade;
	bool ignore_first_irq;
	unsigned long irq_timer;
	bool get_ic_info_flag;
	bool already_reset;
	u8 *fw_buf_dma;
	bool need_judge_irq_throw;
	bool irq_wake_up_state;
#ifdef CONFIG_OPLUS_TP_APK
	bool plug_status;
	bool lock_point_status;
	bool debug_mode_sta;
	bool debug_gesture_sta;
	bool earphone_sta;
	bool charger_sta;
	bool noise_sta;
#endif /*end of CONFIG_OPLUS_TP_APK*/
	/*mp test*/
	struct core_mp_test_data *core_mp;
	int mp_result_count;
	/*fw update*/
	u8 *gestrue_fw;
	int gestrue_fw_size;
	/*debug node*/
	unsigned char *g_user_buf;
	int g_user_buf_size;
	u32 rw_reg[5];
	struct proc_dir_entry *proc_dir_ilitek;
	/*fw update*/
	struct touch_fw_data *tfd;
	struct flash_block_info fbi[FW_BLOCK_INFO_NUM + 1];
	/*health monitor*/
	struct monitor_data *monitor_data;
	int tp_index;
};

struct ilitek_protocol_info {
	u32 ver;
	int fw_ver_len;
	int pro_ver_len;
	int tp_info_len;
	int key_info_len;
	int panel_info_len;
	int core_ver_len;
	int func_ctrl_len;
	int window_len;
	int cdc_len;
	int mp_info_len;
};

struct ilitek_ic_func_ctrl {
	const char *name;
	u8 cmd[32];
	int len;
};

struct ilitek_ic_info {
	u32 pid_addr;
	u32 wdt_addr;
	u32 pc_counter_addr;
	u32 reset_addr;
	u32 otp_addr;
	u32 ana_addr;
	u32 pid;
	u16 id;
	u16 type_hi;
	u16 type_low;
	u32 otp_id;
	u32 ana_id;
	u32 fw_ver;
	u32 core_ver;
	u32 max_count;
	u32 reset_key;
	u16 wtd_key;
	int no_bk_shift;
	s32(*open_sp_formula)(int dac, int raw);
	s32(*open_c_formula)(int dac, int raw, int tvch, int gain);
	void (*hd_dma_check_crc_off)(void *chip_data);
};

struct record_state {
	u8 touch_palm_state_e : 2;
	u8 app_an_statu_e : 3;
	u8 app_sys_check_bg_abnormal : 1;
	u8 g_b_wrong_bg : 1;
};

struct demo_debug_info_id0 {
	u32 id                  : 8;
	u32 sys_powr_state_e    : 3;
	u32 sys_state_e         : 3;
	u32 tp_state_e          : 2;

	u32 touch_palm_state    : 2;
	u32 app_an_statu_e      : 3;
	u32 app_sys_bg_err      : 1;
	u32 g_b_wrong_bg        : 1;
	u32 reserved0           : 1;

	u32 normal_mode         : 1;
	u32 charger_mode        : 1;
	u32 glove_mode          : 1;
	u32 stylus_mode         : 1;
	u32 multi_mode          : 1;
	u32 noise_mode          : 1;
	u32 palm_plus_mode      : 1;
	u32 floating_mode       : 1;

	u32 algo_pt_status0     : 3;
	u32 algo_pt_status1     : 3;
	u32 algo_pt_status2     : 3;
	u32 algo_pt_status3     : 3;
	u32 algo_pt_status4     : 3;
	u32 algo_pt_status5     : 3;
	u32 algo_pt_status6     : 3;
	u32 algo_pt_status7     : 3;
	u32 algo_pt_status8     : 3;
	u32 algo_pt_status9     : 3;
	u32 reserved2           : 2;

	u32 hopping_flg         : 1;
	u32 hopping_index       : 5;
	u32 frequency_h         : 2;
	u32 frequency_l         : 8;
	u32 reserved3           : 8;
	u32 reserved4           : 8;
};

extern s32 ipio_debug_level;
extern struct ilitek_tddi_dev *idev;

/* Prototypes for tddi firmware/flash functions */
void ilitek_tddi_ic_check_otp_prog_mode(void *chip_data);
int ilitek_tddi_fw_dump_iram_data(void *chip_data, u32 start, u32 end);
int ilitek_tddi_fw_upgrade(void *chip_data);

/* Prototypes for tddi core functions */
int ilitek_tddi_ic_watch_dog_ctrl(void *chip_data, bool write, bool enable);
void ilitek_tddi_ic_set_ddi_reg_onepage(void *chip_data, u8 page, u8 reg,
					u8 data);
void ilitek_tddi_ic_get_ddi_reg_onepage(void *chip_data, u8 page, u8 reg);
int ilitek_tddi_ic_whole_reset(void *chip_data);
int ilitek_tddi_ic_code_reset(void *chip_data);
int ilitek_tddi_ic_func_ctrl(void *chip_data, const char *name, int ctrl);
u32 ilitek_tddi_ic_get_pc_counter(void *chip_data);
int ilitek_tddi_ic_check_busy(void *chip_data, int count, int delay);
int ilitek_tddi_ic_get_panel_info(void *chip_data);
int ilitek_tddi_ic_get_tp_info(void *chip_data);
int ilitek_tddi_ic_get_core_ver(void *chip_data);
int ilitek_tddi_ic_get_protocl_ver(void *chip_data);
int ilitek_tddi_ic_get_fw_ver(void *chip_data);
int ilitek_tddi_ic_get_info(void *chip_data);
int ilitek_ice_mode_write(void *chip_data, u32 addr, u32 data, int len);
int ilitek_ice_mode_read(void *chip_data, u32 addr, u32 *data, int len);
int ilitek_ice_mode_ctrl(void *chip_data, bool enable, bool mcu);
void ilitek_tddi_ic_init(void *chip_data);
int ilitek_tddi_edge_palm_ctrl(void *chip_data, u8 type);

/* Prototypes for tddi events */
int ilitek_tddi_switch_mode(void *chip_data, u8 *data);
void ilitek_tddi_wq_ges_recover(void *chip_data);
int ilitek_tddi_reset_ctrl(void *chip_data, enum TP_RST_METHOD mode);


/* Prototypes for demo_debug_info_mode */
void ilitek_set_gesture_fail_reason(void *chip_data, bool enable);
int ilitek_tddi_get_tp_recore_ctrl(void *chip_data, int data);

/* Prototypes for miscs */
void ilitek_tddi_node_init(void *chip_data);
void ilitek_tddi_node_exit(void *chip_data);

void ilitek_dump_data(void *data, int type, int len, int row_len,
		      const char *name);
void netlink_reply_msg(void *raw, int size);
int katoi(char *str);

bool ilitek_check_wake_up_state(u32 msecs);

static inline s32 open_sp_formula_ili9881(int dac, int raw)
{
	return (int)((int)(dac * 2 * 10000 * 161 / 100) - (int)(16384 / 2 -
			(int)raw) * 20000 * 7 / 16384 * 36 / 10) / 31 / 2;
}

static inline s32 open_sp_formula_ili7807(int dac, int raw)
{
	return (int)((int)(dac * 2 * 10000 * 131 / 100) - (int)(16384 / 2 -
			(int)raw) * 20000 * 7 / 16384 * 36 / 10) / 31 / 2;
}

static inline s32 open_c_formula(int dac, int raw, int tvch, int gain)
{
	return (int)((int)(dac * 414 * 39 / 2) + (int)(((int)raw - 8192) * 36 *
			(7 * 100 - 22) * 10 / 16384)) /
	       tvch / 100 / gain;
}

static inline void firmware_hd_dma_crc_off_ili9881(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	/* crc off */
	ilitek_ice_mode_write(chip_info, 0x041016, 0x00, 1);
	/* dma crc */
	ilitek_ice_mode_write(chip_info, 0x041048, 0x00000001, 4);
}

static inline void firmware_hd_dma_crc_off_ili7807(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	/* crc off */
	ilitek_ice_mode_write(chip_info, 0x041016, 0x00, 1);
	/* dma crc */
	ilitek_ice_mode_write(chip_info, 0x041017, 0x03, 1);
}

/*test item*/
enum {
	TYPE_ERROR                 = 0x00,
	TYPE_TIMEING_INFO    = 0x01,/*PV5_4 Command*/
	TYPE_TEST2                   = 0x02,/*[Noise Peak to Peak(IC Only)]*/
	TYPE_TEST3                   = 0x03,/*[Noise Peak To Peak(With Panel)]*/
	TYPE_TEST4                   = 0x04,
	TYPE_TEST5                   = 0x05,
	TYPE_TEST6                   = 0x06,
	TYPE_TEST7                   = 0x07,
	TYPE_TEST8                   = 0x08,
	TYPE_TEST9                   = 0x09,
	TYPE_TEST10                 = 0x0A,
	TYPE_TEST11                 = 0x0B,
	TYPE_TEST12                 = 0x0C,
	TYPE_TEST13                 = 0x0D,
	TYPE_TEST14                 = 0x0E,
	TYPE_TEST15                 = 0x0F,
	TYPE_TEST16                 = 0x010,
	TYPE_MAX                     = 0xFF,
};

extern struct ilitek_test_operations ilitek_9881_test_ops;
extern struct engineer_test_operations ilitek_9881_engineer_test_ops;

#endif /* __ILITEK_H */
