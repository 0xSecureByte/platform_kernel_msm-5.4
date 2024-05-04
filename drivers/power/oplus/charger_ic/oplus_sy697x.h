
#ifndef __SY697X_HEADER__
#define __SY697X_HEADER__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/iio/consumer.h>
#include <linux/pm_wakeup.h>

/* Register 00h */
#define SY697X_REG_00			0x00
#define SY697X_ENHIZ_MASK		    0x80
#define SY697X_ENHIZ_SHIFT		    7
#define SY697X_HIZ_ENABLE          1
#define SY697X_HIZ_DISABLE         0
#define SY697X_ENILIM_MASK		    0x40
#define SY697X_ENILIM_SHIFT		6
#define SY697X_ENILIM_ENABLE       1
#define SY697X_ENILIM_DISABLE      0

#define SY697X_IINLIM_MASK		    0x3F
#define SY697X_IINLIM_SHIFT		0
#define SY697X_IINLIM_BASE         100
#define SY697X_IINLIM_LSB          50

/* Register 01h */
#define SY697X_REG_01			0x01

#define	SY697X_DPDAC_MASK			0xE0
#define	SY697X_DPDAC_SHIFT			5
#define SY697X_DP_HIZ				0x00
#define SY697X_DP_0V				0x01
#define SY697X_DP_0P6V				0x02
#define SY697X_DP_1P2V				0x03
#define SY697X_DP_2P0V				0x04
#define SY697X_DP_2P7V				0x05
#define SY697X_DP_3P3V				0x06
#define SY697X_DP_SHORT			0x07

#define	SY697X_DMDAC_MASK			0x1C
#define	SY697X_DMDAC_SHIFT			2
#define SY697X_DM_HIZ				0x00
#define SY697X_DM_0V				0x01
#define SY697X_DM_0P6V				0x02
#define SY697X_DM_1P2V				0x03
#define SY697X_DM_2P0V				0x04
#define SY697X_DM_2P7V				0x05
#define SY697X_DM_3P3V				0x06

#define	SY697X_EN12V_MASK			0x02
#define	SY697X_EN12V_SHIFT			1
#define	SY697X_ENABLE_12V			1
#define	SY697X_DISABLE_12V			0

#define SY697X_VINDPMOS_MASK       0x01
#define SY697X_VINDPMOS_SHIFT      0
#define	SY697X_VINDPMOS_400MV		0
#define	SY697X_VINDPMOS_600MV		1

/* Register 0x02 */
#define SY697X_REG_02              0x02
#define SY697X_CONV_START_MASK      0x80
#define SY697X_CONV_START_SHIFT     7
#define SY697X_CONV_START_DISABLE   0
#define SY697X_CONV_START_ENABLE    1
#define SY697X_CONV_RATE_MASK       0x40
#define SY697X_CONV_RATE_SHIFT      6
#define SY697X_ADC_CONTINUE_ENABLE  1
#define SY697X_ADC_CONTINUE_DISABLE 0

#define SY697X_BOOST_FREQ_MASK      0x20
#define SY697X_BOOST_FREQ_SHIFT     5
#define SY697X_BOOST_FREQ_1500K     0
#define SY697X_BOOST_FREQ_500K      1

#define SY697X_ICOEN_MASK          0x10
#define SY697X_ICOEN_SHIFT         4
#define SY697X_ICO_ENABLE          1
#define SY697X_ICO_DISABLE         0
#define SY697X_HVDCPEN_MASK        0x08
#define SY697X_HVDCPEN_SHIFT       3
#define SY697X_HVDCPHV_SHIFT       2
#define SY697X_HVDCP_ENABLE        1
#define SY697X_HVDCP_DISABLE       0
#define SY697X_MAXCEN_MASK         0x04
#define SY697X_MAXCEN_SHIFT        2
#define SY697X_MAXC_ENABLE         1
#define SY697X_MAXC_DISABLE        0
#define SY697X_HVDCP_5V_SHIFT      8
#define SY697X_HVDCP_9V_SHIFT      4

#define SY697X_FORCE_DPDM_MASK     0x02
#define SY697X_FORCE_DPDM_SHIFT    1
#define SY697X_FORCE_DPDM          1
#define SY697X_AUTO_DPDM_EN_MASK   0x01
#define SY697X_AUTO_DPDM_EN_SHIFT  0
#define SY697X_AUTO_DPDM_ENABLE    1
#define SY697X_AUTO_DPDM_DISABLE   0


/* Register 0x03 */
#define SY697X_REG_03              0x03
#define SY697X_BAT_VOKOTG_EN_MASK   0x80
#define SY697X_BAT_VOKOTG_EN_SHIFT  7
#define SY697X_BAT_FORCE_DSEL_MASK  0x80
#define SY697X_BAT_FORCE_DSEL_SHIFT 7

#define SY697X_WDT_RESET_MASK      0x40
#define SY697X_WDT_RESET_SHIFT     6
#define SY697X_WDT_RESET           1

#define SY697X_OTG_CONFIG_MASK     0x20
#define SY697X_OTG_CONFIG_SHIFT    5
#define SY697X_OTG_ENABLE          1
#define SY697X_OTG_DISABLE         0

#define SY697X_CHG_CONFIG_MASK     0x10
#define SY697X_CHG_CONFIG_SHIFT    4
#define SY697X_CHG_ENABLE          1
#define SY697X_CHG_DISABLE         0


#define SY697X_SYS_MINV_MASK       0x0E
#define SY697X_SYS_MINV_SHIFT      1

#define SY697X_SYS_MINV_BASE       3000
#define SY697X_SYS_MINV_LSB        100


/* Register 0x04*/
#define SY697X_REG_04              0x04
#define SY697X_EN_PUMPX_MASK       0x80
#define SY697X_EN_PUMPX_SHIFT      7
#define SY697X_PUMPX_ENABLE        1
#define SY697X_PUMPX_DISABLE       0
#define SY697X_ICHG_MASK           0x7F
#define SY697X_ICHG_SHIFT          0
#define SY697X_ICHG_BASE           0
#define SY697X_ICHG_LSB            64

/* Register 0x05*/
#define SY697X_REG_05              0x05
#define SY697X_IPRECHG_MASK        0xF0
#define SY697X_IPRECHG_SHIFT       4
#define SY697X_ITERM_MASK          0x0F
#define SY697X_ITERM_SHIFT         0
#define SY697X_IPRECHG_BASE        64
#define SY697X_IPRECHG_LSB         64
#define SY697X_ITERM_BASE          64
#define SY697X_ITERM_LSB           64

/* Register 0x06*/
#define SY697X_REG_06              0x06
#define SY697X_VREG_MASK           0xFC
#define SY697X_VREG_SHIFT          2
#define SY697X_BATLOWV_MASK        0x02
#define SY697X_BATLOWV_SHIFT       1
#define SY697X_BATLOWV_2800MV      0
#define SY697X_BATLOWV_3000MV      1
#define SY697X_VRECHG_MASK         0x01
#define SY697X_VRECHG_SHIFT        0
#define SY697X_VRECHG_100MV        0
#define SY697X_VRECHG_200MV        1
#define SY697X_VREG_BASE           3840
#define SY697X_VREG_LSB            16

/* Register 0x07*/
#define SY697X_REG_07              0x07
#define SY697X_EN_TERM_MASK        0x80
#define SY697X_EN_TERM_SHIFT       7
#define SY697X_TERM_ENABLE         1
#define SY697X_TERM_DISABLE        0

#define SY697X_WDT_MASK            0x30
#define SY697X_WDT_SHIFT           4
#define SY697X_WDT_DISABLE         0
#define SY697X_WDT_40S             1
#define SY697X_WDT_80S             2
#define SY697X_WDT_160S            3
#define SY697X_WDT_BASE            0
#define SY697X_WDT_LSB             40

#define SY697X_EN_TIMER_MASK       0x08
#define SY697X_EN_TIMER_SHIFT      3

#define SY697X_CHG_TIMER_ENABLE    1
#define SY697X_CHG_TIMER_DISABLE   0

#define SY697X_CHG_TIMER_MASK      0x06
#define SY697X_CHG_TIMER_SHIFT     1
#define SY697X_CHG_TIMER_5HOURS    0
#define SY697X_CHG_TIMER_8HOURS    1
#define SY697X_CHG_TIMER_12HOURS   2
#define SY697X_CHG_TIMER_20HOURS   3

#define SY697X_JEITA_ISET_MASK     0x01
#define SY697X_JEITA_ISET_SHIFT    0
#define SY697X_JEITA_ISET_50PCT    0
#define SY697X_JEITA_ISET_20PCT    1


/* Register 0x08*/
#define SY697X_REG_08              0x08
#define SY697X_BAT_COMP_MASK       0xE0
#define SY697X_BAT_COMP_SHIFT      5
#define SY697X_VCLAMP_MASK         0x1C
#define SY697X_VCLAMP_SHIFT        2
#define SY697X_TREG_MASK           0x03
#define SY697X_TREG_SHIFT          0
#define SY697X_TREG_60C            0
#define SY697X_TREG_80C            1
#define SY697X_TREG_100C           2
#define SY697X_TREG_120C           3

#define SY697X_BAT_COMP_BASE       0
#define SY697X_BAT_COMP_LSB        20
#define SY697X_VCLAMP_BASE         0
#define SY697X_VCLAMP_LSB          32


/* Register 0x09*/
#define SY697X_REG_09              0x09
#define SY697X_FORCE_ICO_MASK      0x80
#define SY697X_FORCE_ICO_SHIFT     7
#define SY697X_FORCE_ICO           1
#define SY697X_TMR2X_EN_MASK       0x40
#define SY697X_TMR2X_EN_SHIFT      6
#define SY697X_BATFET_DIS_MASK     0x20
#define SY697X_BATFET_DIS_SHIFT    5
#define SY697X_BATFET_OFF          1
#define SY697X_BATFET_ON			0

#define SY697X_JEITA_VSET_MASK     0x10
#define SY697X_JEITA_VSET_SHIFT    4
#define SY697X_JEITA_VSET_N150MV   0
#define SY697X_JEITA_VSET_VREG     1
#define SY697X_BATFET_RST_EN_MASK  0x04
#define SY697X_BATFET_RST_EN_SHIFT 2
#define SY697X_BATFET_RST_EN_DISABLE 0
#define SY697X_PUMPX_UP_MASK       0x02
#define SY697X_PUMPX_UP_SHIFT      1
#define SY697X_PUMPX_UP            1
#define SY697X_PUMPX_DOWN_MASK     0x01
#define SY697X_PUMPX_DOWN_SHIFT    0
#define SY697X_PUMPX_DOWN          1


/* Register 0x0A*/
#define SY697X_REG_0A              0x0A
#define SY697X_BOOSTV_MASK         0xF0
#define SY697X_BOOSTV_SHIFT        4
#define SY697X_BOOSTV_BASE         4550
#define SY697X_BOOSTV_LSB          64

#define	SY697X_PFM_OTG_DIS_MASK	0x08
#define	SY697X_PFM_OTG_DIS_SHIFT	3


#define SY697X_BOOST_LIM_MASK      0x07
#define SY697X_BOOST_LIM_SHIFT     0
#define SY697X_BOOST_LIM_500MA     0x00
#define SY697X_BOOST_LIM_750MA     0x01
#define SY697X_BOOST_LIM_1200MA    0x02
#define SY697X_BOOST_LIM_1400MA    0x03
#define SY697X_BOOST_LIM_1650MA    0x04
#define SY697X_BOOST_LIM_1875MA    0x05
#define SY697X_BOOST_LIM_2150MA    0x06
#define SY697X_BOOST_LIM_2450MA    0x07


/* Register 0x0B*/
#define SY697X_REG_0B              0x0B
#define SY697X_VBUS_STAT_MASK      0xE0
#define SY697X_VBUS_STAT_SHIFT     5
#define SY697X_VBUS_TYPE_NONE		0
#define SY697X_VBUS_TYPE_SDP		1
#define SY697X_VBUS_TYPE_CDP		2
#define SY697X_VBUS_TYPE_DCP		3
#define SY697X_VBUS_TYPE_HVDCP		4
#define SY697X_VBUS_TYPE_UNKNOWN	5
#define SY697X_VBUS_TYPE_NON_STD	6
#define SY697X_VBUS_TYPE_OTG		7

#define SY697X_CHRG_STAT_MASK      0x18
#define SY697X_CHRG_STAT_SHIFT     3
#define SY697X_CHRG_STAT_IDLE      0
#define SY697X_CHRG_STAT_PRECHG    1
#define SY697X_CHRG_STAT_FASTCHG   2
#define SY697X_CHRG_STAT_CHGDONE   3

#define SY697X_PG_STAT_MASK        0x04
#define SY697X_PG_STAT_SHIFT       2
#define SY697X_SDP_STAT_MASK       0x02
#define SY697X_SDP_STAT_SHIFT      1
#define SY697X_VSYS_STAT_MASK      0x01
#define SY697X_VSYS_STAT_SHIFT     0


/* Register 0x0C*/
#define SY697X_REG_0C              0x0c
#define SY697X_FAULT_WDT_MASK      0x80
#define SY697X_FAULT_WDT_SHIFT     7
#define SY697X_FAULT_BOOST_MASK    0x40
#define SY697X_FAULT_BOOST_SHIFT   6
#define SY697X_FAULT_CHRG_MASK     0x30
#define SY697X_FAULT_CHRG_SHIFT    4
#define SY697X_FAULT_CHRG_NORMAL   0
#define SY697X_FAULT_CHRG_INPUT    1
#define SY697X_FAULT_CHRG_THERMAL  2
#define SY697X_FAULT_CHRG_TIMER    3

#define SY697X_FAULT_BAT_MASK      0x08
#define SY697X_FAULT_BAT_SHIFT     3
#define SY697X_FAULT_NTC_MASK      0x07
#define SY697X_FAULT_NTC_SHIFT     0
#define SY697X_FAULT_NTC_TSCOLD    1
#define SY697X_FAULT_NTC_TSHOT     2

#define SY697X_FAULT_NTC_WARM      2
#define SY697X_FAULT_NTC_COOL      3
#define SY697X_FAULT_NTC_COLD      5
#define SY697X_FAULT_NTC_HOT       6


/* Register 0x0D*/
#define SY697X_REG_0D              0x0D
#define SY697X_FORCE_VINDPM_MASK   0x80
#define SY697X_FORCE_VINDPM_SHIFT  7
#define SY697X_FORCE_VINDPM_ENABLE 1
#define SY697X_FORCE_VINDPM_DISABLE 0
#define SY697X_VINDPM_MASK         0x7F
#define SY697X_VINDPM_SHIFT        0

#define SY697X_VINDPM_BASE         2600
#define SY697X_VINDPM_LSB          100


/* Register 0x0E*/
#define SY697X_REG_0E              0x0E
#define SY697X_THERM_STAT_MASK     0x80
#define SY697X_THERM_STAT_SHIFT    7
#define SY697X_BATV_MASK           0x7F
#define SY697X_BATV_SHIFT          0
#define SY697X_BATV_BASE           2304
#define SY697X_BATV_LSB            20


/* Register 0x0F*/
#define SY697X_REG_0F              0x0F
#define SY697X_SYSV_MASK           0x7F
#define SY697X_SYSV_SHIFT          0
#define SY697X_SYSV_BASE           2304
#define SY697X_SYSV_LSB            20


/* Register 0x10*/
#define SY697X_REG_10              0x10
#define SY697X_TSPCT_MASK          0x7F
#define SY697X_TSPCT_SHIFT         0
#define SY697X_TSPCT_BASE          21
#define SY697X_TSPCT_LSB           465//should be 0.465,kernel does not support float

/* Register 0x11*/
#define SY697X_REG_11              0x11
#define SY697X_VBUS_GD_MASK        0x80
#define SY697X_VBUS_GD_SHIFT       7
#define SY697X_VBUSV_MASK          0x7F
#define SY697X_VBUSV_SHIFT         0
#define SY697X_VBUSV_BASE          2600
#define SY697X_VBUSV_LSB           100


/* Register 0x12*/
#define SY697X_REG_12              0x12
#define SY697X_ICHGR_MASK          0x7F
#define SY697X_ICHGR_SHIFT         0
#define SY697X_ICHGR_BASE          0
#define SY697X_ICHGR_LSB           50


/* Register 0x13*/
#define SY697X_REG_13              0x13
#define SY697X_VDPM_STAT_MASK      0x80
#define SY697X_VDPM_STAT_SHIFT     7
#define SY697X_IDPM_STAT_MASK      0x40
#define SY697X_IDPM_STAT_SHIFT     6
#define SY697X_IDPM_LIM_MASK       0x3F
#define SY697X_IDPM_LIM_SHIFT      0
#define SY697X_IDPM_LIM_BASE       100
#define SY697X_IDPM_LIM_LSB        50


/* Register 0x14*/
#define SY697X_REG_14              0x14
#define SY697X_RESET_MASK          0x80
#define SY697X_RESET_SHIFT         7
#define SY697X_RESET               1
#define SY697X_ICO_OPTIMIZED_MASK  0x40
#define SY697X_ICO_OPTIMIZED_SHIFT 6
#define SY697X_PN_MASK             0x38
#define SY697X_PN_SHIFT            3
#define SY697X_TS_PROFILE_MASK     0x04
#define SY697X_TS_PROFILE_SHIFT    2
#define SY697X_DEV_REV_MASK        0x03
#define SY697X_DEV_REV_SHIFT       0

#define SY697X_INPUT_INIT_CURRENT	500
#define SY697X_CHARGING_INIT_CURRENT	500
#define SY697X_HW_AICL_POINT	4440

int opchg_get_real_charger_type(void);
extern void sgm7220_set_typec_sinkonly(void);
extern void sgm7220_set_typec_cc_open(void);

struct ntc_iio {
	struct iio_channel	*ntc_switch1_chan;
	struct iio_channel	*ntc_switch2_chan;
	struct iio_channel	*ntc_switch3_chan;
	struct iio_channel	*ntc_switch4_chan;

	struct iio_channel	*usb_temp_chan1;
	struct iio_channel	*usb_temp_chan2;
	struct iio_channel	*batt_btb_temp_chan;
};

enum charger_type {
    CHARGER_UNKNOWN = 0,
    STANDARD_HOST,      /* USB : 450mA */
    CHARGING_HOST,
    NONSTANDARD_CHARGER,    /* AC : 450mA~1A */
    STANDARD_CHARGER,   /* AC : ~1A */
    APPLE_2_1A_CHARGER, /* 2.1A apple charger */
    APPLE_1_0A_CHARGER, /* 1A apple charger */
    APPLE_0_5A_CHARGER, /* 0.5A apple charger */
    WIRELESS_CHARGER,
};

enum enum_check_type {
	CHECK_CHARGER_EXIST,
	CHECK_ENUM_STATUS,
};

struct chg_para{
	int vlim;
	int ilim;

	int vreg;
	int ichg;
};

struct sy697x_platform_data {
	int iprechg;
	int iprechg2;
	int iterm;
	int iterm2;

	int boostv;
	int boostv2;

	int boosti;

	int second_chip_addr;

	struct chg_para usb;
};

struct sy697x {
	struct device *dev;
	struct i2c_client *client;
	struct delayed_work sy697x_bc12_retry_work;
	struct delayed_work sy697x_vol_convert_work;
	struct delayed_work sy697x_aicl_work;
	struct delayed_work usb_enum_check_work;
	struct delayed_work init_work;
	int part_no;
	int revision;

	const char *chg_dev_name;
	const char *eint_name;
	struct wakeup_source *suspend_ws;

	/*fix chgtype identify error*/
	struct wakeup_source *keep_resume_ws;
	wait_queue_head_t wait;
	bool chg_det_enable;
	bool otg_enable;

	enum charger_type chg_type;
	enum power_supply_type oplus_chg_type;

	int status;
	int irq;
	int irq_gpio;
	struct pinctrl *pinctrl;
	struct pinctrl_state *splitchg_inter_active;
	struct pinctrl_state *splitchg_inter_sleep;
	struct mutex i2c_rw_lock;

	bool otg_switch;
	int ccdetect_gpio;
	int ccdetect_irq;
	struct pinctrl_state *ccdetect_active;
	struct pinctrl_state *ccdetect_sleep;

	bool charge_enabled;	/* Register bit status */
	bool power_good;
	bool chg_need_check;
	struct sy697x_platform_data *platform_data;
#ifdef CONFIG_OPLUS_CHARGER_MTK
	struct charger_device *chg_dev;
	struct charger_consumer *chg_consumer;
#endif /*CONFIG_OPLUS_CHARGER_MTK*/
	struct timespec64 st_ptime[2];

	struct power_supply *psy;
	bool disable_hight_vbus;
	bool pdqc_setup_5v;
	bool is_bc12_end;
	bool is_force_aicl;
	bool hvdcp_can_enabled;
	int pre_current_ma;
	int aicr;
	int vbus_type;
	bool hvdcp_checked;
	bool is_force_dpdm;
	bool cdp_retry;
	bool sdp_retry;
	bool chg_start_check;
	bool is_retry_bc12;
	bool cdp_retry_aicl;
	bool usb_connect_start;
	int boot_mode;
	bool vbus_on;
	int irq_handler_count;
	int hw_aicl_point;
	int sw_aicl_point;
	char bc12_delay_cnt;
	char bc12_retried;
	bool bc12_done;

	/*oplus add platform difference of qcom and mtk*/
	/*for mtk*/

	/*for qcom*/
	/* extcon for VBUS / ID notification to usbphy */
	struct extcon_dev	*extcon;
	int			connector_type;
	int typec_port;
	bool otg_online;
	bool otg_present;
	struct regulator	*dpdm_reg;
	struct mutex		dpdm_lock;
	bool			dpdm_enabled;

	/*split ntc channels*/
	struct ntc_iio iio;
	int ntcctrl_gpio;
	int ntcctrl_gpio_amux;
	struct mutex ntc_lock;
	struct pinctrl_state *ntc_switch_low;
	struct pinctrl_state *ntc_switch_high;
	struct pinctrl_state *ntc_switch_amux_low;
	struct pinctrl_state *ntc_switch_amux_high;

	atomic_t		driver_suspended;
	atomic_t		charger_suspended;
	int			before_suspend_icl;
	int			before_unsuspend_icl;
	struct oplus_chg_chip *oplus_chgchip;
	const struct oplus_chgic_operations *chgic_ops;
#if IS_ENABLED(CONFIG_TCPC_CLASS)
	struct tcpc_device *tcpc;
	int pd_type;
#endif
};


struct oplus_chgic_operations {
	void (*typec_sink_removal)(void);
	void (*typec_src_removal)(void);
	void (*typec_sink_insertion)(void);
	bool (*get_otg_switch_status)(void);
	void (*set_otg_switch_status)(bool);
	int (*get_otg_online_status)(void);
	int (*thermal_tmp_get_chg)(void);
	int (*thermal_tmp_get_bb)(void);
	int (*thermal_tmp_get_flash)(void);
	int (*thermal_tmp_get_board)(void);
	int (*thermal_tmp_get_pa)(void);
	int (*thermal_tmp_get_batt)(void);
	int (*thermal_tmp_get_vbus_btb)(void);
	int (*thermal_tmp_get_batt_btb)(void);
	int (*get_usb_status)(void);
	int (*get_typec_cc_orientation)(void);
};

#define ADAPTER_CAP_MAX_NR 10
struct adapter_power_cap {
	uint8_t selected_cap_idx;
	uint8_t nr;
	uint8_t pdp;
	uint8_t pwr_limit[ADAPTER_CAP_MAX_NR];
	int max_mv[ADAPTER_CAP_MAX_NR];
	int min_mv[ADAPTER_CAP_MAX_NR];
	int ma[ADAPTER_CAP_MAX_NR];
	int maxwatt[ADAPTER_CAP_MAX_NR];
	int minwatt[ADAPTER_CAP_MAX_NR];
	uint8_t type[ADAPTER_CAP_MAX_NR];
	int info[ADAPTER_CAP_MAX_NR];
};
enum adapter_cap_type {
	MTK_PD_APDO_START,
	MTK_PD_APDO_END,
	MTK_PD,
	MTK_PD_APDO,
	MTK_CAP_TYPE_UNKNOWN,
 };

#endif
