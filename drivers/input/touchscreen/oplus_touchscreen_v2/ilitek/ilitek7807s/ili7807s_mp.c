// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include "ili7807s.h"

#define VALUE               0
#define RETRY_COUNT         3
#define INT_CHECK           0
#define POLL_CHECK          1

#define BENCHMARK           1
#define NODETYPE            1

#define TYPE_BENCHMARK      0
#define TYPE_NO_JUGE        1
#define TYPE_JUGE           2

#define NORMAL_CSV_PASS_NAME        "mp_pass"
#define NORMAL_CSV_FAIL_NAME        "mp_fail"
#define NORMAL_CSV_WARNING_NAME     "mp_warning"

#define CSV_FILE_SIZE               (1 * M)

#define CMD_MUTUAL_DAC              0x1
#define CMD_MUTUAL_BG               0x2
#define CMD_MUTUAL_SIGNAL           0x3
#define CMD_MUTUAL_NO_BK            0x5
#define CMD_MUTUAL_HAVE_BK          0x8
#define CMD_MUTUAL_BK_DAC           0x10
#define CMD_SELF_DAC                0xC
#define CMD_SELF_BG                 0xF
#define CMD_SELF_SIGNAL             0xD
#define CMD_SELF_NO_BK              0xE
#define CMD_SELF_HAVE_BK            0xB
#define CMD_SELF_BK_DAC             0x11
#define CMD_KEY_DAC                 0x14
#define CMD_KEY_BG                  0x16
#define CMD_KEY_NO_BK               0x7
#define CMD_KEY_HAVE_BK             0x15
#define CMD_KEY_OPEN                0x12
#define CMD_KEY_SHORT               0x13
#define CMD_ST_DAC                  0x1A
#define CMD_ST_BG                   0x1C
#define CMD_ST_NO_BK                0x17
#define CMD_ST_HAVE_BK              0x1B
#define CMD_ST_OPEN                 0x18
#define CMD_TX_SHORT                0x19
#define CMD_RX_SHORT                0x4
#define CMD_RX_OPEN                 0x6
#define CMD_TX_RX_DELTA             0x1E
#define CMD_CM_DATA                 0x9
#define CMD_CS_DATA                 0xA
#define CMD_TRCRQ_PIN               0x20
#define CMD_RESX2_PIN               0x21
#define CMD_MUTUAL_INTEGRA_TIME     0x22
#define CMD_SELF_INTEGRA_TIME       0x23
#define CMD_KEY_INTERGRA_TIME       0x24
#define CMD_ST_INTERGRA_TIME        0x25
#define CMD_PEAK_TO_PEAK            0x1D
#define CMD_GET_TIMING_INFO         0x30
#define CMD_DOZE_P2P                0x32
#define CMD_DOZE_RAW                0x33
#define CMD_PIN_TEST                0x61

#define MP_DATA_PASS                0
#define MP_DATA_FAIL               -1

#define COMPARE_MAX                 999999999
#define COMPARE_MIN                -999999999
#define Mathabs(x) ({                   \
        long ret;               \
		if (sizeof(x) == sizeof(long)) {    \
			long __x = (x);             \
			ret = (__x < 0) ? -__x : __x;       \
		} else {                \
			int __x = (x);              \
			ret = (__x < 0) ? -__x : __x;       \
		}                   \
		ret;                    \
	})

#define DUMP(fmt, arg...)       \
	do {                \
		if (ili_debug_en)   \
			pr_cont(fmt, ##arg);    \
	} while (0)

enum mp_test_catalog {
	MUTUAL_TEST = 0,
	SELF_TEST = 1,
	KEY_TEST = 2,
	ST_TEST = 3,
	TX_RX_DELTA = 4,
	UNTOUCH_P2P = 5,
	PIXEL = 6,
	OPEN_TEST = 7,
	PEAK_TO_PEAK_TEST = 8,
	SHORT_TEST = 9,
	PIN_TEST = 10,
};

struct mp_test_open_c {
	s32 *cap_dac;
	s32 *cap_raw;
	s32 *dcl_cap;
};

struct mp_test_items {
	/* The description must be the same as ini's section name */
	char *desp;
	char *result;
	int catalog;
	u8 cmd;
	u8 spec_option;
	u8 type_option;
	bool run;
	bool lcm;
	int bch_mrk_multi;
	int max;
	int max_res;
	int item_result;
	int min;
	int min_res;
	int frame_count;
	int trimmed_mean;
	int lowest_percentage;
	int highest_percentage;
	int v_tdf_1;
	int v_tdf_2;
	int h_tdf_1;
	int h_tdf_2;


	int tvch;
	int tvcl;
	int gain;
	int cbk_step;
	int cint;
	int accuracy;

	int variation;
	int rinternal;

	int goldenmode;
	int max_min_mode;
	int bch_mrk_frm_num;
	int retry_cnt;
	u8  delay_time;
	u8  test_int_pin;
	u8  int_pulse_test;
	s32 *result_buf;
	s32 *buf;
	s32 *max_buf;
	s32 *min_buf;
	s32 *bench_mark_max;
	s32 *bench_mark_min;
	s32 *bch_mrk_max[8];
	s32 *bch_mrk_min[8];
	s32 *node_type;
	int (*do_test)(int index);

	int test_index;
	u8 p5v_cmd[16];
};

/*static struct core_mp_test_data core_mp = {0};*/

#define MP_TEST_ITEM    48
static struct mp_test_items tItems[MP_TEST_ITEM] = {
	{.desp = "baseline data(bg)", .catalog = MUTUAL_TEST, .cmd = CMD_MUTUAL_BG, .lcm = ON},
	{.desp = "untouch signal data(bg-raw-4096) - mutual", .catalog = MUTUAL_TEST, .cmd = CMD_MUTUAL_SIGNAL, .lcm = ON},
	{.desp = "manual bk data(mutual)", .catalog = MUTUAL_TEST, .cmd = CMD_MUTUAL_BK_DAC, .lcm = ON},
	{.desp = "calibration data(dac) - self", .catalog = SELF_TEST, .cmd = CMD_SELF_DAC, .lcm = ON},
	{.desp = "baselin data(bg,self_tx,self_r)", .catalog = SELF_TEST, .cmd = CMD_SELF_BG, .lcm = ON},
	{.desp = "untouch signal data(bg-raw-4096) - self", .catalog = SELF_TEST, .cmd = CMD_SELF_SIGNAL, .lcm = ON},
	{.desp = "raw data(no bk) - self", .catalog = SELF_TEST, .cmd = CMD_SELF_NO_BK, .lcm = ON},
	{.desp = "raw data(have bk) - self", .catalog = SELF_TEST, .cmd = CMD_SELF_HAVE_BK, .lcm = ON},
	{.desp = "manual bk dac data(self_tx,self_rx)", .catalog = SELF_TEST, .cmd = CMD_SELF_BK_DAC, .lcm = ON},
	{.desp = "calibration data(dac/icon)", .catalog = KEY_TEST, .cmd = CMD_KEY_DAC, .lcm = ON},
	{.desp = "key baseline data", .catalog = KEY_TEST, .cmd = CMD_KEY_BG, .lcm = ON},
	{.desp = "key raw data", .catalog = KEY_TEST, .cmd = CMD_KEY_NO_BK, .lcm = ON},
	{.desp = "key raw bk dac", .catalog = KEY_TEST, .cmd = CMD_KEY_HAVE_BK, .lcm = ON},
	{.desp = "key raw open test", .catalog = KEY_TEST, .cmd = CMD_KEY_OPEN, .lcm = ON},
	{.desp = "key raw short test", .catalog = KEY_TEST, .cmd = CMD_KEY_SHORT, .lcm = ON},
	{.desp = "st calibration data(dac)", .catalog = ST_TEST, .cmd = CMD_ST_DAC, .lcm = ON},
	{.desp = "st baseline data(bg)", .catalog = ST_TEST, .cmd = CMD_ST_BG, .lcm = ON},
	{.desp = "st raw data(no bk)", .catalog = ST_TEST, .cmd = CMD_ST_NO_BK, .lcm = ON},
	{.desp = "st raw(have bk)", .catalog = ST_TEST, .cmd = CMD_ST_HAVE_BK, .lcm = ON},
	{.desp = "st open data", .catalog = ST_TEST, .cmd = CMD_ST_OPEN, .lcm = ON},
	{.desp = "tx short test", .catalog = MUTUAL_TEST, .cmd = CMD_TX_SHORT, .lcm = ON},
	{.desp = "rx open", .catalog = MUTUAL_TEST, .cmd = CMD_RX_OPEN, .lcm = ON},
	{.desp = "untouch cm data", .catalog = MUTUAL_TEST, .cmd = CMD_CM_DATA, .lcm = ON},
	{.desp = "untouch cs data", .catalog = MUTUAL_TEST, .cmd = CMD_CS_DATA, .lcm = ON},
	{.desp = "tx/rx delta", .catalog = TX_RX_DELTA, .cmd = CMD_TX_RX_DELTA, .lcm = ON},
	{.desp = "untouch peak to peak", .catalog = UNTOUCH_P2P, .cmd = CMD_MUTUAL_SIGNAL, .lcm = ON},
	{.desp = "pixel raw (no bk)", .catalog = PIXEL, .cmd = CMD_MUTUAL_NO_BK, .lcm = ON},
	{.desp = "pixel raw (have bk)", .catalog = PIXEL, .cmd = CMD_MUTUAL_HAVE_BK, .lcm = ON},
	{.desp = "noise peak to peak(cut panel)", .catalog = PEAK_TO_PEAK_TEST, .lcm = ON},
	{.desp = "open test(integration)", .catalog = OPEN_TEST, .cmd = CMD_RX_SHORT, .lcm = ON},
	{.desp = "open test(cap)", .catalog = OPEN_TEST, .cmd = CMD_RX_SHORT, .lcm = ON},
	/* Following is the new test items for protocol 5.4.0 above */
	{.desp = "pin test ( int and rst )", .catalog = PIN_TEST, .cmd = CMD_PIN_TEST, .lcm = ON},
	{.desp = "noise peak to peak(with panel)", .catalog = PEAK_TO_PEAK_TEST, .lcm = ON, .test_index = TYPE_TEST3},
	{.desp = "noise peak to peak(ic only)", .catalog = PEAK_TO_PEAK_TEST, .cmd = CMD_PEAK_TO_PEAK, .lcm = ON, .test_index = TYPE_TEST2},
	/*{.desp = "open test(integration)_sp", .catalog = OPEN_TEST, .lcm = ON},*/
	{.desp = "raw data(no bk)", .catalog = MUTUAL_TEST, .cmd = CMD_MUTUAL_NO_BK, .lcm = ON, .test_index = TYPE_TEST4},
	{.desp = "raw data(have bk)", .catalog = MUTUAL_TEST, .cmd = CMD_MUTUAL_HAVE_BK, .lcm = ON, .test_index = TYPE_TEST5},
	{.desp = "calibration data(dac)", .catalog = MUTUAL_TEST, .cmd = CMD_MUTUAL_DAC, .lcm = ON, .test_index = TYPE_TEST6},
	/*{.desp = "short test -ili9881", .catalog = SHORT_TEST, .cmd = CMD_RX_SHORT, .lcm = ON},*/
	{.desp = "short test", .catalog = SHORT_TEST, .lcm = ON, .test_index = TYPE_TEST7},
	{.desp = "doze raw data", .catalog = MUTUAL_TEST, .lcm = ON, .test_index = TYPE_TEST8},
	{.desp = "doze peak to peak", .catalog = PEAK_TO_PEAK_TEST, .lcm = ON, .test_index = TYPE_TEST9},
	{.desp = "open test_c", .catalog = OPEN_TEST, .lcm = ON, .test_index = TYPE_TEST10},
	{.desp = "touch deltac", .catalog = MUTUAL_TEST, .lcm = ON, .test_index = TYPE_TEST11},
	/* LCM OFF TEST */
	{.desp = "raw data(have bk) (lcm off)", .catalog = MUTUAL_TEST, .lcm = OFF, .test_index = TYPE_TEST15},
	{.desp = "raw data(no bk) (lcm off)", .catalog = MUTUAL_TEST, .lcm = OFF, .test_index = TYPE_TEST14},
	{.desp = "noise peak to peak(with panel) (lcm off)", .catalog = PEAK_TO_PEAK_TEST, .lcm = OFF, .test_index = TYPE_TEST13},
	{.desp = "noise peak to peak(ic only) (lcm off)", .catalog = PEAK_TO_PEAK_TEST, .lcm = OFF, .test_index = TYPE_TEST12},
	{.desp = "raw data_td (lcm off)", .catalog = MUTUAL_TEST, .lcm = OFF, .test_index = TYPE_TEST16},
	{.desp = "peak to peak_td (lcm off)", .catalog = PEAK_TO_PEAK_TEST, .lcm = OFF, .test_index = TYPE_TEST17},
};

static s32 *frame_buf;
static s32 **frm_buf;
static s32 *key_buf;
static s32 *cap_dac, *cap_raw;

unsigned int __attribute__((weak)) get_project(void)
{
	return 1;
}

static int  ilitek_get_item_para(struct auto_testdata *testdata,
				 struct mp_test_items *p_mp_test_items, int item_index)
{
	struct test_item_info *p_test_item_info = NULL;
	int ret = 0;
	int i  = 0;

	if (!testdata || !p_mp_test_items) {
		TPD_INFO("item: %d testdata or p_mp_test_items is null\n", item_index);
		return -1;
	}

	p_test_item_info = get_test_item_info(testdata->fw, item_index);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", item_index);
		return -1;

	} else {
		if (p_test_item_info->p_buffer[0] == 1 && p_test_item_info->para_num >= 32) {
			/*p5_4 cmd*/
			for (i = 0; i < (ARRAY_SIZE(p_mp_test_items->p5v_cmd)); i++) {
				p_mp_test_items->p5v_cmd[i] = (u8) p_test_item_info->p_buffer[i + 1];
			}

			p_mp_test_items->run = p_test_item_info->p_buffer[17];
			p_mp_test_items->spec_option = (u8) p_test_item_info->p_buffer[18];
			p_mp_test_items->type_option = (u8) p_test_item_info->p_buffer[19];
			p_mp_test_items->max = p_test_item_info->p_buffer[20];
			p_mp_test_items->min = p_test_item_info->p_buffer[21];
			p_mp_test_items->frame_count = p_test_item_info->p_buffer[22];
			p_mp_test_items->trimmed_mean = p_test_item_info->p_buffer[23];
			p_mp_test_items->lowest_percentage = p_test_item_info->p_buffer[24];
			p_mp_test_items->highest_percentage = p_test_item_info->p_buffer[25];
			p_mp_test_items->v_tdf_1 = p_test_item_info->p_buffer[26];
			p_mp_test_items->v_tdf_2 = p_test_item_info->p_buffer[27];
			p_mp_test_items->h_tdf_1 = p_test_item_info->p_buffer[28];
			p_mp_test_items->h_tdf_2 = p_test_item_info->p_buffer[29];
			p_mp_test_items->tvch = p_test_item_info->p_buffer[30];
			p_mp_test_items->tvcl = p_test_item_info->p_buffer[31];
			p_mp_test_items->gain = p_test_item_info->p_buffer[32];
			p_mp_test_items->cbk_step = p_test_item_info->p_buffer[33];
			p_mp_test_items->cint = p_test_item_info->p_buffer[34];
			p_mp_test_items->accuracy = p_test_item_info->p_buffer[35];
			p_mp_test_items->variation = p_test_item_info->p_buffer[36];
			p_mp_test_items->rinternal = p_test_item_info->p_buffer[37];
			p_mp_test_items->goldenmode = p_test_item_info->p_buffer[38];
			p_mp_test_items->max_min_mode = p_test_item_info->p_buffer[39];
			p_mp_test_items->retry_cnt = p_test_item_info->p_buffer[40];
			TPD_DEBUG("item: %d,"
				  "run:%d,spec_option:%d,"
				  "type_option:%d,max:%d,"
				  "min:%d,frame_count:%d,"
				  "trimmed_mean:%d,lowest_percentage:%d,"
				  "highest_percentage:%d,v_tdf_1:%d,"
				  "v_tdf_2:%d,h_tdf_1:%d,"
				  "h_tdf_2:%d,tvch:%d,"
				  "tvcl:%d,gain:%d,"
				  "cbk_step:%d,variation:%d,"
				  "cint:%d,rinternal:%d,"
				  "accuracy:%d,goldenmode:%d,"
				  "max_min_mode:%d,retry_cnt:%d\n", item_index,
				  p_mp_test_items->run, p_mp_test_items->spec_option,
				  p_mp_test_items->type_option, p_mp_test_items->max,
				  p_mp_test_items->min, p_mp_test_items->frame_count,
				  p_mp_test_items->trimmed_mean, p_mp_test_items->lowest_percentage,
				  p_mp_test_items->highest_percentage, p_mp_test_items->v_tdf_1,
				  p_mp_test_items->v_tdf_2, p_mp_test_items->h_tdf_1,
				  p_mp_test_items->h_tdf_2, p_mp_test_items->tvch,
				  p_mp_test_items->tvcl, p_mp_test_items->gain,
				  p_mp_test_items->cbk_step, p_mp_test_items->variation,
				  p_mp_test_items->cint, p_mp_test_items->rinternal,
				  p_mp_test_items->accuracy, p_mp_test_items->goldenmode,
				  p_mp_test_items->max_min_mode, p_mp_test_items->retry_cnt
				 );
			/*TPD_DEBUG("p5v_cmd:%*ph\n", sizeof(p_mp_test_items->p5v_cmd), p_mp_test_items->p5v_cmd);*/
		}

		if ((p_mp_test_items->spec_option == BENCHMARK)
				&& (p_mp_test_items->catalog == PEAK_TO_PEAK_TEST)) {
			for (i = 0; i < p_mp_test_items->bch_mrk_frm_num; i++) {
				p_mp_test_items->bch_mrk_max[i] = (uint32_t *)(testdata->fw->data +
								  p_test_item_info->top_limit_offset);
				p_mp_test_items->bch_mrk_min[i] = (uint32_t *)(testdata->fw->data +
								  p_test_item_info->floor_limit_offset);
			}

		} else {
			if (p_test_item_info->item_limit_type == LIMIT_TYPE_TX_RX_DATA) {
				p_mp_test_items->bench_mark_max = (uint32_t *)(testdata->fw->data +
								  p_test_item_info->top_limit_offset);
				p_mp_test_items->bench_mark_min = (uint32_t *)(testdata->fw->data +
								  p_test_item_info->floor_limit_offset);
			}
		}
	}

	tp_kfree((void **)&p_test_item_info);
	return ret;
}

static int  ilitek_get_para_openshort(struct auto_testdata *testdata,
				      struct mp_test_items *p_mp_test_items,
				      struct open_test_para *p_open_test,
				      int item_index)
{
	struct test_item_info *p_test_item_info = NULL;
	int ret = 0;
	int j  = 0;

	if (!testdata || !p_mp_test_items || !p_open_test) {
		TPD_INFO("item: %d auto_testdata or mp_test_items  open_test_c_spec is null\n",
			 item_index);
		return -1;
	}

	p_test_item_info = get_test_item_info(testdata->fw, item_index);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", item_index);
		return -1;

	} else {
		if (p_test_item_info->p_buffer[0] == 1 && p_test_item_info->para_num >= 50) {
			/*p5_4 cmd*/
			for (j = 0; j < (ARRAY_SIZE(p_open_test->cap1_dac_cmd)); j++) {
				p_open_test->cap1_dac_cmd[j] = (u8) p_test_item_info->p_buffer[j + 1];
			}

			/*p5_4 cmd*/
			for (j = 0; j < (ARRAY_SIZE(p_open_test->cap1_raw_cmd)); j++) {
				p_open_test->cap1_raw_cmd[j] = (u8) p_test_item_info->p_buffer[j + 1 +
							       ARRAY_SIZE(
								       p_open_test->cap1_dac_cmd)];
			}

			p_mp_test_items->run = p_test_item_info->p_buffer[33];
			p_mp_test_items->spec_option = (u8) p_test_item_info->p_buffer[34];
			p_mp_test_items->type_option = (u8) p_test_item_info->p_buffer[35];
			p_mp_test_items->max = p_test_item_info->p_buffer[36];
			p_mp_test_items->min = p_test_item_info->p_buffer[37];
			p_mp_test_items->frame_count = p_test_item_info->p_buffer[38];
			p_mp_test_items->trimmed_mean = p_test_item_info->p_buffer[39];
			p_mp_test_items->lowest_percentage = p_test_item_info->p_buffer[40];
			p_mp_test_items->highest_percentage = p_test_item_info->p_buffer[41];
			p_mp_test_items->v_tdf_1 = p_test_item_info->p_buffer[42];
			p_mp_test_items->v_tdf_2 = p_test_item_info->p_buffer[43];
			p_mp_test_items->h_tdf_1 = p_test_item_info->p_buffer[44];
			p_mp_test_items->h_tdf_2 = p_test_item_info->p_buffer[45];
			p_mp_test_items->tvch = p_test_item_info->p_buffer[46];
			p_mp_test_items->tvcl = p_test_item_info->p_buffer[47];
			p_mp_test_items->gain = p_test_item_info->p_buffer[48];
			p_mp_test_items->cbk_step = p_test_item_info->p_buffer[49];
			p_mp_test_items->cint = p_test_item_info->p_buffer[50];
			p_mp_test_items->accuracy = p_test_item_info->p_buffer[51];
			p_mp_test_items->variation = p_test_item_info->p_buffer[52];
			p_mp_test_items->rinternal = p_test_item_info->p_buffer[53];
			p_mp_test_items->goldenmode = p_test_item_info->p_buffer[54];
			p_mp_test_items->max_min_mode = p_test_item_info->p_buffer[55];
			p_mp_test_items->retry_cnt = p_test_item_info->p_buffer[56];
			/*p_open_test->tvch = p_test_item_info->p_buffer[48];
			p_open_test->tvcl = p_test_item_info->p_buffer[49];
			p_open_test->gain = p_test_item_info->p_buffer[50];
			*/
			TPD_DEBUG("item: %d,"
				  "run:%d,spec_option:%d,"
				  "type_option:%d,max:%d,"
				  "min:%d,frame_count:%d,"
				  "trimmed_mean:%d,lowest_percentage:%d,"
				  "highest_percentage:%d,v_tdf_1:%d,"
				  "v_tdf_2:%d,h_tdf_1:%d,"
				  "h_tdf_2:%d,tvch:%d,"
				  "tvcl:%d,gain:%d,"
				  "cbk_step:%d,variation:%d,"
				  "cint:%d,rinternal:%d,"
				  "accuracy:%d,goldenmode:%d,"
				  "max_min_mode:%d,retry_cnt:%d\n", item_index,
				  p_mp_test_items->run, p_mp_test_items->spec_option,
				  p_mp_test_items->type_option, p_mp_test_items->max,
				  p_mp_test_items->min, p_mp_test_items->frame_count,
				  p_mp_test_items->trimmed_mean, p_mp_test_items->lowest_percentage,
				  p_mp_test_items->highest_percentage, p_mp_test_items->v_tdf_1,
				  p_mp_test_items->v_tdf_2, p_mp_test_items->h_tdf_1,
				  p_mp_test_items->h_tdf_2, p_mp_test_items->tvch,
				  p_mp_test_items->tvcl, p_mp_test_items->gain,
				  p_mp_test_items->cbk_step, p_mp_test_items->variation,
				  p_mp_test_items->cint, p_mp_test_items->rinternal,
				  p_mp_test_items->accuracy, p_mp_test_items->goldenmode,
				  p_mp_test_items->max_min_mode, p_mp_test_items->retry_cnt
				 );
		}

		if (p_test_item_info->item_limit_type == LIMIT_TYPE_TX_RX_DATA) {
			p_mp_test_items->bench_mark_max = (uint32_t *)(testdata->fw->data +
							  p_test_item_info->top_limit_offset);
			p_mp_test_items->bench_mark_min = (uint32_t *)(testdata->fw->data +
							  p_test_item_info->floor_limit_offset);
		}
	}

	tp_kfree((void **)&p_test_item_info);
	return ret;
}

static void dump_benchmark_data(s32 *max_ptr, s32 *min_ptr)
{
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	ili_dump_data(max_ptr, 32, core_mp->frame_len, core_mp->xch_len,
		      "Dump Benchmark Max : ");
	ili_dump_data(min_ptr, 32, core_mp->frame_len, core_mp->xch_len,
		      "Dump Benchmark Min : ");
}

static void run_pixel_test(int index)
{
	int i, x, y;
	s32 *p_comb = frame_buf;
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	for (y = 0; y < core_mp->ych_len; y++) {
		for (x = 0; x < core_mp->xch_len; x++) {
			int tmp[4] = { 0 }, max = 0;
			int shift = y * core_mp->xch_len;
			int centre = p_comb[shift + x];

			/*
			 * if its position is in corner, the number of point
			 * we have to minus is around 2 to 3.
			 */
			if (y == 0 && x == 0) {
				tmp[0] = Mathabs(centre - p_comb[(shift + 1) + x]); /* down */
				tmp[1] = Mathabs(centre - p_comb[shift + (x + 1)]); /* right */

			} else if (y == (core_mp->ych_len - 1) && x == 0) {
				tmp[0] = Mathabs(centre - p_comb[(shift - 1) + x]); /* up */
				tmp[1] = Mathabs(centre - p_comb[shift + (x + 1)]); /* right */

			} else if (y == 0 && x == (core_mp->xch_len - 1)) {
				tmp[0] = Mathabs(centre - p_comb[(shift + 1) + x]); /* down */
				tmp[1] = Mathabs(centre - p_comb[shift + (x - 1)]); /* left */

			} else if (y == (core_mp->ych_len - 1) && x == (core_mp->xch_len - 1)) {
				tmp[0] = Mathabs(centre - p_comb[(shift - 1) + x]); /* up */
				tmp[1] = Mathabs(centre - p_comb[shift + (x - 1)]); /* left */

			} else if (y == 0 && x != 0) {
				tmp[0] = Mathabs(centre - p_comb[(shift + 1) + x]); /* down */
				tmp[1] = Mathabs(centre - p_comb[shift + (x - 1)]); /* left */
				tmp[2] = Mathabs(centre - p_comb[shift + (x + 1)]); /* right */

			} else if (y != 0 && x == 0) {
				tmp[0] = Mathabs(centre - p_comb[(shift - 1) + x]); /* up */
				tmp[1] = Mathabs(centre - p_comb[shift + (x + 1)]); /* right */
				tmp[2] = Mathabs(centre - p_comb[(shift + 1) + x]); /* down */

			} else if (y == (core_mp->ych_len - 1) && x != 0) {
				tmp[0] = Mathabs(centre - p_comb[(shift - 1) + x]); /* up */
				tmp[1] = Mathabs(centre - p_comb[shift + (x - 1)]); /* left */
				tmp[2] = Mathabs(centre - p_comb[shift + (x + 1)]); /* right */

			} else if (y != 0 && x == (core_mp->xch_len - 1)) {
				tmp[0] = Mathabs(centre - p_comb[(shift - 1) + x]); /* up */
				tmp[1] = Mathabs(centre - p_comb[shift + (x - 1)]); /* left */
				tmp[2] = Mathabs(centre - p_comb[(shift + 1) + x]); /* down */

			} else {
				/* middle minus four directions */
				tmp[0] = Mathabs(centre - p_comb[(shift - 1) + x]); /* up */
				tmp[1] = Mathabs(centre - p_comb[(shift + 1) + x]); /* down */
				tmp[2] = Mathabs(centre - p_comb[shift + (x - 1)]); /* left */
				tmp[3] = Mathabs(centre - p_comb[shift + (x + 1)]); /* right */
			}

			max = tmp[0];

			for (i = 0; i < 4; i++) {
				if (tmp[i] > max) {
					max = tmp[i];
				}
			}

			tItems[index].buf[shift + x] = max;
		}
	}
}

static void run_untouch_p2p_test(int index)
{
	int x, y;
	s32 *p_comb = frame_buf;
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	for (y = 0; y < core_mp->ych_len; y++) {
		for (x = 0; x < core_mp->xch_len; x++) {
			int shift = y * core_mp->xch_len;

			if (p_comb[shift + x] > tItems[index].max_buf[shift + x]) {
				tItems[index].max_buf[shift + x] = p_comb[shift + x];
			}

			if (p_comb[shift + x] < tItems[index].min_buf[shift + x]) {
				tItems[index].min_buf[shift + x] = p_comb[shift + x];
			}

			tItems[index].buf[shift + x] =
				tItems[index].max_buf[shift + x] - tItems[index].min_buf[shift + x];
		}
	}
}

static int run_open_test(int index)
{
	int i, x, y, k, ret = 0;
	int border_x[] = { -1, 0, 1, 1, 1, 0, -1, -1};
	int border_y[] = { -1, -1, -1, 0, 1, 1, 1, 0};
	s32 *p_comb = frame_buf;
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	if (ipio_strcmp(tItems[index].desp, "open test(integration)") == 0) {
		for (i = 0; i < core_mp->frame_len; i++) {
			tItems[index].buf[i] = p_comb[i];
		}

	} else if (ipio_strcmp(tItems[index].desp, "open test(cap)") == 0) {
		/*
		 * Each result is getting from a 3 by 3 grid depending on where the centre location is.
		 * So if the centre is at corner, the number of node grabbed from a grid will be different.
		 */
		for (y = 0; y < core_mp->ych_len; y++) {
			for (x = 0; x < core_mp->xch_len; x++) {
				int sum = 0, avg = 0, count = 0;
				int shift = y * core_mp->xch_len;
				int centre = p_comb[shift + x];

				for (k = 0; k < 8; k++) {
					if (((y + border_y[k] >= 0) && (y + border_y[k] < core_mp->ych_len)) &&
							((x + border_x[k] >= 0) && (x + border_x[k] < core_mp->xch_len))) {
						count++;
						sum += p_comb[(y + border_y[k]) * core_mp->xch_len + (x + border_x[k])];
					}
				}

				avg = (sum + centre) / (count + 1); /* plus 1 because of centre */
				tItems[index].buf[shift + x] = (centre * 100) / avg;
			}
		}
	}

	return ret;
}

static void run_tx_rx_delta_test(int index)
{
	int x, y;
	s32 *p_comb = frame_buf;
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	for (y = 0; y < core_mp->ych_len; y++) {
		for (x = 0; x < core_mp->xch_len; x++) {
			int shift = y * core_mp->xch_len;

			/* Tx Delta */
			if (y != (core_mp->ych_len - 1)) {
				core_mp->tx_delta_buf[shift + x] = Mathabs(p_comb[shift + x] - p_comb[(shift +
								   1) + x]);
			}

			/* Rx Delta */
			if (x != (core_mp->xch_len - 1)) {
				core_mp->rx_delta_buf[shift + x] = Mathabs(p_comb[shift + x] - p_comb[shift +
								   (x + 1)]);
			}
		}
	}
}

static void mp_print_csv_header(char *csv, int *csv_len, int *csv_line,
				int file_size)
{
	int i, j, tmp_len = *csv_len, tmp_line = *csv_line;
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	/* header must has 19 line*/
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len),
			    "==============================================================================\n");
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len),
			    "ILITek C-TP Utility V%s	%x : Driver Sensor Test\n", DRIVER_VERSION,
			    core_mp->chip_pid);
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len),
			    "Confidentiality Notice:\n");
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len),
			    "Any information of this tool is confidential and privileged.\n");
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len),
			    "@ ILI TECHNOLOGY CORP. All Rights Reserved.\n");
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len),
			    "==============================================================================\n");
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len, "oplus_%d\n",
			    get_project());
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
			    "Test Platform ,Mobile\n");
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
			    "This is %s module\n",
			    ilits->ts->panel_data.manufacture_info.manufacture);
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len),
			    "Firmware Version ,0x%x\n",
			    core_mp->fw_ver);
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len),
			    "Panel information ,XCH=%d, YCH=%d\n",
			    core_mp->xch_len, core_mp->ych_len);
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len),
			    "INI Release Version ,%d\n",
			    core_mp->ini_ver);
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len),
			    "INI Release Date ,%d\n",
			    core_mp->ini_date);
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "Test Item:\n");
	tmp_line++;

	for (i = 0; i < TYPE_MAX; i++) {
		if (!core_mp->test_index[i]) {
			break;
		}

		for (j = 0; j < MP_TEST_ITEM; j++) {
			if (core_mp->test_index[i] == tItems[j].test_index && tItems[j].run) {
				if (tItems[j].item_result == MP_DATA_PASS) {
					tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "   ---%s, OK\n",
							    tItems[j].desp);

				} else {
					tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "   ---%s, NG\n",
							    tItems[j].desp);
				}

				tmp_line++;
				break;
			}
		}
	}

	/* print final result*/
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
			    "Final Result ,%s\n",
			    (core_mp->final_result == MP_DATA_PASS) ? "PASS" : "FAIL");

	while (tmp_line < 19) {
		tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "\n");
		tmp_line++;
	}

	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len),
			    "==============================================================================\n");
	*csv_len = tmp_len;
	*csv_line = tmp_line;
}

static void mp_print_csv_tail(char *csv, int *csv_len, int file_size)
{
	int tmp_len = *csv_len;
	tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len),
			    "==============================================================================\n");
	*csv_len = tmp_len;
}

static void mp_print_csv_cdc_cmd(char *csv, int *csv_len, int index,
				 int file_size)
{
	int i, j = 0, j_len = 0, tmp_len = *csv_len, size;
	char str[128] = {0};
	char *open_c_cmd[] = {"open cap1 dac", "open cap1 raw"};
	unsigned char open_c_cmd_data[16] = {0};/*OPEN CAP1 DAC*/
	char *name = tItems[index].desp;
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	if (ipio_strcmp(name, "open test_c") == 0) {
		size = ARRAY_SIZE(open_c_cmd);

		for (i = 0; i < size; i++) {
			if (0 == i) {
				memcpy(open_c_cmd_data,  core_mp->open_para.cap1_dac_cmd,
				       sizeof(open_c_cmd_data));

			} else {
				memcpy(open_c_cmd_data,  core_mp->open_para.cap1_raw_cmd,
				       sizeof(open_c_cmd_data));
			}

			j_len = 0;
			memset(str, 0, sizeof(str));

			for (j = 0; j < ARRAY_SIZE(open_c_cmd_data);) {
				j_len += snprintf(str + j_len, sizeof(str) - j_len, "0x%x,",
						  open_c_cmd_data[j++]);
			}

			tmp_len += snprintf(csv + tmp_len, file_size - tmp_len,
					    "%s = ,%s\n", open_c_cmd[i], str);
		}

	} else {
		for (j = 0; j < ARRAY_SIZE(tItems[index].p5v_cmd);) {
			j_len += snprintf(str + j_len, sizeof(str) - j_len, "0x%x,",
					  tItems[index].p5v_cmd[j++]);
		}

		tmp_len += snprintf(csv + tmp_len, file_size - tmp_len, "CDC command = ,%s\n",
				    str);

		/* Print short parameters */
		if (ipio_strcmp(name, "short test") == 0) {
			tmp_len += snprintf(csv + tmp_len, (file_size - tmp_len), "Variation = ,%d\n",
					    core_mp->short_para.variation);
		}
	}

	*csv_len = tmp_len;
}

static void mp_compare_cdc_show_result(int index, s32 *tmp, char *csv,
				       int *csv_len, int type, s32 *max_ts,
				       s32 *min_ts, const char *desp, int file_zise)
{
	int x, y, tmp_len = *csv_len;
	int mp_result = MP_DATA_PASS;
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	if (ERR_ALLOC_MEM(tmp)) {
		ILI_ERR("The data of test item is null (%p)\n", tmp);
		mp_result = -EMP_INVAL;
		goto out;
	}

	/* print X raw only */
	for (x = 0; x < core_mp->xch_len; x++) {
		if (x == 0) {
			DUMP("\n %s ", desp);
			tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "\n%s ,", desp);
		}

		DUMP("  X_%d	,", (x + 1));
		tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "X_%d,", (x + 1));
	}

	DUMP("\n");
	tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "\n");

	for (y = 0; y < core_mp->ych_len; y++) {
		DUMP("  Y_%d	,", (y + 1));
		tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "Y_%d,", (y + 1));

		for (x = 0; x < core_mp->xch_len; x++) {
			int shift = y * core_mp->xch_len + x;

			/* In Short teset, we only identify if its value is low than min threshold. */
			if (tItems[index].catalog == SHORT_TEST) {
				if (tmp[shift] < min_ts[shift]) {
					DUMP(" #%7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "#%d,", tmp[shift]);
					mp_result = MP_DATA_FAIL;

				} else {
					DUMP(" %7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "%d,", tmp[shift]);
				}

				continue;
			}

			if ((tmp[shift] <= max_ts[shift] && tmp[shift] >= min_ts[shift])
					|| (type != TYPE_JUGE)) {
				if ((tmp[shift] == COMPARE_MAX || tmp[shift] == COMPARE_MIN)
						&& (type == TYPE_BENCHMARK)) {
					DUMP("%s", "BYPASS,");
					tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "BYPASS,");

				} else {
					DUMP(" %7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "%d,", tmp[shift]);
				}

			} else {
				if (tmp[shift] > max_ts[shift]) {
					DUMP(" *%7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "*%d,", tmp[shift]);

				} else {
					DUMP(" #%7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "#%d,", tmp[shift]);
				}

				mp_result = MP_DATA_FAIL;
			}
		}

		DUMP("\n");
		tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "\n");
	}

out:

	if (type == TYPE_JUGE) {
		if (mp_result == MP_DATA_PASS) {
			pr_info("\n Result : PASS\n");
			tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "Result : PASS\n");

		} else {
			pr_info("\n Result : FAIL\n");
			tmp_len += snprintf(csv + tmp_len, (file_zise - tmp_len), "Result : FAIL\n");
		}
	}

	*csv_len = tmp_len;
}

#define ABS(a, b) ((a > b) ? (a - b) : (b - a))
#define ADDR(x, y) ((y * ilits->core_mp.xch_len) + (x))

static s32 open_c_formula(int inCap1DAC, int inCap1Raw, int accuracy)
{
	s32 inCap1Value = 0;
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	u16 id = core_mp->chip_id;
	u8 type = core_mp->chip_type;
	int in_fout_range = 16384;
	int in_fout_range_half = 8192;
	int in_vadc_range = 36;
	int in_vbk = 39;
	int in_cbk_step = core_mp->open_para.cbk_step;/* from mp.ini */
	int in_cint = core_mp->open_para.cint;/* from mp.ini */
	int in_vdrv = core_mp->open_para.tvch - core_mp->open_para.tvcl;/* from mp.ini */
	int in_gain = core_mp->open_para.gain;/*  from mp.ini */
	int in_magnification = 10;
	int in_part_dac = 0;
	int in_part_raw = 0;

	if ((in_cbk_step == 0) || (in_cint == 0)) {
		if (id == ILI9881_CHIP) {
			if ((type == ILI_N) || (type == ILI_O)) {
				in_cbk_step = 32;
				in_cint = 70;
			}

		} else if (id == ILI9882_CHIP) {
			if (type == ILI_N) {
				in_cbk_step = 42;
				in_cint = 70;

			} else if (type == ILI_H) {
				in_cbk_step = 42;
				in_cint = 69;
			}

		} else if (id == ILI7807_CHIP) {
			if (type == ILI_Q) {
				in_cbk_step = 28;
				in_cint = 70;

			} else if (type == ILI_S) {
				in_cbk_step = 38;
				in_cint = 66;
				in_vbk = 42;
				in_vdrv = in_vdrv / 10;
				in_gain = in_gain / 10;

			} else if (type == ILI_V) {
				in_cbk_step = 28;
				in_cint = 70;
			}
		}
	}

	in_part_dac = (inCap1DAC * in_cbk_step * in_vbk / 2);
	in_part_raw = ((inCap1Raw - in_fout_range_half) * in_vadc_range * in_cint * 10 /
		     in_fout_range);

	if (accuracy) {
		inCap1Value = ((in_part_dac + in_part_raw) * 10) / in_vdrv / in_magnification /
			      in_gain;

	} else {
		inCap1Value = (in_part_dac + in_part_raw) / in_vdrv / in_magnification / in_gain;
	}

	return inCap1Value;
}

static void allnode_open_cdc_result(int index, int *buf, int *dac, int *raw)
{
	int i;
	char *desp = tItems[index].desp;
	int accuracy = 0;
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	if (ipio_strcmp(desp, "open test_c") == 0) {
		accuracy = core_mp->open_para.accuracy;

		for (i = 0; i < core_mp->frame_len; i++) {
			buf[i] = open_c_formula(dac[i], raw[i], accuracy);
		}
	}
}

static int codeToOhm(s32 *ohm, s32 *Code, u16 *v_tdf, u16 *h_tdf)
{
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	u16 id = core_mp->chip_id;
	u8 type = core_mp->chip_type;
	int in_tvch = core_mp->short_para.tvch;
	int in_tvcl = core_mp->short_para.tvcl;
	int in_variation = core_mp->short_para.variation;
	int in_tdf1 = 0;
	int in_tdf2 = 0;
	int in_cint = core_mp->short_para.cint;
	int in_rinternal = core_mp->short_para.rinternal;
	s32 temp = 0;
	int j = 0;

	if (core_mp->is_longv) {
		in_tdf1 = *v_tdf;
		in_tdf2 = *(v_tdf + 1);

	} else {
		in_tdf1 = *h_tdf;
		in_tdf2 = *(h_tdf + 1);
	}

	if (in_variation == 0) {
		in_variation = 100;
	}

	if ((in_cint == 0) || (in_rinternal == 0)) {
		if (id == ILI9881_CHIP) {
			if ((type == ILI_N) || (type == ILI_O)) {
				in_rinternal = 1915;
				in_cint = 70;
			}

		} else if (id == ILI9882_CHIP) {
			if (type == ILI_N) {
				in_rinternal = 1354;
				in_cint = 70;

			} else if (type == ILI_H) {
				in_rinternal = 1354;
				in_cint = 69;
			}

		} else if (id == ILI7807_CHIP) {
			if (type == ILI_Q) {
				in_rinternal = 1500;
				in_cint = 70;

			} else if (type == ILI_S) {
				in_rinternal = 1500;
				in_cint = 66;
				in_tvch = in_tvch / 10;
				in_tvcl = in_tvcl / 10;

			} else if (type == ILI_V) {
				in_rinternal = 1500;
				in_cint = 70;
			}
		}
	}

	for (j = 0; j < core_mp->frame_len; j++) {
		if (Code[j] == 0) {
			ILI_ERR("code is invalid\n");

		} else {
			temp = ((in_tvch - in_tvcl) * in_variation * (in_tdf1 - in_tdf2) * (1 << 12) /
				(9 * Code[j] * in_cint)) * 1000;
			temp = (temp - in_rinternal) / 1000;
		}

		ohm[j] = temp;
	}

	/* Unit = M Ohm */
	return temp;
}

static int short_test(int index, int frame_index)
{
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	u32 pid = core_mp->chip_pid >> 8;
	int j = 0, ret = 0;
	u16 v_tdf[2] = {0};
	u16 h_tdf[2] = {0};
	v_tdf[0] = tItems[index].v_tdf_1;
	v_tdf[1] = tItems[index].v_tdf_2;
	h_tdf[0] = tItems[index].h_tdf_1;
	h_tdf[1] = tItems[index].h_tdf_2;
	core_mp->short_para.tvch = tItems[index].tvch;
	core_mp->short_para.tvcl = tItems[index].tvcl;
	core_mp->short_para.variation = tItems[index].variation;
	core_mp->short_para.cint = tItems[index].cint;
	core_mp->short_para.rinternal = tItems[index].rinternal;

	/* 9881N, 9881O, 7807Q, 7807S not support cbk_step and cint read from mp.ini */
	if ((pid != 0x988117) && (pid != 0x988118) && (pid != 0x78071A)
			&& (pid != 0x78071C)) {
		if ((core_mp->short_para.cint == 0) || (core_mp->short_para.rinternal == 0)) {
			ILI_ERR("Failed to get short parameter");
			core_mp->lost_parameter = true;
			return -1;
		}
	}

	ILI_INFO("TVCH = %d, TVCL = %d, Variation = %d, V_TDF1 = %d, V_TDF2 = %d, H_TDF1 = %d, H_TDF2 = %d, Cint = %d, Rinternal = %d\n"
		 , core_mp->short_para.tvch, core_mp->short_para.tvcl,
		 core_mp->short_para.variation,
		 tItems[index].v_tdf_1, tItems[index].v_tdf_2, tItems[index].h_tdf_1,
		 tItems[index].h_tdf_2,
		 core_mp->short_para.cint, core_mp->short_para.rinternal);

	if (core_mp->protocol_ver >= PROTOCOL_VER_540) {
		/* Calculate code to ohm and save to tItems[index].buf */
		ili_dump_data(frame_buf, 10, core_mp->frame_len, core_mp->xch_len,
			      "Short Raw 1");
		codeToOhm(&tItems[index].buf[frame_index * core_mp->frame_len], frame_buf,
			  v_tdf, h_tdf);
		ili_dump_data(&tItems[index].buf[frame_index * core_mp->frame_len], 10,
			      core_mp->frame_len,
			      core_mp->xch_len, "Short Ohm 1");

	} else {
		for (j = 0; j < core_mp->frame_len; j++) {
			tItems[index].buf[frame_index * core_mp->frame_len + j] = frame_buf[j];
		}
	}

	return ret;
}

static int allnode_key_cdc_data(int index)
{
	int i, ret = 0, len = 0;
	int in_dacp = 0, in_dacn = 0;
	u8 cmd[3] = {0};
	u8 *ori = NULL;
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	len = core_mp->key_len * 2;
	ILI_DBG("Read key's length = %d\n", len);
	ILI_DBG("core_mp.key_len = %d\n", core_mp->key_len);

	if (len <= 0) {
		ILI_ERR("Length is invalid\n");
		ret = -1;
		goto out;
	}

	/* CDC init */
	cmd[0] = P5_X_SET_CDC_INIT;
	cmd[1] = tItems[index].cmd;
	cmd[2] = 0;
	/* Allocate a buffer for the original */
	ori = kcalloc(len, sizeof(u8), GFP_KERNEL);

	if (ERR_ALLOC_MEM(ori)) {
		ILI_ERR("Failed to allocate ori mem (%ld)\n", PTR_ERR(ori));
		ret = -1;
		goto out;
	}

	ret = ilits->wrapper(cmd, sizeof(cmd), ori, len, ON, ON);

	if (ret < 0) {
		ILI_ERR(" fail to get cdc data\n");
		goto out;
	}

	ili_dump_data(ori, 8, len, 0, "Key CDC original");

	if (key_buf == NULL) {
		key_buf = kcalloc(core_mp->key_len, sizeof(s32), GFP_KERNEL);

		if (ERR_ALLOC_MEM(key_buf)) {
			ILI_ERR("Failed to allocate FrameBuffer mem (%ld)\n", PTR_ERR(key_buf));
			goto out;
		}

	} else {
		memset(key_buf, 0x0, core_mp->key_len);
	}

	/* Convert original data to the physical one in each node */
	for (i = 0; i < core_mp->frame_len; i++) {
		if (tItems[index].cmd == CMD_KEY_DAC) {
			/* DAC - P */
			if (((ori[(2 * i) + 1] & 0x80) >> 7) == 1) {
				/* Negative */
				in_dacp = 0 - (int)(ori[(2 * i) + 1] & 0x7F);

			} else {
				in_dacp = ori[(2 * i) + 1] & 0x7F;
			}

			/* DAC - N */
			if (((ori[(1 + (2 * i)) + 1] & 0x80) >> 7) == 1) {
				/* Negative */
				in_dacn = 0 - (int)(ori[(1 + (2 * i)) + 1] & 0x7F);

			} else {
				in_dacn = ori[(1 + (2 * i)) + 1] & 0x7F;
			}

			key_buf[i] = (in_dacp + in_dacn) / 2;
		}
	}

	ili_dump_data(key_buf, 32, core_mp->frame_len, core_mp->xch_len,
		      "Key CDC combined data");
out:
	ili_kfree((void **)&ori);
	return ret;
}

static int mp_cdc_init_cmd_common(u8 *cmd, int len, int index)
{
	int ret = 0;
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	if (core_mp->protocol_ver >= PROTOCOL_VER_540) {
		core_mp->cdc_len = 15;

		if (ARRAY_SIZE(tItems[index].p5v_cmd) >= len) {
			memcpy(cmd, tItems[index].p5v_cmd, len);
		}

		return ret;
	}

	return -1;
}

static int allnode_open_cdc_data(int mode, int *buf)
{
	int i = 0, ret = 0, len = 0;
	int in_dacp = 0, in_dacn = 0;
	u8 cmd[15] = {0};
	u8 *ori = NULL;
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	/* Multipling by 2 is due to the 16 bit in each node */
	len = (core_mp->xch_len * core_mp->ych_len * 2) + 2;
	ILI_DBG("Read X/Y Channel length = %d, mode = %d\n", len, mode);

	if (ERR_ALLOC_MEM(buf)) {
		ILI_ERR("buf is null\n");
		ret = -EMP_NOMEM;
		goto out;
	}

	if (len <= 2) {
		ILI_ERR("Length is invalid\n");
		ret = -EMP_INVAL;
		goto out;
	}

	if (4 == mode) {
		memcpy(cmd, core_mp->open_para.cap1_dac_cmd, sizeof(cmd));

	} else if (5 == mode) {
		memcpy(cmd, core_mp->open_para.cap1_raw_cmd, sizeof(cmd));
	}

	ili_dump_data(cmd, 8, sizeof(cmd), 0, "Open command");
	/* Allocate a buffer for the original */
	ori = kcalloc(len, sizeof(u8), GFP_KERNEL);

	if (ERR_ALLOC_MEM(ori)) {
		ILI_ERR("Failed to allocate ori, (%ld)\n", PTR_ERR(ori));
		ret = -EMP_NOMEM;
		goto out;
	}

	/* Get original frame(cdc) data */
	if (!ilits->eng_flow) {
		if (ilits->wrapper(cmd, core_mp->cdc_len, ori, len, ON, ON) < 0) {
			ILI_ERR("Failed to get cdc data\n");
			ret = -EMP_GET_CDC;
			goto out;
		}
	} else {
		msleep(200);
		if (ilits->wrapper(cmd, core_mp->cdc_len, ori, len, OFF, OFF) < 0) {
			ILI_ERR("Failed to get cdc data\n");
		}
	}

	ili_dump_data(ori, 8, len, 0, "Open CDC original");

	/* Convert original data to the physical one in each node */
	for (i = 0; i < core_mp->frame_len; i++) {
		if ((mode == 0) || (mode == 4)) {
			/* DAC - P */
			if (((ori[(2 * i) + 1] & 0x80) >> 7) == 1) {
				/* Negative */
				in_dacp = 0 - (int)(ori[(2 * i) + 1] & 0x7F);

			} else {
				in_dacp = ori[(2 * i) + 1] & 0x7F;
			}

			/* DAC - N */
			if (((ori[(1 + (2 * i)) + 1] & 0x80) >> 7) == 1) {
				/* Negative */
				in_dacn = 0 - (int)(ori[(1 + (2 * i)) + 1] & 0x7F);

			} else {
				in_dacn = ori[(1 + (2 * i)) + 1] & 0x7F;
			}

			buf[i] = in_dacp + in_dacn;

		} else {
			/* H byte + L byte */
			s32 tmp = (ori[(2 * i) + 1] << 8) + ori[(1 + (2 * i)) + 1];

			if ((tmp & 0x8000) == 0x8000) {
				buf[i] = tmp - 65536;

			} else {
				buf[i] = tmp;
			}
		}
	}

	ili_dump_data(buf, 10, core_mp->frame_len,  core_mp->xch_len,
		      "Open CDC combined");
out:
	ili_kfree((void **)&ori);
	return ret;
}

static int allnode_peak_to_peak_cdc_data(int index)
{
	int i, k, ret = 0, len = 0, rd_frame_num = 1, cmd_len = 0;
	u8 cmd[15] = {0};
	u8 *ori = NULL;
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	/* Multipling by 2 is due to the 16 bit in each node */
	len = (core_mp->frame_len * 2) + 2;
	ILI_DBG("Read X/Y Channel length = %d\n", len);

	if (len <= 2) {
		ILI_ERR("Length is invalid\n");
		ret = -EMP_INVAL;
		goto out;
	}

	memset(cmd, 0xFF, sizeof(cmd));
	/* Allocate a buffer for the original */
	ori = kcalloc(len, sizeof(u8), GFP_KERNEL);

	if (ERR_ALLOC_MEM(ori)) {
		ILI_ERR("Failed to allocate ori, (%ld)\n", PTR_ERR(ori));
		ret = -EMP_NOMEM;
		goto out;
	}

	if (tItems[index].bch_mrk_multi) {
		rd_frame_num = tItems[index].bch_mrk_frm_num - 1;
	}

	if (frm_buf == NULL) {
		frm_buf = (s32 **)kzalloc(tItems[index].bch_mrk_frm_num * sizeof(s32 *),
					  GFP_KERNEL);

		if (ERR_ALLOC_MEM(frm_buf)) {
			ILI_ERR("Failed to allocate frm_buf mem (%ld)\n", PTR_ERR(frm_buf));
			ret = -EMP_NOMEM;
			goto out;
		}

		for (i = 0; i < tItems[index].bch_mrk_frm_num; i++) {
			frm_buf[i] = (s32 *)kzalloc(core_mp->frame_len * sizeof(s32), GFP_KERNEL);

			if (ERR_ALLOC_MEM(frm_buf)) {
				ILI_ERR("Failed to allocate frm_buf[%d] mem (%ld)\n", i, PTR_ERR(frm_buf));
				ret = -EMP_NOMEM;
				goto out;
			}
		}
	}

	/* CDC init */
	if (mp_cdc_init_cmd_common(cmd, sizeof(cmd), index) < 0) {
		ILI_ERR("Failed to get cdc command\n");
		ret = -EMP_CMD;
		goto out;
	}

	ili_dump_data(cmd, 8, core_mp->cdc_len, 0, "Mutual CDC command");

	for (k = 0; k < rd_frame_num; k++) {
		if (k == 0) {
			cmd_len = core_mp->cdc_len;

		} else {
			cmd_len = 0;
		}

		memset(ori, 0, len);

		/* Get original frame(cdc) data */
		if (!ilits->eng_flow) {
			if (ilits->wrapper(cmd, cmd_len, ori, len, ON, ON) < 0) {
				ILI_ERR("Failed to get cdc data\n");
				ret = -EMP_GET_CDC;
				goto out;
			}
		} else {
			msleep(200);
			if (ilits->wrapper(cmd, cmd_len, ori, len, OFF, OFF) < 0) {
				ILI_ERR("Failed to get cdc data\n");
			}
		}

		ili_dump_data(ori, 8, len, 0, "Mutual CDC original");

		/* Convert original data to the physical one in each node */
		for (i = 0; i < core_mp->frame_len; i++) {
			/* H byte + L byte */
			s32 tmp = (ori[(2 * i) + 1] << 8) + ori[(1 + (2 * i)) + 1];

			if ((tmp & 0x8000) == 0x8000) {
				frm_buf[k][i] = tmp - 65536;

			} else {
				frm_buf[k][i] = tmp;
			}

			/* multiple case frame3 = frame1 - frame2*/
			if (tItems[index].bch_mrk_multi && rd_frame_num == k + 1) {
				frm_buf[k + 1][i] = frm_buf[k - 1][i] - frm_buf[k][i];
			}
		}
	}

	if (tItems[index].bch_mrk_multi) {
		ili_dump_data(frm_buf[0], 32, core_mp->frame_len, core_mp->xch_len,
			      "Mutual CDC combined[0]/frame1");
		ili_dump_data(frm_buf[1], 32, core_mp->frame_len, core_mp->xch_len,
			      "Mutual CDC combined[1]/frame2");
		ili_dump_data(frm_buf[2], 32, core_mp->frame_len, core_mp->xch_len,
			      "Mutual CDC combined[2]/frame3");

	} else {
		ili_dump_data(frm_buf[0], 32, core_mp->frame_len, core_mp->xch_len,
			      "Mutual CDC combined[0]/frame1");
	}

out:
	ili_kfree((void **)&ori);
	return ret;
}

static int allnode_mutual_cdc_data(int index)
{
	int i, ret = 0, len = 0;
	int in_dacp = 0, in_dacn = 0;
	u8 cmd[15] = {0};
	u8 *ori = NULL;
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	/* Multipling by 2 is due to the 16 bit in each node */
	len = (core_mp->frame_len * 2) + 2;
	ILI_DBG("Read X/Y Channel length = %d\n", len);

	if (len <= 2) {
		ILI_ERR("Length is invalid\n");
		ret = -EMP_INVAL;
		goto out;
	}

	memset(cmd, 0xFF, sizeof(cmd));

	/* CDC init */
	if (mp_cdc_init_cmd_common(cmd, sizeof(cmd), index) < 0) {
		ILI_ERR("Failed to get cdc command\n");
		ret = -EMP_CMD;
		goto out;
	}

	ili_dump_data(cmd, 8, core_mp->cdc_len, 0, "Mutual CDC command");
	/* Allocate a buffer for the original */
	ori = kcalloc(len, sizeof(u8), GFP_KERNEL);

	if (ERR_ALLOC_MEM(ori)) {
		ILI_ERR("Failed to allocate ori, (%ld)\n", PTR_ERR(ori));
		ret = -EMP_NOMEM;
		goto out;
	}

	/* Get original frame(cdc) data */
	if (!ilits->eng_flow) {
		if (ilits->wrapper(cmd, core_mp->cdc_len, ori, len, ON, ON) < 0) {
			ILI_ERR("Failed to get cdc data\n");
			ret = -EMP_GET_CDC;
			goto out;
		}
	} else {
		msleep(200);
		if (ilits->wrapper(cmd, core_mp->cdc_len, ori, len, OFF, OFF) < 0) {
			ILI_ERR("Failed to get cdc data\n");
		}
	}

	ili_dump_data(ori, 8, len, 0, "Mutual CDC original");

	if (frame_buf == NULL) {
		frame_buf = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

		if (ERR_ALLOC_MEM(frame_buf)) {
			ILI_ERR("Failed to allocate FrameBuffer mem (%ld)\n", PTR_ERR(frame_buf));
			ret = -EMP_NOMEM;
			goto out;
		}

	} else {
		memset(frame_buf, 0x0, core_mp->frame_len);
	}

	/* Convert original data to the physical one in each node */
	for (i = 0; i < core_mp->frame_len; i++) {
		if (ipio_strcmp(tItems[index].desp, "calibration data(dac)") == 0) {
			/* DAC - P */
			if (((ori[(2 * i) + 1] & 0x80) >> 7) == 1) {
				/* Negative */
				in_dacp = 0 - (int)(ori[(2 * i) + 1] & 0x7F);

			} else {
				in_dacp = ori[(2 * i) + 1] & 0x7F;
			}

			/* DAC - N */
			if (((ori[(1 + (2 * i)) + 1] & 0x80) >> 7) == 1) {
				/* Negative */
				in_dacn = 0 - (int)(ori[(1 + (2 * i)) + 1] & 0x7F);

			} else {
				in_dacn = ori[(1 + (2 * i)) + 1] & 0x7F;
			}

			frame_buf[i] = (in_dacp + in_dacn) / 2;

		} else {
			/* H byte + L byte */
			s32 tmp = (ori[(2 * i) + 1] << 8) + ori[(1 + (2 * i)) + 1];

			if ((tmp & 0x8000) == 0x8000) {
				frame_buf[i] = tmp - 65536;

			} else {
				frame_buf[i] = tmp;
			}

			if (ipio_strcmp(tItems[index].desp, "raw data(no bk)") == 0 ||
					ipio_strcmp(tItems[index].desp, "raw data(no bk) (lcm off)") == 0) {
				frame_buf[i] -= core_mp->no_bk_shift;
			}
		}
	}

	ili_dump_data(frame_buf, 32, core_mp->frame_len, core_mp->xch_len,
		      "Mutual CDC combined");
out:
	ili_kfree((void **)&ori);
	return ret;
}

static void compare_MaxMin_result(int index, s32 *data)
{
	int x, y;
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	for (y = 0; y < core_mp->ych_len; y++) {
		for (x = 0; x < core_mp->xch_len; x++) {
			int shift = y * core_mp->xch_len;

			if (tItems[index].catalog == UNTOUCH_P2P) {
				return;

			} else if (tItems[index].catalog == TX_RX_DELTA) {
				/* Tx max/min comparison */
				if (core_mp->tx_delta_buf[shift + x] < data[shift + x]) {
					core_mp->tx_max_buf[shift + x] = data[shift + x];
				}

				if (core_mp->tx_delta_buf[shift + x] > data[shift + x]) {
					core_mp->tx_min_buf[shift + x] = data[shift + x];
				}

				/* Rx max/min comparison */
				if (core_mp->rx_delta_buf[shift + x] < data[shift + x]) {
					core_mp->rx_max_buf[shift + x] = data[shift + x];
				}

				if (core_mp->rx_delta_buf[shift + x] > data[shift + x]) {
					core_mp->rx_min_buf[shift + x] = data[shift + x];
				}

			} else {
				if (tItems[index].max_buf[shift + x] < data[shift + x]) {
					tItems[index].max_buf[shift + x] = data[shift + x];
				}

				if (tItems[index].min_buf[shift + x] > data[shift + x]) {
					tItems[index].min_buf[shift + x] = data[shift + x];
				}
			}
		}
	}
}

static int create_mp_test_frame_buffer(int index, int frame_count)
{
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	ILI_DBG("Create MP frame buffers (index = %d), count = %d\n",
		index, frame_count);

	if (tItems[index].catalog == TX_RX_DELTA) {
		if (core_mp->tx_delta_buf == NULL) {
			core_mp->tx_delta_buf = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

			if (ERR_ALLOC_MEM(core_mp->tx_delta_buf)) {
				ILI_ERR("Failed to allocate tx_delta_buf mem\n");
				ili_kfree((void **)&core_mp->tx_delta_buf);
				return -ENOMEM;
			}
		}

		if (core_mp->rx_delta_buf == NULL) {
			core_mp->rx_delta_buf = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

			if (ERR_ALLOC_MEM(core_mp->rx_delta_buf)) {
				ILI_ERR("Failed to allocate rx_delta_buf mem\n");
				ili_kfree((void **)&core_mp->rx_delta_buf);
				return -ENOMEM;
			}
		}

		if (core_mp->tx_max_buf == NULL) {
			core_mp->tx_max_buf = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

			if (ERR_ALLOC_MEM(core_mp->tx_max_buf)) {
				ILI_ERR("Failed to allocate tx_max_buf mem\n");
				ili_kfree((void **)&core_mp->tx_max_buf);
				return -ENOMEM;
			}
		}

		if (core_mp->tx_min_buf == NULL) {
			core_mp->tx_min_buf = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

			if (ERR_ALLOC_MEM(core_mp->tx_min_buf)) {
				ILI_ERR("Failed to allocate tx_min_buf mem\n");
				ili_kfree((void **)&core_mp->tx_min_buf);
				return -ENOMEM;
			}
		}

		if (core_mp->rx_max_buf == NULL) {
			core_mp->rx_max_buf = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

			if (ERR_ALLOC_MEM(core_mp->rx_max_buf)) {
				ILI_ERR("Failed to allocate rx_max_buf mem\n");
				ili_kfree((void **)&core_mp->rx_max_buf);
				return -ENOMEM;
			}
		}

		if (core_mp->rx_min_buf == NULL) {
			core_mp->rx_min_buf = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

			if (ERR_ALLOC_MEM(core_mp->rx_min_buf)) {
				ILI_ERR("Failed to allocate rx_min_buf mem\n");
				ili_kfree((void **)&core_mp->rx_min_buf);
				return -ENOMEM;
			}
		}

	} else {
		if (tItems[index].buf == NULL) {
			tItems[index].buf = vmalloc(frame_count * core_mp->frame_len * sizeof(s32));

			if (ERR_ALLOC_MEM(tItems[index].buf)) {
				ILI_ERR("Failed to allocate buf mem\n");
				ili_kfree((void **)&tItems[index].buf);
				return -ENOMEM;
			}
		}

		if (tItems[index].result_buf == NULL) {
			tItems[index].result_buf = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

			if (ERR_ALLOC_MEM(tItems[index].result_buf)) {
				ILI_ERR("Failed to allocate result_buf mem\n");
				ili_kfree((void **)&tItems[index].result_buf);
				return -ENOMEM;
			}
		}

		if (tItems[index].max_buf == NULL) {
			tItems[index].max_buf = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

			if (ERR_ALLOC_MEM(tItems[index].max_buf)) {
				ILI_ERR("Failed to allocate max_buf mem\n");
				ili_kfree((void **)&tItems[index].max_buf);
				return -ENOMEM;
			}
		}

		if (tItems[index].min_buf == NULL) {
			tItems[index].min_buf = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

			if (ERR_ALLOC_MEM(tItems[index].min_buf)) {
				ILI_ERR("Failed to allocate min_buf mem\n");
				ili_kfree((void **)&tItems[index].min_buf);
				return -ENOMEM;
			}
		}
	}

	return 0;
}

static int mutual_test(int index)
{
	int i = 0, j = 0, x = 0, y = 0, ret = 0, get_frame_cont = 1;
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	ILI_DBG("index = %d, desp = %s, Frame Count = %d\n",
		index, tItems[index].desp, tItems[index].frame_count);

	/*
	 * We assume that users who are calling the test forget to config frame count
	 * as 1, so we just help them to set it up.
	 */
	if (tItems[index].frame_count <= 0) {
		ILI_ERR("Frame count is zero, which is at least set as 1\n");
		tItems[index].frame_count = 1;
	}

	ret = create_mp_test_frame_buffer(index, tItems[index].frame_count);

	if (ret < 0) {
		ret = -EMP_NOMEM;
		goto out;
	}

	/* Init Max/Min buffer */
	for (y = 0; y < core_mp->ych_len; y++) {
		for (x = 0; x < core_mp->xch_len; x++) {
			if (tItems[i].catalog == TX_RX_DELTA) {
				core_mp->tx_max_buf[y * core_mp->xch_len + x] = COMPARE_MIN;
				core_mp->rx_max_buf[y * core_mp->xch_len + x] = COMPARE_MIN;
				core_mp->tx_min_buf[y * core_mp->xch_len + x] = COMPARE_MAX;
				core_mp->rx_min_buf[y * core_mp->xch_len + x] = COMPARE_MAX;

			} else {
				tItems[index].max_buf[y * core_mp->xch_len + x] = COMPARE_MIN;
				tItems[index].min_buf[y * core_mp->xch_len + x] = COMPARE_MAX;
			}
		}
	}

	if (tItems[index].catalog != PEAK_TO_PEAK_TEST) {
		get_frame_cont = tItems[index].frame_count;
	}

	if (tItems[index].spec_option == BENCHMARK) {
		dump_benchmark_data(tItems[index].bench_mark_max, tItems[index].bench_mark_min);
	}

	for (i = 0; i < get_frame_cont; i++) {
		ret = allnode_mutual_cdc_data(index);

		if (ret < 0) {
			ILI_ERR("Failed to initialise CDC data, %d\n", ret);
			goto out;
		}

		switch (tItems[index].catalog) {
		case PIXEL:
			run_pixel_test(index);
			break;

		case UNTOUCH_P2P:
			run_untouch_p2p_test(index);
			break;

		case OPEN_TEST:
			run_open_test(index);
			break;

		case TX_RX_DELTA:
			run_tx_rx_delta_test(index);
			break;

		case SHORT_TEST:
			ret = short_test(index, i);

			if (ret < 0) {
				ret = -EMP_PARA_NULL;
				goto out;
			}

			break;

		default:
			for (j = 0; j < core_mp->frame_len; j++) {
				tItems[index].buf[i * core_mp->frame_len + j] = frame_buf[j];
			}

			break;
		}

		compare_MaxMin_result(index, &tItems[index].buf[i * core_mp->frame_len]);
	}

out:
	return ret;
}

static int  peak_to_peak_test(int index)
{
	int i = 0, j = 0, x = 0, y = 0, ret = 0;
	/*char benchmark_str[128] = {0};*/
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	ILI_DBG("index = %d, desp = %s bch_mrk_frm_num = %d\n"
		, index, tItems[index].desp, tItems[index].bch_mrk_frm_num);
	ret = create_mp_test_frame_buffer(index, tItems[index].bch_mrk_frm_num);

	if (ret < 0) {
		ret = -EMP_NOMEM;
		goto out;
	}

	if (tItems[index].spec_option == BENCHMARK) {
		for (i = 0; i < tItems[index].bch_mrk_frm_num; i++) {
			dump_benchmark_data(tItems[index].bch_mrk_max[i], tItems[index].bch_mrk_min[i]);
		}
	}

	/* Init Max/Min buffer */
	for (y = 0; y < core_mp->ych_len; y++) {
		for (x = 0; x < core_mp->xch_len; x++) {
			tItems[index].max_buf[y * core_mp->xch_len + x] = COMPARE_MIN;
			tItems[index].min_buf[y * core_mp->xch_len + x] = COMPARE_MAX;
		}
	}

	ret = allnode_peak_to_peak_cdc_data(index);

	if (ret < 0) {
		ILI_ERR("Failed to initialise CDC data, %d\n", ret);
		goto out;
	}

	for (i = 0; i < tItems[index].bch_mrk_frm_num; i++) {
		for (j = 0; j < core_mp->frame_len; j++) {
			tItems[index].buf[i * core_mp->frame_len + j] = frm_buf[i][j];
		}

		compare_MaxMin_result(index, &tItems[index].buf[i * core_mp->frame_len]);
	}

out:
	return ret;
}

static int open_test_cap(int index)
{
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	u32 pid = core_mp->chip_pid >> 8;
	struct mp_test_open_c *open;
	int i = 0, x = 0, y = 0, ret = 0, addr = 0;
	ILI_DBG("index = %d, desp = %s, Frame Count = %d\n",
		index, tItems[index].desp, tItems[index].frame_count);

	if (tItems[index].frame_count <= 0) {
		ILI_ERR("Frame count is zero, which is at least set as 1\n");
		tItems[index].frame_count = 1;
	}

	open = kzalloc(tItems[index].frame_count * sizeof(struct mp_test_open_c),
		       GFP_KERNEL);

	if (ERR_ALLOC_MEM(open)) {
		ILI_ERR("Failed to allocate open mem (%ld)\n", PTR_ERR(open));
		ret = -EMP_NOMEM;
		goto out;
	}

	if (create_mp_test_frame_buffer(index, tItems[index].frame_count) < 0) {
		ret = -EMP_NOMEM;
		goto out;
	}

	if (cap_dac == NULL) {
		cap_dac = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

		if (ERR_ALLOC_MEM(cap_dac)) {
			ILI_ERR("Failed to allocate cap_dac buffer\n");
			return -EMP_NOMEM;
		}

	} else {
		memset(cap_dac, 0x0, core_mp->frame_len);
	}

	if (cap_raw == NULL) {
		cap_raw = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

		if (ERR_ALLOC_MEM(cap_raw)) {
			ILI_ERR("Failed to allocate cap_raw buffer\n");
			ili_kfree((void **)&cap_dac);
			return -EMP_NOMEM;
		}

	} else {
		memset(cap_raw, 0x0, core_mp->frame_len);
	}

	/* Init Max/Min buffer */
	for (y = 0; y < core_mp->ych_len; y++) {
		for (x = 0; x < core_mp->xch_len; x++) {
			tItems[index].max_buf[y * core_mp->xch_len + x] = COMPARE_MIN;
			tItems[index].min_buf[y * core_mp->xch_len + x] = COMPARE_MAX;
		}
	}

	if (tItems[index].spec_option == BENCHMARK) {
		dump_benchmark_data(tItems[index].bench_mark_max, tItems[index].bench_mark_min);
	}

	core_mp->open_para.gain = tItems[index].gain;
	core_mp->open_para.tvch = tItems[index].tvch;
	core_mp->open_para.tvcl = tItems[index].tvcl;
	core_mp->open_para.cbk_step = tItems[index].cbk_step;
	core_mp->open_para.cint = tItems[index].cint;
	core_mp->open_para.accuracy  = tItems[index].accuracy;

	/* 9881N, 9881O, 7807Q, 7807S not support cbk_step and cint read from mp.ini */
	if ((pid != 0x988117) && (pid != 0x988118) && (pid != 0x78071A)
			&& (pid != 0x78071C)) {
		if ((core_mp->open_para.cbk_step == 0) || (core_mp->open_para.cint == 0)) {
			ILI_ERR("Failed to get open parameter");
			ret = -EMP_PARA_NULL;
			core_mp->lost_parameter = true;
			goto out;
		}
	}

	ILI_INFO("gain = %d, tvch = %d, tvcl = %d, cbk_step = %d, cint = %d\n",
		 core_mp->open_para.gain,
		 core_mp->open_para.tvch, core_mp->open_para.tvcl, core_mp->open_para.cbk_step,
		 core_mp->open_para.cint);

	for (i = 0; i < tItems[index].frame_count; i++) {
		open[i].cap_dac = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);
		open[i].cap_raw = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);
		open[i].dcl_cap = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);
	}

	for (i = 0; i < tItems[index].frame_count; i++) {
		ret = allnode_open_cdc_data(4, open[i].cap_dac);

		if (ret < 0) {
			ILI_ERR("Failed to get Open CAP DAC data, %d\n", ret);
			goto out;
		}
		msleep(5);

		ret = allnode_open_cdc_data(5, open[i].cap_raw);

		if (ret < 0) {
			ILI_ERR("Failed to get Open CAP RAW data, %d\n", ret);
			goto out;
		}

		allnode_open_cdc_result(index, open[i].dcl_cap, open[i].cap_dac,
					open[i].cap_raw);

		/* record fist frame for debug */
		if (i == 0) {
			ipio_memcpy(cap_dac, open[i].cap_dac, core_mp->frame_len * sizeof(s32),
				    core_mp->frame_len * sizeof(s32));
			ipio_memcpy(cap_raw, open[i].cap_raw, core_mp->frame_len * sizeof(s32),
				    core_mp->frame_len * sizeof(s32));
		}

		ili_dump_data(open[i].dcl_cap, 10, core_mp->frame_len, core_mp->xch_len,
			      "DCL_Cap");
		addr = 0;

		for (y = 0; y < core_mp->ych_len; y++) {
			for (x = 0; x < core_mp->xch_len; x++) {
				tItems[index].buf[(i * core_mp->frame_len) + addr] = open[i].dcl_cap[addr];
				addr++;
			}
		}

		compare_MaxMin_result(index, &tItems[index].buf[i * core_mp->frame_len]);
	}

out:

	if (open != NULL) {
		for (i = 0; i < tItems[index].frame_count; i++) {
			ili_kfree((void **)&open[i].cap_dac);
			ili_kfree((void **)&open[i].cap_raw);
			ili_kfree((void **)&open[i].dcl_cap);
		}

		ili_kfree((void **)open);
	}

	return ret;
}

static int key_test(int index)
{
	int i, j = 0, ret = 0;
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	ILI_DBG("Item = %s, Frame Count = %d\n",
		tItems[index].desp, tItems[index].frame_count);

	if (tItems[index].frame_count == 0) {
		ILI_ERR("Frame count is zero, which at least sets as 1\n");
		ret = -EINVAL;
		goto out;
	}

	ret = create_mp_test_frame_buffer(index, tItems[index].frame_count);

	if (ret < 0) {
		goto out;
	}

	for (i = 0; i < tItems[index].frame_count; i++) {
		ret = allnode_key_cdc_data(index);

		if (ret < 0) {
			ILI_ERR("Failed to initialise CDC data, %d\n", ret);
			goto out;
		}

		for (j = 0; j < core_mp->key_len; j++) {
			tItems[index].buf[j] = key_buf[j];
		}
	}

	compare_MaxMin_result(index, tItems[index].buf);
out:
	return ret;
}

static int self_test(int index)
{
	ILI_ERR("TDDI has no self to be tested currently\n");
	return -1;
}

static int st_test(int index)
{
	ILI_ERR("ST Test is not supported by the driver\n");
	return -1;
}

static int mp_get_timing_info(void *chip_data, struct auto_testdata *testdata)
{
	int ret = 0;
	int i = 0;
	struct test_item_info *p_test_item_info = NULL;
	struct ilitek_ts_data *chip_info = (struct ilitek_ts_data *)chip_data;
	struct core_mp_test_data *core_mp = &chip_info->core_mp;
	p_test_item_info = get_test_item_info(testdata->fw, TYPE_TIMEING_INFO);

	if (!p_test_item_info) {
		ILI_ERR("item: %d get_test_item_info fail\n", TYPE_TIMEING_INFO);
		return -1;

	} else {
		if (p_test_item_info->p_buffer[0] == 1) {
			core_mp->is_longv = p_test_item_info->p_buffer[7];

			for (i = 0; i < TYPE_MAX; i++) {
				if (!p_test_item_info->p_buffer[11 + i]) {
					break;
				}

				core_mp->test_index[i] = (u8)p_test_item_info->p_buffer[11 + i];
				ILI_INFO("test %d item\n", core_mp->test_index[i]);
			}
		}
	}

	ILI_INFO("p_test_item_info->p_buffer[7] = %d\n", p_test_item_info->p_buffer[7]);
	core_mp->ini_date = p_test_item_info->p_buffer[9];
	core_mp->ini_ver = p_test_item_info->p_buffer[10];
	ILI_INFO("all test %d item\n", i);
	ILI_INFO("DDI Mode = %s\n", (core_mp->is_longv ? "Long V" : "Long H"));
	ILI_INFO("ini_date = %d ini_ver = %d\n", core_mp->ini_date, core_mp->ini_ver);
	return ret;
}

static int mp_test_data_sort_average(s32 *oringin_data, int index,
				     s32 *avg_result)
{
	int i, j, k, x, y, len = 5, size;
	s32 u32temp;
	int u32up_frame, u32down_frame;
	s32 *u32sum_raw_data = NULL;
	s32 *u32data_buff = NULL;
	int ret = 0;
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	if (tItems[index].frame_count <= 1) {
		ret = 0;
		goto out;
	}

	if (ERR_ALLOC_MEM(oringin_data)) {
		ILI_ERR("Input wrong address\n");
		ret = -ENOMEM;
		goto out;
	}

	u32data_buff = kcalloc(core_mp->frame_len * tItems[index].frame_count,
			       sizeof(s32), GFP_KERNEL);

	if (ERR_ALLOC_MEM(u32data_buff)) {
		ILI_ERR("Failed to allocate u32data_buff FRAME buffer\n");
		ret = -ENOMEM;
		goto out;
	}

	u32sum_raw_data = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

	if (ERR_ALLOC_MEM(u32data_buff)) {
		ILI_ERR("Failed to allocate u32sum_raw_data FRAME buffer\n");
		ret = -ENOMEM;
		goto out;
	}

	size = core_mp->frame_len * tItems[index].frame_count;

	for (i = 0; i < size; i++) {
		u32data_buff[i] = oringin_data[i];
	}

	u32up_frame = tItems[index].frame_count * tItems[index].highest_percentage /
		      100;
	u32down_frame = tItems[index].frame_count * tItems[index].lowest_percentage /
			100;
	ILI_DBG("Up=%d, Down=%d -%s\n", u32up_frame, u32down_frame, tItems[index].desp);

	if (ili_debug_en) {
		pr_cont("\n[Show Original frist%d and last%d node data]\n", len, len);

		for (i = 0; i < core_mp->frame_len; i++) {
			for (j = 0; j < tItems[index].frame_count; j++) {
				if ((i < len) || (i >= (core_mp->frame_len - len))) {
					pr_cont("%d,", u32data_buff[j * core_mp->frame_len + i]);
				}
			}

			if ((i < len) || (i >= (core_mp->frame_len - len))) {
				pr_cont("\n");
			}
		}
	}

	for (i = 0; i < core_mp->frame_len; i++) {
		for (j = 0; j < tItems[index].frame_count - 1; j++) {
			for (k = 0; k < (tItems[index].frame_count - 1 - j); k++) {
				x = i + k * core_mp->frame_len;
				y = i + (k + 1) * core_mp->frame_len;

				if (*(u32data_buff + x) > *(u32data_buff + y)) {
					u32temp = *(u32data_buff + x);
					*(u32data_buff + x) = *(u32data_buff + y);
					*(u32data_buff + y) = u32temp;
				}
			}
		}
	}

	if (ili_debug_en) {
		pr_cont("\n[After sorting frist%d and last%d node data]\n", len, len);

		for (i = 0; i < core_mp->frame_len; i++) {
			for (j = u32down_frame; j < tItems[index].frame_count - u32up_frame; j++) {
				if ((i < len) || (i >= (core_mp->frame_len - len))) {
					pr_cont("%d,", u32data_buff[i + j * core_mp->frame_len]);
				}
			}

			if ((i < len) || (i >= (core_mp->frame_len - len))) {
				pr_cont("\n");
			}
		}
	}

	for (i = 0; i < core_mp->frame_len; i++) {
		u32sum_raw_data[i] = 0;

		for (j = u32down_frame; j < tItems[index].frame_count - u32up_frame; j++) {
			u32sum_raw_data[i] += u32data_buff[i + j * core_mp->frame_len];
		}

		avg_result[i] = u32sum_raw_data[i] / (tItems[index].frame_count - u32down_frame
						      - u32up_frame);
	}

	if (ili_debug_en) {
		pr_cont("\n[Average result frist%d and last%d node data]\n", len, len);

		for (i = 0; i < core_mp->frame_len; i++) {
			if ((i < len) || (i >= (core_mp->frame_len - len))) {
				pr_cont("%d,", avg_result[i]);
			}
		}

		if ((i < len) || (i >= (core_mp->frame_len - len))) {
			pr_cont("\n");
		}
	}

out:
	ili_kfree((void **)&u32data_buff);
	ili_kfree((void **)&u32sum_raw_data);
	return ret;
}

static void mp_compare_cdc_result(int index, s32 *tmp, s32 *max_ts, s32 *min_ts,
				  int *result)
{
	int i;
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	if (ERR_ALLOC_MEM(tmp)) {
		ILI_ERR("The data of test item is null (%p)\n", tmp);
		*result = MP_DATA_FAIL;
		return;
	}

	if (tItems[index].catalog == SHORT_TEST) {
		for (i = 0; i < core_mp->frame_len; i++) {
			if (tmp[i] < min_ts[i]) {
				*result = MP_DATA_FAIL;
				return;
			}
		}

	} else {
		for (i = 0; i < core_mp->frame_len; i++) {
			if (tmp[i] > max_ts[i] || tmp[i] < min_ts[i]) {
				ILI_DBG("Fail No.%d: max=%d, val=%d, min=%d\n", i, max_ts[i], tmp[i],
					min_ts[i]);
				*result = MP_DATA_FAIL;
				return;
			}
		}
	}
}

static int mp_compare_test_result(int index)
{
	int i, test_result = MP_DATA_PASS;
	s32 *max_threshold = NULL, *min_threshold = NULL;
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	max_threshold = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

	if (ERR_ALLOC_MEM(max_threshold)) {
		ILI_ERR("Failed to allocate threshold FRAME buffer\n");
		test_result = MP_DATA_FAIL;
		goto out;
	}

	min_threshold = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

	if (ERR_ALLOC_MEM(min_threshold)) {
		ILI_ERR("Failed to allocate threshold FRAME buffer\n");
		test_result = MP_DATA_FAIL;
		goto out;
	}

	/* Show test result as below */
	if (tItems[index].catalog == TX_RX_DELTA) {
		if (ERR_ALLOC_MEM(core_mp->rx_delta_buf)
				|| ERR_ALLOC_MEM(core_mp->tx_delta_buf)) {
			ILI_ERR("This test item (%s) has no data inside its buffer\n",
				tItems[index].desp);
			test_result = MP_DATA_FAIL;
			goto out;
		}

		for (i = 0; i < core_mp->frame_len; i++) {
			max_threshold[i] = core_mp->tx_delta_max;
			min_threshold[i] = core_mp->tx_delta_min;
		}

		mp_compare_cdc_result(index, core_mp->tx_max_buf, max_threshold, min_threshold,
				      &test_result);
		mp_compare_cdc_result(index, core_mp->tx_min_buf, max_threshold, min_threshold,
				      &test_result);

		for (i = 0; i < core_mp->frame_len; i++) {
			max_threshold[i] = core_mp->rx_delta_max;
			min_threshold[i] = core_mp->rx_delta_min;
		}

		mp_compare_cdc_result(index, core_mp->rx_max_buf, max_threshold, min_threshold,
				      &test_result);
		mp_compare_cdc_result(index, core_mp->rx_min_buf, max_threshold, min_threshold,
				      &test_result);

	} else {
		if (ERR_ALLOC_MEM(tItems[index].buf) || ERR_ALLOC_MEM(tItems[index].max_buf) ||
				ERR_ALLOC_MEM(tItems[index].min_buf)
				|| ERR_ALLOC_MEM(tItems[index].result_buf)) {
			ILI_ERR("This test item (%s) has no data inside its buffer\n",
				tItems[index].desp);
			test_result = MP_DATA_FAIL;
			goto out;
		}

		if (tItems[index].spec_option == BENCHMARK) {
			if (tItems[index].catalog == PEAK_TO_PEAK_TEST) {
				for (i = 0; i < core_mp->frame_len; i++) {
					max_threshold[i] = tItems[index].bch_mrk_max[0][i];
					min_threshold[i] = tItems[index].bch_mrk_min[0][i];
				}

			} else {
				for (i = 0; i < core_mp->frame_len; i++) {
					max_threshold[i] = tItems[index].bench_mark_max[i];
					min_threshold[i] = tItems[index].bench_mark_min[i];
				}
			}

		} else {
			for (i = 0; i < core_mp->frame_len; i++) {
				max_threshold[i] = tItems[index].max;
				min_threshold[i] = tItems[index].min;
			}
		}

		/* general result */
		if (tItems[index].trimmed_mean && tItems[index].catalog != PEAK_TO_PEAK_TEST) {
			mp_test_data_sort_average(tItems[index].buf, index, tItems[index].result_buf);
			mp_compare_cdc_result(index, tItems[index].result_buf, max_threshold,
					      min_threshold, &test_result);

		} else {
			if (tItems[index].bch_mrk_multi) {
				for (i = 0; i < tItems[index].bch_mrk_frm_num; i++) {
					mp_compare_cdc_result(index, frm_buf[i], tItems[index].bch_mrk_max[i],
							      tItems[index].bch_mrk_min[i],
							      &test_result);
				}

			} else {
				mp_compare_cdc_result(index, tItems[index].max_buf, max_threshold,
						      min_threshold, &test_result);
				mp_compare_cdc_result(index, tItems[index].min_buf, max_threshold,
						      min_threshold, &test_result);
			}
		}
	}

out:
	ili_kfree((void **)&max_threshold);
	ili_kfree((void **)&min_threshold);

	if (frm_buf != NULL) {
		for (i = 0; i < tItems[index].bch_mrk_frm_num; i++)
			if (frm_buf[i] != NULL) {
				ili_kfree((void **)&frm_buf[i]);
			}

		ili_kfree((void **)&frm_buf);
	}

	if (core_mp->all_pass == true) {
		test_result = MP_DATA_PASS;
	}

	tItems[index].item_result = test_result;
	return test_result;
}

static void mp_do_retry(int index, int count)
{
	if (count == 0) {
		ILI_INFO("Finish retry action\n");
		return;
	}

	ILI_INFO("retry = %d, item = %s\n", count, tItems[index].desp);
	tItems[index].do_test(index);

	if (mp_compare_test_result(index) < 0) {
		return mp_do_retry(index, count - 1);
	}
}

static int mp_show_result(void *chip_data, struct auto_testdata *testdata,
			  bool lcm_on)
{
	int ret = MP_DATA_PASS;
	int i, x, y, j, k, csv_len = 0, pass_item_count = 0, line_count = 0,
			   get_frame_cont = 1;
	s32 *max_threshold = NULL, *min_threshold = NULL;
	char *csv = NULL;
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	csv = vmalloc(CSV_FILE_SIZE);

	if (ERR_ALLOC_MEM(csv)) {
		ILI_ERR("Failed to allocate CSV mem\n");
		ret = -EMP_NOMEM;
		goto fail_open;
	}

	memset(csv, 0, CSV_FILE_SIZE);
	max_threshold = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);
	min_threshold = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

	if (ERR_ALLOC_MEM(max_threshold) || ERR_ALLOC_MEM(min_threshold)) {
		ILI_ERR("Failed to allocate threshold FRAME buffer\n");
		ret = -EMP_NOMEM;
		goto fail_open;
	}

	for (i = 0; i < MP_TEST_ITEM; i++) {
		if (tItems[i].run) {
			if (tItems[i].item_result < 0) {
				pass_item_count = 0;
				break;
			}

			pass_item_count++;
		}
	}

	if (pass_item_count == 0) {
		core_mp->final_result = MP_DATA_FAIL;

	} else {
		core_mp->final_result = MP_DATA_PASS;
	}

	mp_print_csv_header(csv, &csv_len, &line_count, CSV_FILE_SIZE);

	for (k = 0; k < TYPE_MAX; k++) {
		if (!core_mp->test_index[k]) {
			break;
		}

		for (i = 0; i < MP_TEST_ITEM; i++) {
			if (!tItems[i].run || (core_mp->test_index[k] != tItems[i].test_index)) {
				continue;
			}

			get_frame_cont = 1;

			if (tItems[i].item_result == MP_DATA_PASS) {
				pr_info("\n[%s],OK \n\n", tItems[i].desp);
				csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "\n[%s],OK\n\n",
						    tItems[i].desp);

			} else {
				pr_info("\n[%s],NG \n\n", tItems[i].desp);
				csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "\n[%s],NG\n\n",
						    tItems[i].desp);
			}

			mp_print_csv_cdc_cmd(csv, &csv_len, i, CSV_FILE_SIZE);
			pr_info("Frame count = %d\n", tItems[i].frame_count);
			csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len),
					    "Frame count = %d\n",
					    tItems[i].frame_count);

			if (tItems[i].trimmed_mean && tItems[i].catalog != PEAK_TO_PEAK_TEST) {
				pr_info("lowest percentage = %d\n", tItems[i].lowest_percentage);
				csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len),
						    "lowest percentage = %d\n",
						    tItems[i].lowest_percentage);
				pr_info("highest percentage = %d\n", tItems[i].highest_percentage);
				csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len),
						    "highest percentage = %d\n",
						    tItems[i].highest_percentage);
			}

			/* Show result of benchmark max and min */
			if (tItems[i].spec_option == BENCHMARK) {
				if (tItems[i].catalog == PEAK_TO_PEAK_TEST) {
					for (j = 0; j < core_mp->frame_len; j++) {
						max_threshold[j] = tItems[i].bch_mrk_max[0][j];
						min_threshold[j] = tItems[i].bch_mrk_min[0][j];
					}

				} else {
					for (j = 0; j < core_mp->frame_len; j++) {
						max_threshold[j] = tItems[i].bench_mark_max[j];
						min_threshold[j] = tItems[i].bench_mark_min[j];
					}
				}

			} else {
				for (j = 0; j < core_mp->frame_len; j++) {
					max_threshold[j] = tItems[i].max;
					min_threshold[j] = tItems[i].min;
				}

				pr_info("Max = %d\n", tItems[i].max);
				csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "Max = %d\n",
						    tItems[i].max);
				pr_info("Min = %d\n", tItems[i].min);
				csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "Min = %d\n",
						    tItems[i].min);
			}

			if (ipio_strcmp(tItems[i].desp, "open test_c") == 0) {
				mp_compare_cdc_show_result(i, cap_dac, csv, &csv_len, TYPE_NO_JUGE,
							   max_threshold, min_threshold,
							   "CAP_DAC", CSV_FILE_SIZE);
				mp_compare_cdc_show_result(i, cap_raw, csv, &csv_len, TYPE_NO_JUGE,
							   max_threshold, min_threshold,
							   "CAP_RAW", CSV_FILE_SIZE);
			}

			if (tItems[i].catalog == TX_RX_DELTA) {
				if (ERR_ALLOC_MEM(core_mp->rx_delta_buf)
						|| ERR_ALLOC_MEM(core_mp->tx_delta_buf)) {
					ILI_ERR("This test item (%s) has no data inside its buffer\n", tItems[i].desp);
					continue;
				}

			} else {
				if (ERR_ALLOC_MEM(tItems[i].buf) || ERR_ALLOC_MEM(tItems[i].max_buf) ||
						ERR_ALLOC_MEM(tItems[i].min_buf)) {
					ILI_ERR("This test item (%s) has no data inside its buffer\n", tItems[i].desp);
					continue;
				}
			}

			/* Show test result as below */
			if (tItems[i].catalog == KEY_TEST) {
				for (x = 0; x < core_mp->key_len; x++) {
					DUMP("KEY_%02d ", x);
					csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "KEY_%02d,", x);
				}

				DUMP("\n");
				csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "\n");

				for (y = 0; y < core_mp->key_len; y++) {
					DUMP(" %3d   ", tItems[i].buf[y]);
					csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), " %3d, ",
							    tItems[i].buf[y]);
				}

				DUMP("\n");
				csv_len += snprintf(csv + csv_len, (CSV_FILE_SIZE - csv_len), "\n");

			} else if (tItems[i].catalog == TX_RX_DELTA) {
				for (j = 0; j < core_mp->frame_len; j++) {
					max_threshold[j] = core_mp->tx_delta_max;
					min_threshold[j] = core_mp->tx_delta_min;
				}

				mp_compare_cdc_show_result(i, core_mp->tx_max_buf, csv, &csv_len, TYPE_JUGE,
							   max_threshold,
							   min_threshold, "TX Max Hold", CSV_FILE_SIZE);
				mp_compare_cdc_show_result(i, core_mp->tx_min_buf, csv, &csv_len, TYPE_JUGE,
							   max_threshold,
							   min_threshold, "TX Min Hold", CSV_FILE_SIZE);

				for (j = 0; j < core_mp->frame_len; j++) {
					max_threshold[j] = core_mp->rx_delta_max;
					min_threshold[j] = core_mp->rx_delta_min;
				}

				mp_compare_cdc_show_result(i, core_mp->rx_max_buf, csv, &csv_len, TYPE_JUGE,
							   max_threshold,
							   min_threshold, "RX Max Hold", CSV_FILE_SIZE);
				mp_compare_cdc_show_result(i, core_mp->rx_min_buf, csv, &csv_len, TYPE_JUGE,
							   max_threshold,
							   min_threshold, "RX Min Hold", CSV_FILE_SIZE);

			} else {
				/* general result */
				if (tItems[i].trimmed_mean && tItems[i].catalog != PEAK_TO_PEAK_TEST) {
					mp_compare_cdc_show_result(i, tItems[i].result_buf, csv, &csv_len, TYPE_JUGE, max_threshold, min_threshold, "Mean result", CSV_FILE_SIZE);
				}

				if (tItems[i].catalog != PEAK_TO_PEAK_TEST) {
					get_frame_cont = tItems[i].frame_count;
				}

				if (tItems[i].bch_mrk_multi) {
					get_frame_cont = tItems[i].bch_mrk_frm_num;
				}

				/* result of each frame */
				for (j = 0; j < get_frame_cont; j++) {
					char frame_name[128] = {0};
					snprintf(frame_name, 128, "Frame %d", (j + 1));
					mp_compare_cdc_show_result(i, &tItems[i].buf[(j * core_mp->frame_len)], csv,
								   &csv_len, TYPE_NO_JUGE,
								   max_threshold, min_threshold, frame_name, CSV_FILE_SIZE);
				}
			}
		}
	}

	mp_print_csv_tail(csv, &csv_len, CSV_FILE_SIZE);

	if (csv_len >= CSV_FILE_SIZE) {
		ILI_ERR("The length saved to CSV is too long !\n");
		ret = -EMP_INVAL;
		goto fail_open;
	}

	ILI_INFO("csv_len = 0x%x\n", csv_len);

	if (lcm_on && testdata->fp) {
		ILI_INFO("testdata->length = 0x%x\n", (u32)testdata->length);
		tp_test_write(testdata->fp, testdata->length, csv, csv_len, testdata->pos);

	} else if ((!lcm_on) && testdata->bs_fp) {
		ILI_INFO("testdata->bs_length = 0x%x\n", (u32)testdata->bs_length);
		tp_test_write(testdata->bs_fp, testdata->bs_length, csv, csv_len,
			      testdata->bs_pos);
	}

	/*for(i = 0;i < csv_len;i++) {*/
	/*  pr_cont("%c", csv[i]);*/
	/*}*/
	ILI_INFO("Writing Data into CSV succeed\n");
fail_open:
	ili_vfree((void **)&csv);
	ili_kfree((void **)&max_threshold);
	ili_kfree((void **)&min_threshold);
	return ret;
}

static void ilitek_tddi_mp_init_item(void)
{
	int i = 0;
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	memset(core_mp, 0, sizeof(struct core_mp_test_data));
	memset(&core_mp->open_para, 0, sizeof(core_mp->open_para));
	memset(&core_mp->short_para, 0, sizeof(core_mp->short_para));
	core_mp->chip_pid = ilits->chip->pid;
	core_mp->chip_id = ilits->chip->id;
	core_mp->chip_type = ilits->chip->type;
	core_mp->chip_ver = ilits->chip->ver;
	core_mp->fw_ver = ilits->chip->fw_ver;
	core_mp->protocol_ver = ilits->protocol->ver;
	core_mp->core_ver = ilits->chip->core_ver;
	core_mp->cdc_len = ilits->protocol->cdc_len;
	core_mp->no_bk_shift = ilits->chip->no_bk_shift;
	core_mp->xch_len = ilits->xch_num;
	core_mp->ych_len = ilits->ych_num;
	core_mp->frame_len = core_mp->xch_len * core_mp->ych_len;
	core_mp->stx_len = 0;
	core_mp->srx_len = 0;
	core_mp->key_len = 0;
	core_mp->st_len = 0;
	core_mp->tdf = 240;
	core_mp->busy_cdc = INT_CHECK;
	core_mp->retry = ilits->mp_retry;
	core_mp->td_retry = false;
	core_mp->final_result = MP_DATA_FAIL;
	core_mp->lost_benchmark = false;
	core_mp->lost_parameter = false;
	core_mp->all_pass = false;
	ILI_INFO("============== TP & Panel info ================\n");
	ILI_INFO("Driver version = %s\n", DRIVER_VERSION);
	/*ILI_INFO("TP Module = %s\n", ilits->md_name);*/
	ILI_INFO("CHIP = 0x%x\n", core_mp->chip_pid);
	ILI_INFO("Firmware version = %x\n", core_mp->fw_ver);
	ILI_INFO("Protocol version = %x\n", core_mp->protocol_ver);
	ILI_INFO("Core version = %x\n", core_mp->core_ver);
	ILI_INFO("Read CDC Length = %d\n", core_mp->cdc_len);
	ILI_INFO("X length = %d, Y length = %d\n", core_mp->xch_len, core_mp->ych_len);
	ILI_INFO("Frame length = %d\n", core_mp->frame_len);
	ILI_INFO("Check busy method = %s\n",
		 (core_mp->busy_cdc ? "Polling" : "Interrupt"));
	ILI_INFO("===============================================\n");

	for (i = 0; i < MP_TEST_ITEM; i++) {
		tItems[i].spec_option = 0;
		tItems[i].type_option = 0;
		tItems[i].run = false;
		tItems[i].max = 0;
		tItems[i].max_res = MP_DATA_FAIL;
		tItems[i].item_result = MP_DATA_PASS;
		tItems[i].min = 0;
		tItems[i].min_res = MP_DATA_FAIL;
		tItems[i].frame_count = 0;
		tItems[i].trimmed_mean = 0;
		tItems[i].lowest_percentage = 0;
		tItems[i].highest_percentage = 0;
		tItems[i].v_tdf_1 = 0;
		tItems[i].v_tdf_2 = 0;
		tItems[i].h_tdf_1 = 0;
		tItems[i].h_tdf_2 = 0;
		tItems[i].max_min_mode = 0;
		tItems[i].bch_mrk_multi = false;
		tItems[i].bch_mrk_frm_num = 1;
		tItems[i].goldenmode = 0;
		tItems[i].retry_cnt = RETRY_COUNT;
		tItems[i].result_buf = NULL;
		tItems[i].buf = NULL;
		tItems[i].max_buf = NULL;
		tItems[i].min_buf = NULL;
		tItems[i].bench_mark_max = NULL;
		tItems[i].bench_mark_min = NULL;
		/*tItems[i].bch_mrk_max = NULL;*/
		/*tItems[i].bch_mrk_min = NULL;*/
		tItems[i].node_type = NULL;
		tItems[i].delay_time = 0;
		tItems[i].test_int_pin = 0;
		tItems[i].int_pulse_test = 0;

		if (tItems[i].catalog == MUTUAL_TEST) {
			tItems[i].do_test = mutual_test;

		} else if (tItems[i].catalog == TX_RX_DELTA) {
			tItems[i].do_test = mutual_test;

		} else if (tItems[i].catalog == UNTOUCH_P2P) {
			tItems[i].do_test = mutual_test;

		} else if (tItems[i].catalog == PIXEL) {
			tItems[i].do_test = mutual_test;

		} else if (tItems[i].catalog == OPEN_TEST) {
			if (ipio_strcmp(tItems[i].desp, "open test_c") == 0) {
				tItems[i].do_test = open_test_cap;

			} else {
				tItems[i].do_test = mutual_test;
			}

		} else if (tItems[i].catalog == KEY_TEST) {
			tItems[i].do_test = key_test;

		} else if (tItems[i].catalog == SELF_TEST) {
			tItems[i].do_test = self_test;

		} else if (tItems[i].catalog == ST_TEST) {
			tItems[i].do_test = st_test;

		} else if (tItems[i].catalog == PEAK_TO_PEAK_TEST) {
			tItems[i].do_test = peak_to_peak_test;

		} else if (tItems[i].catalog == SHORT_TEST) {
			tItems[i].do_test = mutual_test;
		}

		tItems[i].result = kmalloc(16, GFP_KERNEL);
		snprintf(tItems[i].result, 16, "%s", "FAIL");
	}
}

static void mp_p2p_td_retry_after_ra_fail(int p2p_td)
{
	int i;

	for (i = 0; i < MP_TEST_ITEM; i++) {
		if (ipio_strcmp(tItems[i].desp,
				"noise peak to peak(with panel) (lcm off)") == 0) {
			break;
		}
	}

	if (i >= MP_TEST_ITEM) {
		return;
	}

	ILI_DBG("i = %d, p2p_noise_ret = %d, p2p_noise_run = %d\n",
		i, tItems[i].item_result, tItems[i].run);

	if (tItems[i].item_result == MP_DATA_PASS && tItems[i].run == 1) {
		tItems[p2p_td].do_test(p2p_td);
	}
}

static void mp_test_run(bool lcm_on, int test_index,
			struct auto_testdata *testdata)
{
	int i = 0;
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	for (i = 0; i < MP_TEST_ITEM; i++) {
		if (tItems[i].test_index == test_index && tItems[i].lcm == lcm_on) {
			ILI_INFO("Run MP Test Item : %s test_index = %d\n", tItems[i].desp, test_index);
			break;
		}
	}

	if (i >= MP_TEST_ITEM) {
		ILI_ERR("Not found test_index : %d lcm_on = %d\n", test_index, lcm_on);
		return;
	}

	if (strncmp("open test_c", tItems[i].desp, strlen("open test_c")) == 0) {
		ilitek_get_para_openshort(testdata, &tItems[i], &core_mp->open_para,
					  tItems[i].test_index);

	} else {
		ilitek_get_item_para(testdata, &tItems[i], tItems[i].test_index);
	}

	if (!tItems[i].run) {
		ILI_ERR("Not found test_index : %d lcm_on = %d, run = %d\n", test_index, lcm_on,
			tItems[i].run);
		return;
	}

	if (tItems[i].goldenmode && (tItems[i].spec_option != tItems[i].goldenmode)) {
		core_mp->lost_benchmark = true;
	}

	ILI_DBG("%s: run = %d, max = %d, min = %d, frame_count = %d\n", tItems[i].desp,
		tItems[i].run, tItems[i].max, tItems[i].min, tItems[i].frame_count);
	ILI_DBG("v_tdf_1 = %d, v_tdf_2 = %d, h_tdf_1 = %d, h_tdf_2 = %d",
		tItems[i].v_tdf_1,
		tItems[i].v_tdf_2, tItems[i].h_tdf_1, tItems[i].h_tdf_2);

	if (!tItems[i].do_test) {
		ILI_ERR("Run MP Test Item : %s, do_test func is NULL, so return\n", tItems[i].desp);
		return;
	}

	tItems[i].do_test(i);
	mp_compare_test_result(i);

	/* P2P TD retry after RA sample failed. */
	if (ipio_strcmp(tItems[i].desp, "peak to peak_td (lcm off)") == 0 &&
			tItems[i].item_result == MP_DATA_FAIL) {
		/*parser_get_int_data(tItems[i].desp, "recheck ptop lcm off", str, sizeof(str));*/
		/*ILI_INFO("Peak to Peak TD retry = %d\n", ili_katoi(str));*/
		core_mp->td_retry = 0;/*ili_katoi(str);*/

		if (core_mp->td_retry) {
			mp_p2p_td_retry_after_ra_fail(i);
		}
	}

	if (core_mp->retry && tItems[i].item_result == MP_DATA_FAIL) {
		ILI_INFO("MP failed, doing retry %d times\n", tItems[i].retry_cnt);
		mp_do_retry(i, tItems[i].retry_cnt);
	}
}

static void mp_test_free(void)
{
	int i, j;
	struct core_mp_test_data *core_mp = &ilits->core_mp;
	ILI_INFO("Free all allocated mem for MP\n");
	core_mp->final_result = MP_DATA_FAIL;
	core_mp->td_retry = false;

	for (i = 0; i < MP_TEST_ITEM; i++) {
		tItems[i].run = false;
		tItems[i].max_res = MP_DATA_FAIL;
		tItems[i].min_res = MP_DATA_FAIL;
		tItems[i].item_result = MP_DATA_PASS;

		if (tItems[i].catalog == TX_RX_DELTA) {
			ili_kfree((void **)&core_mp->rx_delta_buf);
			ili_kfree((void **)&core_mp->tx_delta_buf);
			ili_kfree((void **)&core_mp->tx_max_buf);
			ili_kfree((void **)&core_mp->tx_min_buf);
			ili_kfree((void **)&core_mp->rx_max_buf);
			ili_kfree((void **)&core_mp->rx_min_buf);

		} else {
			ili_kfree((void **)&tItems[i].result);
			ili_kfree((void **)&tItems[i].result_buf);
			ili_kfree((void **)&tItems[i].max_buf);
			ili_kfree((void **)&tItems[i].min_buf);
			ili_vfree((void **)&tItems[i].buf);
		}
	}

	ili_kfree((void **)&frame_buf);
	ili_kfree((void **)&key_buf);

	if (frm_buf != NULL) {
		for (j = 0; j < 3; j++)
			if (frm_buf[j] != NULL) {
				kfree(frm_buf[j]);
				frm_buf[j] = NULL;
			}

		kfree(frm_buf);
		frm_buf = NULL;
	}
}

/* The method to copy results to user depends on what APK needs */
static void mp_copy_ret_to_apk(char *buf, struct seq_file *s, char *message)
{
	int i, len = 2, count = 0;
	int test_failed_items = 0;

	if (!buf && ERR_ALLOC_MEM(message) && ERR_ALLOC_MEM(s)) {
		ILI_ERR("set message buffer is null\n");
		return;
	}

	for (i = 0; i < MP_TEST_ITEM; i++) {
		if (!tItems[i].run) {
			continue;
		}

		if (tItems[i].item_result == MP_DATA_FAIL) {
			test_failed_items++;
			ILI_ERR("[%s] = FAIL\n", tItems[i].desp);

			if (!ERR_ALLOC_MEM(buf)) {
				len += snprintf(buf + len, PAGE_SIZE - len, "[%s] = FAIL\n", tItems[i].desp);
			}

			if (!ERR_ALLOC_MEM(message)) {
				if (count < MESSAGE_SIZE) {
					count += snprintf(message + count, MESSAGE_SIZE - count,
							  "%s FAIL; 	   ", tItems[i].desp);
				}
			}

			if (!ERR_ALLOC_MEM(s)) {
				seq_printf(s, "%s FAIL; 	   ", tItems[i].desp);
			}

		} else {
			ILI_INFO("[%s] = PASS\n", tItems[i].desp);

			if (!ERR_ALLOC_MEM(buf)) {
				len += snprintf(buf + len, PAGE_SIZE - len, "[%s] = PASS\n", tItems[i].desp);
			}
		}
	}

	ilits->mp_result_count = test_failed_items;
}

static void ilitek_tddi_mp_all_pass_check(void)
{
	int ret = 0, retry = 5;
	u8 cmd[1] = {0xFD};
	u8 temp[3] = {0};
	struct core_mp_test_data *core_mp = &ilits->core_mp;

	if (ilits->eng_flow) {
		core_mp->all_pass = true;
		ILI_INFO("eng_flow MP test\n");
		return;
	}

	ilits->wait_int_timeout = 20;

	do {
		ret = ilits->wrapper(cmd, 1, temp, 3, ON, OFF);

		if (ret < 0) {
			ILI_ERR("Write MP All Pass command failed\n");
			core_mp->all_pass = false;

		} else {
			if (temp[0] == 0xFD) {
				break;
			}
		}
	} while (--retry >= 0);

	ilits->wait_int_timeout = MP_INT_TIMEOUT;

	if (retry <= 0) {
		ILI_ERR("MP All Pass command failed, normal mode\n");
		core_mp->all_pass = false;
		return;
	}

	if (temp[2] == ili_calc_packet_checksum(temp, sizeof(temp) - 1) &&
			temp[0] == 0xFD && temp[1] == 0x02) {
			/*0xFD:back door command header, 0x02:Normal mode*/
		core_mp->all_pass = true;
		ILI_ERR("MP mode check: Back door mode, MP All Pass\n");

	} else {
		core_mp->all_pass =
			false;              /*0xFD:back door command header, 0x01:Normal mode*/
		ILI_ERR("MP mode check: Normal mode \n");
	}
}

int ili_mp_test_main(char *apk, struct seq_file *s,
		     char *message, int msg_size,
		     bool lcm_on, void *chip_data,
		     struct auto_testdata *testdata)
{
	int i, ret = 0;
	struct ilitek_ts_data *chip_info;
	struct core_mp_test_data *core_mp;
	chip_info = (struct ilitek_ts_data *)chip_data;
	core_mp = &chip_info->core_mp;

	if (ilits->xch_num <= 0 || ilits->ych_num <= 0) {
		ILI_ERR("Invalid frame length (%d, %d)\n", ilits->xch_num, ilits->ych_num);

		if (!ERR_ALLOC_MEM(message)) {
			snprintf(message, MESSAGE_SIZE, "Invalid frame length (xch %d, ych %d)\n",
				 ilits->xch_num,
				 ilits->ych_num);
		}

		if (!ERR_ALLOC_MEM(s)) {
			seq_printf(s, "Invalid frame length (xch %d, ych %d)\n", ilits->xch_num,
				   ilits->ych_num);
		}

		ret = -EMP_INVAL;
		goto out;
	}

	ilitek_tddi_mp_init_item();
	ilitek_tddi_mp_all_pass_check();

	/* Read timing info from ini file */
	if (mp_get_timing_info(chip_data, testdata) < 0) {
		ILI_ERR("Failed to get timing info from ini\n");

		if (!ERR_ALLOC_MEM(message)) {
			snprintf(message, MESSAGE_SIZE, "Failed to get timing info from ini\n");
		}

		if (!ERR_ALLOC_MEM(s)) {
			seq_printf(s, "Failed to get timing info from ini\n");
		}

		ret = -EMP_TIMING_INFO;
		goto out;
	}

#if MP_INT_LEVEL

	if (ili_ic_int_trigger_ctrl(true) < 0) {
		ILI_ERR("Failed to set INT as Level trigger\n");

		if (!ERR_ALLOC_MEM(message)) {
			snprintf(message, MESSAGE_SIZE, "Failed to set INT as Level trigger\n");
		}

		if (!ERR_ALLOC_MEM(s)) {
			seq_printf(s, "Failed to set INT as Level trigger\n");
		}

		ret = -EMP_CMD;
		goto out;
	}

#endif

	for (i = 0; i < TYPE_MAX; i++) {
		if (!core_mp->test_index[i]) {
			ILI_INFO("test end\n");
			break;
		}

		mp_test_run(lcm_on, core_mp->test_index[i], testdata);
		msleep(5);
	}

	ret = mp_show_result(chip_data, testdata, lcm_on);
	mp_copy_ret_to_apk(apk, s, message);
out:
	mp_test_free();
#if MP_INT_LEVEL

	if (ili_ic_int_trigger_ctrl(false) < 0) {
		ILI_ERR("Failed to set INT back to pluse trigger\n");

		if (!ERR_ALLOC_MEM(message)) {
			snprintf(message, MESSAGE_SIZE, "Failed to set INT back to pluse trigger\n");
		}

		if (!ERR_ALLOC_MEM(s)) {
			seq_printf(s, "Failed to set INT back to pluse trigger\n");
		}

		ret = -EMP_CMD;
	}

#endif
	return ret;
}

static int ili7807_auto_test_preoperation(struct seq_file *s,
		void *chip_data,
		struct auto_testdata *testdata)
{
	/*char apk_ret[100] = {0};*/
	struct ilitek_ts_data *chip_info = (struct ilitek_ts_data *)chip_data;
	TPD_INFO("s->size = %d  s->count = %d\n", (int)s->size, (int)s->count);
	mutex_lock(&chip_info->touch_mutex);
	enable_irq(ilits->irq_num);/*because oplus disable*/
	chip_info->mp_result_count = 0;
	ili_mp_test_handler(NULL, s, NULL, 0, ON, chip_info, testdata);
	disable_irq_nosync(ilits->irq_num);
	mutex_unlock(&chip_info->touch_mutex);
	TPD_INFO("chip_info->mp_result_count = %d\n", chip_info->mp_result_count);
	return chip_info->mp_result_count;
}

static int ili7807_black_screen_preoperation(char *msg, int msg_size,
		void *chip_data,
		struct auto_testdata *testdata)
{
	/*char apk_ret[100] = {0};*/
	struct ilitek_ts_data *chip_info = (struct ilitek_ts_data *)chip_data;
	mutex_lock(&chip_info->touch_mutex);
	/*enable_irq(ilits->irq_num);  because oplus disable*/
	chip_info->mp_result_count = 0;
	ili_mp_test_handler(NULL, NULL, msg, msg_size, OFF, chip_info, testdata);
	/*disable_irq_nosync(ilits->irq_num);*/
	mutex_unlock(&chip_info->touch_mutex);
	TPD_INFO("chip_info->mp_result_count = %d\n", chip_info->mp_result_count);
	return chip_info->mp_result_count;
}


struct ilitek_test_operations ilitek_7807_test_ops = {
	.auto_test_preoperation = ili7807_auto_test_preoperation,
	.black_screen_preoperation = ili7807_black_screen_preoperation,
};

struct engineer_test_operations ilitek_7807_engineer_test_ops = {
	.auto_test               = ilitek_auto_test,
	.black_screen_test = ilitek_black_screen_test,
};
