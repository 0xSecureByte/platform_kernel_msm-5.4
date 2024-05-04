/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#ifndef HX83112B_H
#define HX83112B_H

/*********PART1:Head files**********************/
#include <linux/i2c.h>

#ifdef CONFIG_SPI_MT65XX
#include <linux/platform_data/spi-mt65xx.h>
#endif
#include "../himax_common.h"
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
#include "mtk_gpio.h"
#else
#include "oplus_spi.h"
#endif

#ifndef REMOVE_OPLUS_FUNCTION
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
#include <mt-plat/mtk_boot_common.h>
#else
#include <soc/oplus/system/boot_mode.h>
#endif
#endif

#define HX_ZERO_FLASH
#define HX_ENTER_ALGORITHM_NUMBER		/*Support calculation enter algorithm number*/

/*********PART2:Define Area**********************/
#define ENABLE_UNICODE    0x40
#define ENABLE_VEE        0x20
#define ENABLE_CIRCLE     0x08
#define ENABLE_SWIPE      0x02
#define ENABLE_DTAP       0x01

#define UNICODE_DETECT    0x0b
#define VEE_DETECT        0x0a
#define CIRCLE_DETECT     0x08
#define SWIPE_DETECT      0x07
#define DTAP_DETECT       0x03

#define RESET_TO_NORMAL_TIME         5    /*Sleep time after reset*/

#define SPURIOUS_FP_LIMIT            100
#define SPURIOUS_FP_RX_NUM           8
#define SPURIOUS_FP_TX_NUM           9
#define SPURIOUS_FP_BASE_DATA_RETRY  10

#define I2C_ERROR_MAX_TIME           5

#define HX_32K_SZ 0x8000
#define FOUR_BYTE_ADDR_SZ     4

#define EXTEND_EE_SHORT_RESET_DUR    60
#define EXTEND_EE_SHORT_INT_DUR      150
#define EXTEND_EE_SHORT_TX_ON_COUNT  146
#define EXTEND_EE_SHORT_RX_ON_COUNT  146
#define EXTEND_EE_SHORT_TEST_LIMIT_PART1    160
#define EXTEND_EE_SHORT_TEST_LIMIT_PART2    80         /* ( unit = ratio )*/

/* tddi f54 test reporting - */
#define ELEC_OPEN_TEST_TX_ON_COUNT      2
#define ELEC_OPEN_TEST_RX_ON_COUNT      2
#define ELEC_OPEN_INT_DUR_ONE           15
#define ELEC_OPEN_INT_DUR_TWO           25
#define ELEC_OPEN_TEST_LIMIT_ONE        500
#define ELEC_OPEN_TEST_LIMIT_TWO        50

#define COMMAND_FORCE_UPDATE            4

/*#define UPDATE_DISPLAY_CONFIG*/

/*Self Capacitance key test limite*/
#define MENU_LOW_LIMITE        1630
#define MENU_HIGH_LIMITE       3803

#define BACK_LOW_LIMITE        3016
#define BACK_HIGH_LIMITE       7039

#define TEST_FAIL    1
#define TEST_PASS    0

#define LIMIT_DOZE_LOW     50
#define LIMIT_DOZE_HIGH    975
#define firmware_update_space	131072	/*128*1024*/

/*gmq-himax*/

#define HX64K         0x10000
#define FW_BIN_16K_SZ 0x4000
struct touchpanel_data *private_ts;

#define HX_KEY_MAX_COUNT             4
#define DEFAULT_RETRY_CNT            3
#define HIMAX_REG_RETRY_TIMES        10

#define HX_83112A_SERIES_PWON        16
#define HX_83112B_SERIES_PWON        17

#define HX_TP_BIN_CHECKSUM_SW        1
#define HX_TP_BIN_CHECKSUM_HW        2
#define HX_TP_BIN_CHECKSUM_CRC       3

#define SHIFTBITS 5

#define  FW_SIZE_64K          65536
#define  FW_SIZE_128K         131072

#define HX_FINGER_ON          1
#define HX_FINGER_LEAVE       2

#define HX_REPORT_COORD       1
#define HX_REPORT_SMWP_EVENT  2

/*gesture head information*/
#define GEST_PTLG_ID_LEN     (4)
#define GEST_PTLG_HDR_LEN    (4)
#define GEST_PTLG_HDR_ID1    (0xCC)
#define GEST_PTLG_HDR_ID2    (0x44)
#define GEST_PT_MAX_NUM      (128)

#define BS_RAWDATANOISE      10
#define    BS_OPENSHORT      0

#define    SKIPRXNUM         31
/**COMMON USE*********END***/

/** FOR DEBUG USE*****START***/
typedef enum {
	DEBUG_DATA_BASELINE = 0x08,
	DEBUG_DATA_DELTA    = 0x09,
	DEBUG_DATA_RAW      = 0x0A,
	DEBUG_DATA_DOWN     = 0x0B,
} DEBUG_DATA_TYPE;

/*self test use*/

/* ASCII format */
#define ASCII_LF    (0x0A)
#define ASCII_CR    (0x0D)
#define ASCII_COMMA (0x2C)
#define ASCII_ZERO  (0x30)
#define CHAR_EL     '\0'
#define CHAR_NL     '\n'
#define ACSII_SPACE (0x20)

typedef enum {
	HIMAX_INSPECTION_NULL = 0,
	HIMAX_INSPECTION_OPEN,
	HIMAX_INSPECTION_MICRO_OPEN,
	HIMAX_INSPECTION_SHORT,
	HIMAX_INSPECTION_RAWDATA,
	HIMAX_INSPECTION_NOISE,
	HIMAX_INSPECTION_LPWUG_RAWDATA,
	HIMAX_INSPECTION_LPWUG_NOISE,
	HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA,
	HIMAX_INSPECTION_LPWUG_IDLE_NOISE,
	HIMAX_INSPECTION_SORTING,
	HIMAX_INSPECTION_BACK_NORMAL,
} THP_INSPECTION_ENUM;

/* Error code of AFE Inspection */
#define HX_RSLT_OUT_PATH_OK "/sdcard/TpTestReport/screenOn/OK/"
#define HX_RSLT_OUT_PATH_NG "/sdcard/TpTestReport/screenOn/NG/"
#define HX_GES_RSLT_OUT_PATH_OK "/sdcard/TpTestReport/screenOff/OK/"
#define HX_GES_RSLT_OUT_PATH_NG "/sdcard/TpTestReport/screenOff/NG/"

/*#define HX_RSLT_OUT_FILE_OK "tp_testlimit_OK_"*/
/*#define HX_RSLT_OUT_FILE_NG "tp_testlimitst_NG_"*/

typedef enum {
	RESULT_OK = 0,
	RESULT_ERR,
	RESULT_RETRY,
} RETURN_RESLUT;

typedef enum {
	SKIPTXNUM_START = 6,
	SKIPTXNUM_6 = SKIPTXNUM_START,
	SKIPTXNUM_7,
	SKIPTXNUM_8,
	SKIPTXNUM_9,
	SKIPTXNUM_END = SKIPTXNUM_9,
} SKIPTXNUMINDEX;

char *g_himax_inspection_mode[] = {
	"HIMAX_INSPECTION_NULL",
	"HIMAX_INSPECTION_OPEN",
	"HIMAX_INSPECTION_MICRO_OPEN",
	"HIMAX_INSPECTION_SHORT",
	"HIMAX_INSPECTION_RAWDATA",
	"HIMAX_INSPECTION_NOISE",
	"HIMAX_INSPECTION_LPWUG_RAWDATA",
	"HIMAX_INSPECTION_LPWUG_NOISE",
	"HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA",
	"HIMAX_INSPECTION_LPWUG_IDLE_NOISE",
	"HIMAX_INSPECTION_SORTING",
	"HIMAX_INSPECTION_BACK_NORMAL",
};

/* for criteria */
char *g_hx_inspt_crtra_name[] = {
	"CRITERIA_OPEN_MIN",
	"CRITERIA_OPEN_MAX",
	"CRITERIA_MICRO_OPEN_MIN",
	"CRITERIA_MICRO_OPEN_MAX",
	"CRITERIA_SHORT_MIN",
	"CRITERIA_SHORT_MAX",
	"CRITERIA_RAWDATA_MIN",   /* CRITERIA_RAW_MIN */
	"CRITERIA_RAWDATA_MAX",  /* CRITERIA_RAW_MAX */
	"CRITERIA_NOISE_MAX",
	"LPWUG_RAWDATA_MIN",
	"LPWUG_RAWDATA_MAX",
	"LPWUG_NOISE_MIN",
	"LPWUG_NOISE_MAX",
	"LPWUG_IDLE_RAWDATA_MIN",
	"LPWUG_IDLE_RAWDATA_MAX",
	"LPWUG_IDLE_NOISE_MIN",
	"LPWUG_IDLE_NOISE_MAX",
	NULL
};

enum HX_CRITERIA_ENUM {
	IDX_OPENMIN = 0,
	IDX_OPENMAX,
	IDX_M_OPENMIN,
	IDX_M_OPENMAX,
	IDX_SHORTMIN,
	IDX_SHORTMAX,
	IDX_RAWDATA_MIN,
	IDX_RAWDATA_MAX,
	IDX_NOISEMAX,
	IDX_LPWUG_RAWDATA_MIN,
	IDX_LPWUG_RAWDATA_MAX,
	IDX_LPWUG_NOISE_MIN,
	IDX_LPWUG_NOISE_MAX,
	IDX_LPWUG_IDLE_RAWDATA_MIN,
	IDX_LPWUG_IDLE_RAWDATA_MAX,
	IDX_LPWUG_IDLE_NOISE_MIN,
	IDX_LPWUG_IDLE_NOISE_MAX,
};

/* Error code of AFE Inspection */
typedef enum {
	THP_AFE_INSPECT_OK      = 0,               /* OK */
	THP_AFE_INSPECT_ESPI    = (1 << 0),        /* SPI communication error */
	THP_AFE_INSPECT_ERAW    = (1 << 1),        /* Raw data error */
	THP_AFE_INSPECT_ENOISE  = (1 << 2),        /* Noise error */
	THP_AFE_INSPECT_EOPEN   = (1 << 3),        /* Sensor open error */
	THP_AFE_INSPECT_EMOPEN  = (1 << 4),        /* Sensor micro open error */
	THP_AFE_INSPECT_ESHORT  = (1 << 5),        /* Sensor short error */
	THP_AFE_INSPECT_ERC     = (1 << 6),        /* Sensor RC error */
	THP_AFE_INSPECT_EPIN    = (1 << 7),        /* Errors of TSVD!FTSHD!FTRCST!FTRCRQ and other PINs
												when Report Rate Switching between 60 Hz and 120 Hz */
	THP_AFE_INSPECT_EOTHER  = (1 << 8),         /* All other errors */
	THP_AFE_INSPECT_EFILE  = (1 << 9)         /* Get Criteria file error */
} THP_AFE_INSPECT_ERR_ENUM;


/*Himax MP Limit*/
/*#define RAWMIN         3547*0.05
#define RAWMAX         5352*0.9*/

/* Define Criteria*/
#define RAWMIN         906
#define RAWMAX         16317
#define SHORTMIN       0
#define SHORTMAX       150
#define OPENMIN        50
#define OPENMAX        500
#define M_OPENMIN      0
#define M_OPENMAX      150

#define RAWMIN_BD12    895;
#define RAWMAX_BD12    16117;
#define LPWUG_RAWDATA_MAX_BD12        9999;
#define LPWUG_IDLE_RAWDATA_MAX_BD12   9999;

#define NOISEFRAME     BS_RAWDATANOISE+40
#define NOISE_P        256 /*gmqtest*/
#define UNIFMAX        500

/*Himax MP Password*/
#define PWD_OPEN_START      0x77
#define PWD_OPEN_END        0x88
#define PWD_SHORT_START     0x11
#define PWD_SHORT_END       0x33
#define PWD_RAWDATA_START   0x00
#define PWD_RAWDATA_END     0x99
#define PWD_NOISE_START     0x00
#define PWD_NOISE_END       0x99
#define PWD_SORTING_START   0xAA
#define PWD_SORTING_END     0xCC

/*Himax DataType*/
#define DATA_OPEN           0x0B
#define DATA_MICRO_OPEN     0x0C
#define DATA_SHORT          0x0A
#define DATA_RAWDATA        0x0A
#define DATA_NOISE          0x0F
#define DATA_BACK_NORMAL    0x00
#define DATA_LPWUG_RAWDATA  0x0C
#define DATA_LPWUG_NOISE    0x0F
/*#define DATA_DOZE_RAWDATA 0x0A
#define DATA_DOZE_NOISE 0x0F */
#define DATA_LPWUG_IDLE_RAWDATA    0x0A
#define DATA_LPWUG_IDLE_NOISE      0x0F

/*Himax Data Ready Password*/
#define Data_PWD0    0xA5
#define Data_PWD1    0x5A

#define BS_LPWUG           1
/*#define BS_DOZE            1*/
#define BS_LPWUG_IDLE      1

/* Define Criteria*/
#define LPWUG_NOISE_MAX          100
#define LPWUG_NOISE_MIN          0
#define LPWUG_RAWDATA_MAX        15000
#define LPWUG_RAWDATA_MIN        0
#define DOZE_NOISE_MAX           100
#define DOZE_NOISE_MIN           0
#define DOZE_RAWDATA_MAX         9999
#define DOZE_RAWDATA_MIN         0
#define LPWUG_IDLE_NOISE_MAX     100
#define LPWUG_IDLE_NOISE_MIN     0
#define LPWUG_IDLE_RAWDATA_MAX   15000
#define LPWUG_IDLE_RAWDATA_MIN   0

#define PWD_NONE                 0x00
#define PWD_LPWUG_START          0x55
#define PWD_LPWUG_END            0x66
/*#define PWD_DOZE_START 0x22
#define PWD_DOZE_END 0x44 */
#define PWD_LPWUG_IDLE_START     0x50
#define PWD_LPWUG_IDLE_END       0x60

#define SKIP_NOTCH_START         5
#define SKIP_NOTCH_END           10
#define SKIP_DUMMY_START         23    /*TX+SKIP_NOTCH_START*/
#define SKIP_DUMMY_END           28 /* TX+SKIP_NOTCH_END*/

/** FOR DEBUG USE ****END****/

struct himax_report_data {
	int touch_all_size;
	int raw_cnt_max;
	int raw_cnt_rmd;
	int touch_info_size;
	uint8_t finger_num;
	uint8_t finger_on;
	uint8_t *hx_coord_buf;
	uint8_t hx_state_info[5];

	int event_size;
	uint8_t *hx_event_buf;
	int hx_event_buf_oplus[128];

	int rawdata_size;
	uint8_t diag_cmd;
	uint8_t *hx_rawdata_buf;
	/*uint32_t *diag_mutual;*/
	int32_t *diag_mutual;
	uint8_t rawdata_frame_size;
};

/*********PART3:Struct Area**********************/
struct himax_fw_debug_info {
	u16 recal0 : 1;
	u16 recal1 : 1;
	u16 paseline : 1;
	u16 palm : 1;
	u16 idle : 1;
	u16 water : 1;
	u16 hopping : 1;
	u16 noise : 1;
	u16 glove : 1;
	u16 border : 1;
	u16 vr : 1;
	u16 big_small : 1;
	u16 one_block : 1;
	u16 blewing : 1;
	u16 thumb_flying : 1;
	u16 border_extend : 1;
};

struct chip_data_hx83112a_nf {
	uint32_t *p_tp_fw;
	tp_dev tp_type;
	char   *test_limit_name;
	struct himax_proc_operations
		*hx_ops; /*hx83112b func provide to hx83112b common driver*/

	struct hw_resource  *hw_res;
	struct monitor_data *monitor_data;
	/********SPI bus*******************************/
	struct spi_device    *hx_spi;
	int                  hx_irq;
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
	struct mtk_chip_config hx_spi_mcc;
#else
	struct mt_chip_conf    hx_spi_mcc;
#endif
	/********SPI bus*******************************/
#ifdef HX_ZERO_FLASH
	struct mutex             spi_lock;
	struct workqueue_struct  *himax_0f_update_wq;
	struct delayed_work      work_0f_update;
	struct firmware_headfile *p_firmware_headfile;
	uint8_t *tmp_data;
	int g_f_0f_updat;
	int cfg_crc;
	int cfg_sz;
	bool cfg_flag;
	uint8_t sram_min[4];
	unsigned char *FW_buf;
#endif
	struct himax_report_data *hx_touch_data;
	uint8_t     touch_direction;    /*show touchpanel current direction*/
	bool        using_headfile;
	bool        first_download_finished;

	uint32_t    fw_id;
	uint16_t    fw_ver;

	u8 *g_fw_buf;
	size_t g_fw_len;
	bool g_fw_sta;

	bool dsram_flag;
	int32_t diag_self[100];
	int diag_command;
	uint8_t register_command[4];
	int touch_info_point_cnt;
#ifdef CONFIG_OPLUS_TP_APK
	bool lock_point_status;
	bool plug_status;
	bool debug_mode_sta;
	bool debug_gesture_sta;
	bool earphone_sta;
	bool charger_sta;
	bool noise_sta;
#endif /*end of CONFIG_OPLUS_TP_APK*/

	int tp_index;

	int hw_reset_active;
	int irq_en_cnt;

	int check_point_format;
	unsigned char switch_algo;
	uint8_t proc_send_flag;
	u8 esd_reset_activate;

	int max_mutual;
	int min_mutual;
	int max_self;
	int min_self;

	int mutual_set_flag;
	uint8_t cmd_set[8];

	/**GESTURE_TRACK*/
	int gest_pt_cnt;
	int gest_pt_x[10];
	int gest_pt_y[10];

	/**COMMON USE   ***START***/
	unsigned long FW_VER_MAJ_FLASH_ADDR;
	unsigned long FW_VER_MIN_FLASH_ADDR;
	unsigned long CFG_VER_MAJ_FLASH_ADDR;
	unsigned long CFG_VER_MIN_FLASH_ADDR;
	unsigned long CID_VER_MAJ_FLASH_ADDR;
	unsigned long CID_VER_MIN_FLASH_ADDR;
	unsigned long FW_VER_MAJ_FLASH_LENG;
	unsigned long FW_VER_MIN_FLASH_LENG;
	unsigned long CFG_VER_MAJ_FLASH_LENG;
	unsigned long CFG_VER_MIN_FLASH_LENG;
	unsigned long CID_VER_MAJ_FLASH_LENG;
	unsigned long CID_VER_MIN_FLASH_LENG;
	unsigned long FW_CFG_VER_FLASH_ADDR;

	uint16_t NOISEMAX;
	uint16_t LPWUG_NOISEMAX;
};

/*********PART4:ZERO FLASH**********************/
#if defined(HX_ZERO_FLASH)
#define HX_0F_DEBUG
#define HX_48K_SZ 0xC000
#define MAX_TRANS_SZ    240     /*MTK:240_WRITE_limit*/
#define MAX_RECVS_SZ    56      /*MTK:56_READ_limit*/

#define zf_addr_dis_flash_reload              0x10007f00
#define zf_data_dis_flash_reload              0x00009AA9
#define ZF_ADDR_SYSTEM_RESET                 0x90000018
#define ZF_DATA_SYSTEM_RESET                 0x00000055
#define ZF_DATA_SRAM_START_ADDR              0x08000000
#define ZF_DATA_SRAM_CLEAN             0x10000000
#define ZF_DATA_CFG_INFO             0x10007000
#define ZF_DATA_FW_CFG_P1             0x10007084
#define ZF_DATA_FW_CFG_P2             0x10007264
#define ZF_DATA_FW_CFG_P3             0x10007300
#define ZF_DATA_ADC_CFG_1             0x10006A00
#define ZF_DATA_ADC_CFG_2             0x10007B28
#define ZF_DATA_ADC_CFG_3             0x10007AF0
#define ZF_DATA_MAP_TALBLE             0x10007500
#define ZF_DATA_MODE_SWITCH             0x10007294

struct zf_operation {
	uint8_t addr_dis_flash_reload[4];
	uint8_t data_dis_flash_reload[4];
	uint8_t addr_system_reset[4];
	uint8_t data_system_reset[4];
	uint8_t data_sram_start_addr[4];
	uint8_t data_sram_clean[4];
	uint8_t data_cfg_info[4];
	uint8_t data_fw_cfg[4];
	uint8_t data_fw_cfg_p1[4];
	uint8_t data_fw_cfg_p2[4];
	uint8_t data_fw_cfg_p3[4];
	uint8_t data_adc_cfg_1[4];
	uint8_t data_adc_cfg_2[4];
	uint8_t data_adc_cfg_3[4];
	uint8_t data_map_table[4];
	uint8_t data_mode_switch[4];
};
struct zf_info {
	uint8_t sram_addr[4];
	int write_size;
	uint32_t fw_addr;
	uint32_t cfg_addr;
};


struct himax_core_fp {
	int (*fp_reload_disable)(struct chip_data_hx83112a_nf *chip_info, int disable);
	void (*fp_sys_reset)(struct chip_data_hx83112a_nf *chip_info);
	void (*fp_clean_sram_0f)(struct chip_data_hx83112a_nf *chip_info, uint8_t *addr,
				 int write_len, int type);
	void (*fp_write_sram_0f)(struct chip_data_hx83112a_nf *chip_info,
				 const struct firmware *fw_entry, uint8_t *addr, int start_index,
				 uint32_t write_len);
	void (*fp_firmware_update_0f)(struct chip_data_hx83112a_nf *chip_info,
				      const struct firmware *fw_entry);
	int (*fp_0f_operation_dirly)(struct chip_data_hx83112a_nf *chip_info);
	int (*fp_0f_op_file_dirly)(struct chip_data_hx83112a_nf *chip_info,
				   char *file_name);
	void (*fp_0f_operation)(struct work_struct *work);
#ifdef HX_0F_DEBUG
	void (*fp_read_sram_0f)(struct chip_data_hx83112a_nf *chip_info,
				const struct firmware *fw_entry, uint8_t *addr, int start_index, int read_len);
	void (*fp_read_all_sram)(struct chip_data_hx83112a_nf *chip_info, uint8_t *addr,
				 int read_len);
	void (*fp_firmware_read_0f)(struct chip_data_hx83112a_nf *chip_info,
				    const struct firmware *fw_entry, int type);
	void (*fp_0f_operation_check)(struct chip_data_hx83112a_nf *chip_info,
				      int type);
#endif  /*HX_0F_DEBUG*/
};
#endif  /*HX_ZERO_FLASH*/

#endif  /*HX83112B_H*/
