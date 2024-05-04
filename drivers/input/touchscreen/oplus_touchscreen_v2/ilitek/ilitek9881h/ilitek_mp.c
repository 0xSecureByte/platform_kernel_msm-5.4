// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include "ilitek.h"

#define MP_PASS            0
#define MP_FAIL            -1
#define VALUE            0

#define RETRY_COUNT        3
#define INT_CHECK        0
#define POLL_CHECK        1
#define DELAY_CHECK        2

#define BENCHMARK        1

#define TYPE_BENCHMARK        0
#define TYPE_NO_JUGE        1
#define TYPE_JUGE        2

#define NORMAL_CSV_PASS_NAME    "mp_pass"
#define NORMAL_CSV_FAIL_NAME    "mp_fail"
#define CSV_FILE_SIZE        (1 * M)

#define PARSER_MAX_CFG_BUF        (512 * 3)

#define PARSER_MAX_KEY_NAME_LEN        100
#define PARSER_MAX_KEY_VALUE_LEN    2000
#define BENCHMARK_STR_SIZE          256
#define BENCHMARK_KEY_NAME        "benchmark_data"
#define NODE_TYPE_KEY_NAME        "node type"
#define INI_ERR_OUT_OF_LINE        -1

#define CMD_MUTUAL_DAC            0x1
#define CMD_MUTUAL_BG            0x2
#define CMD_MUTUAL_SIGNAL        0x3
#define CMD_MUTUAL_NO_BK        0x5
#define CMD_MUTUAL_HAVE_BK        0x8
#define CMD_MUTUAL_BK_DAC        0x10
#define CMD_SELF_DAC            0xC
#define CMD_SELF_BG            0xF
#define CMD_SELF_SIGNAL            0xD
#define CMD_SELF_NO_BK            0xE
#define CMD_SELF_HAVE_BK        0xB
#define CMD_SELF_BK_DAC            0x11
#define CMD_KEY_DAC            0x14
#define CMD_KEY_BG            0x16
#define CMD_KEY_NO_BK            0x7
#define CMD_KEY_HAVE_BK            0x15
#define CMD_KEY_OPEN            0x12
#define CMD_KEY_SHORT            0x13
#define CMD_ST_DAC            0x1A
#define CMD_ST_BG            0x1C
#define CMD_ST_NO_BK            0x17
#define CMD_ST_HAVE_BK            0x1B
#define CMD_ST_OPEN            0x18
#define CMD_TX_SHORT            0x19
#define CMD_RX_SHORT            0x4
#define CMD_RX_OPEN            0x6
#define CMD_TX_RX_DELTA            0x1E
#define CMD_CM_DATA            0x9
#define CMD_CS_DATA            0xA
#define CMD_TRCRQ_PIN            0x20
#define CMD_RESX2_PIN            0x21
#define CMD_MUTUAL_INTEGRA_TIME        0x22
#define CMD_SELF_INTEGRA_TIME        0x23
#define CMD_KEY_INTERGRA_TIME        0x24
#define CMD_ST_INTERGRA_TIME        0x25
#define CMD_PEAK_TO_PEAK        0x1D
#define CMD_GET_TIMING_INFO        0x30
#define CMD_DOZE_P2P            0x32
#define CMD_DOZE_RAW            0x33

#define DUMP(fmt, arg...)        \
	do {                    \
		if ((LEVEL_DEBUG == tp_debug) || ipio_debug_level)    \
		pr_cont(fmt, ##arg);        \
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
};

struct mp_test_open_c {
	s32 *cap_dac;
	s32 *cap_raw;
	s32 *dcl_cap;
};

struct mp_test_items {
	char *name;
	/* The description must be the same as ini's section name */
	char *desp;
	char *result;
	int catalog;
	u8 cmd;
	u8 spec_option;
	u8 type_option;
	bool run;
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
	s32 *result_buf;
	s32 *buf;
	s32 *max_buf;
	s32 *min_buf;
	s32 *bench_mark_max;
	s32 *bench_mark_min;
	s32 *node_type;
	int (*do_test)(void *chip_data, int index);
	int test_index;
	u8 p5v_cmd[16];
};

#define MP_TEST_ITEM    48
static struct mp_test_items tItems[MP_TEST_ITEM] = {
	{.name = "mutual_dac", .desp = "calibration data(dac)", .result = "FAIL", .catalog = MUTUAL_TEST, .test_index = TYPE_TEST6},
	{.name = "mutual_bg", .desp = "baseline data(bg)", .result = "FAIL", .catalog = MUTUAL_TEST},
	{.name = "mutual_signal", .desp = "untouch signal data(bg-raw-4096) - mutual", .result = "FAIL", .catalog = MUTUAL_TEST},
	{.name = "mutual_no_bk", .desp = "raw data(no bk)", .result = "FAIL", .catalog = MUTUAL_TEST, .test_index = TYPE_TEST5},
	{.name = "mutual_has_bk", .desp = "raw data(have bk)", .result = "FAIL", .catalog = MUTUAL_TEST},
	{.name = "mutual_bk_dac", .desp = "manual bk data(mutual)", .result = "FAIL", .catalog = MUTUAL_TEST},
	{.name = "self_dac", .desp = "calibration data(dac) - self", .result = "FAIL", .catalog = SELF_TEST},
	{.name = "self_bg", .desp = "baselin data(bg,self_tx,self_r)", .result = "FAIL", .catalog = SELF_TEST},
	{.name = "self_signal", .desp = "untouch signal data(bg-raw-4096) - self", .result = "FAIL", .catalog = SELF_TEST},
	{.name = "self_no_bk", .desp = "raw data(no bk) - self", .result = "FAIL", .catalog = SELF_TEST},
	{.name = "self_has_bk", .desp = "raw data(have bk) - self", .result = "FAIL", .catalog = SELF_TEST},
	{.name = "self_bk_dac", .desp = "manual bk dac data(self_tx,self_rx)", .result = "FAIL", .catalog = SELF_TEST},
	{.name = "key_dac", .desp = "calibration data(dac/icon)", .result = "FAIL", .catalog = KEY_TEST},
	{.name = "key_bg", .desp = "key baseline data", .result = "FAIL", .catalog = KEY_TEST},
	{.name = "key_no_bk", .desp = "key raw data", .result = "FAIL", .catalog = KEY_TEST},
	{.name = "key_has_bk", .desp = "key raw bk dac", .result = "FAIL", .catalog = KEY_TEST},
	{.name = "key_open", .desp = "key raw open test", .result = "FAIL", .catalog = KEY_TEST},
	{.name = "key_short", .desp = "key raw short test", .result = "FAIL", .catalog = KEY_TEST},
	{.name = "st_dac", .desp = "st calibration data(dac)", .result = "FAIL", .catalog = ST_TEST},
	{.name = "st_bg", .desp = "st baseline data(bg)", .result = "FAIL", .catalog = ST_TEST},
	{.name = "st_no_bk", .desp = "st raw data(no bk)", .result = "FAIL", .catalog = ST_TEST},
	{.name = "st_has_bk", .desp = "st raw(have bk)", .result = "FAIL", .catalog = ST_TEST},
	{.name = "st_open", .desp = "st open data", .result = "FAIL", .catalog = ST_TEST},
	{.name = "tx_short", .desp = "tx short test", .result = "FAIL", .catalog = MUTUAL_TEST},
	{.name = "rx_short", .desp = "short test -ili9881", .result = "FAIL", .catalog = SHORT_TEST, .test_index = TYPE_TEST4},
	{.name = "rx_open", .desp = "rx open", .result = "FAIL", .catalog = MUTUAL_TEST},
	{.name = "cm_data", .desp = "untouch cm data", .result = "FAIL", .catalog = MUTUAL_TEST},
	{.name = "cs_data", .desp = "untouch cs data", .result = "FAIL", .catalog = MUTUAL_TEST},
	{.name = "tx_rx_delta", .desp = "tx/rx delta", .result = "FAIL", .catalog = TX_RX_DELTA},
	{.name = "p2p", .desp = "untouch peak to peak", .result = "FAIL", .catalog = UNTOUCH_P2P},
	{.name = "pixel_no_bk", .desp = "pixel raw (no bk)", .result = "FAIL", .catalog = PIXEL},
	{.name = "pixel_has_bk", .desp = "pixel raw (have bk)", .result = "FAIL", .catalog = PIXEL},
	{.name = "open_integration", .desp = "open test(integration)", .result = "FAIL", .catalog = OPEN_TEST},
	{.name = "open_cap", .desp = "open test(cap)", .result = "FAIL", .catalog = OPEN_TEST},
	/* New test items for protocol 5.4.0 as below */
	{.name = "noise_peak_to_peak_ic", .desp = "noise peak to peak(ic only)", .result = "FAIL", .catalog = PEAK_TO_PEAK_TEST, .test_index = TYPE_TEST3},
	{.name = "noise_peak_to_peak_panel", .desp = "noise peak to peak(with panel)", .result = "FAIL", .catalog = PEAK_TO_PEAK_TEST, .test_index = TYPE_TEST2},
	{.name = "noise_peak_to_peak_ic_lcm_off", .desp = "noise peak to peak(ic only) (lcm off)", .result = "FAIL", .catalog = PEAK_TO_PEAK_TEST, .test_index = TYPE_TEST14},
	{.name = "noise_peak_to_peak_panel_lcm_off", .desp = "noise peak to peak(with panel) (lcm off)", .result = "FAIL", .catalog = PEAK_TO_PEAK_TEST, .test_index = TYPE_TEST13},
	{.name = "mutual_no_bk_lcm_off", .desp = "raw data(no bk) (lcm off)", .result = "FAIL", .catalog = MUTUAL_TEST, .test_index = TYPE_TEST12},
	{.name = "mutual_has_bk_lcm_off", .desp = "raw data(have bk) (lcm off)", .result = "FAIL", .catalog = MUTUAL_TEST},
	{.name = "open_integration_sp", .desp = "open test(integration)_sp", .result = "FAIL", .catalog = OPEN_TEST},
	{.name = "doze_raw", .desp = "doze raw data", .result = "FAIL", .catalog = MUTUAL_TEST, .test_index = TYPE_TEST7},
	{.name = "doze_p2p", .desp = "doze peak to peak", .result = "FAIL", .catalog = PEAK_TO_PEAK_TEST, .test_index = TYPE_TEST8},
	{.name = "doze_raw_td_lcm_off", .desp = "raw data_td (lcm off)", .result = "FAIL", .catalog = MUTUAL_TEST, .test_index = TYPE_TEST15},
	{.name = "doze_p2p_td_lcm_off", .desp = "peak to peak_td (lcm off)", .result = "FAIL", .catalog = PEAK_TO_PEAK_TEST, .test_index = TYPE_TEST16},
	{.name = "rx_short", .desp = "short test", .result = "FAIL", .catalog = SHORT_TEST},
	{.name = "open test_c", .desp = "open test_c", .result = "FAIL", .catalog = OPEN_TEST, .test_index = TYPE_TEST9},
	{.name = "touch deltac", .desp = "touch deltac", .result = "FAIL", .catalog = MUTUAL_TEST, .test_index = TYPE_TEST10},
};

static int  ilitek_get_item_para(struct auto_testdata *testdata,
				 struct mp_test_items *p_mp_test_items, int item_index)
{
	struct test_item_info *p_test_item_info = NULL;
	int ret = 0;
	int j  = 0;

	if (!testdata || !p_mp_test_items) {
		TPD_INFO("item: %d testdata or p_mp_test_items is null\n", item_index);
		return -1;
	}

	p_test_item_info = get_test_item_info(testdata->fw, item_index);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", item_index);
		ret = -1;

	} else {
		if (p_test_item_info->p_buffer[0] == 1 && p_test_item_info->para_num >= 32) {
			/*p5_4 cmd*/
			for (j = 0; j < (ARRAY_SIZE(p_mp_test_items->p5v_cmd)); j++) {
				p_mp_test_items->p5v_cmd[j] = (u8) p_test_item_info->p_buffer[j + 1];
			}

			p_mp_test_items->spec_option = (u8) p_test_item_info->p_buffer[17];
			p_mp_test_items->type_option = (u8) p_test_item_info->p_buffer[18];
			p_mp_test_items->run = p_test_item_info->p_buffer[19];
			p_mp_test_items->max = p_test_item_info->p_buffer[20];
			p_mp_test_items->max_res = p_test_item_info->p_buffer[21];
			p_mp_test_items->min = p_test_item_info->p_buffer[22];
			p_mp_test_items->min_res = p_test_item_info->p_buffer[23];
			p_mp_test_items->frame_count = p_test_item_info->p_buffer[24];
			p_mp_test_items->trimmed_mean = p_test_item_info->p_buffer[25];
			p_mp_test_items->lowest_percentage = p_test_item_info->p_buffer[26];
			p_mp_test_items->highest_percentage = p_test_item_info->p_buffer[27];
			p_mp_test_items->v_tdf_1 = p_test_item_info->p_buffer[28];
			p_mp_test_items->v_tdf_2 = p_test_item_info->p_buffer[29];
			p_mp_test_items->h_tdf_1 = p_test_item_info->p_buffer[30];
			p_mp_test_items->h_tdf_2 = p_test_item_info->p_buffer[31];
			TPD_DEBUG("item: %d,"
				  "spec_option:%d,type_option:%d,"
				  "run:%d,max:%d,"
				  "max_res:%d,min:%d,"
				  "min_res:%d,frame_count:%d,"
				  "trimmed_mean:%d,lowest_percentage:%d,"
				  "highest_percentage:%d,v_tdf_1:%d,"
				  "v_tdf_2:%d,h_tdf_1:%d,h_tdf_2:%d\n", item_index,
				  p_mp_test_items->spec_option, p_mp_test_items->type_option,
				  p_mp_test_items->run, p_mp_test_items->max,
				  p_mp_test_items->max_res, p_mp_test_items->min,
				  p_mp_test_items->min_res, p_mp_test_items->frame_count,
				  p_mp_test_items->trimmed_mean, p_mp_test_items->lowest_percentage,
				  p_mp_test_items->highest_percentage, p_mp_test_items->v_tdf_1,
				  p_mp_test_items->v_tdf_2, p_mp_test_items->h_tdf_1,
				  p_mp_test_items->h_tdf_2);
			TPD_DEBUG("p5v_cmd:%*ph\n", sizeof(p_mp_test_items->p5v_cmd),
				  p_mp_test_items->p5v_cmd);
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

static int  ilitek_get_para_openshort(struct auto_testdata *testdata,
				      struct mp_test_items *p_mp_test_items,
				      struct open_test_c_spec *p_open_test,
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
		ret = -1;

	} else {
		if (p_test_item_info->p_buffer[0] == 1 && p_test_item_info->para_num >= 50) {
			/*p5_4 cmd*/
			for (j = 0; j < (ARRAY_SIZE(p_open_test->cap1_dac_cmd)); j++) {
				p_open_test->cap1_dac_cmd[j] = (u8) p_test_item_info->p_buffer[j + 1];
			}

			/*p5_4 cmd*/
			for (j = 0; j < (ARRAY_SIZE(p_open_test->cap1_raw_cmd)); j++) {
				p_open_test->cap1_raw_cmd[j] = (u8) p_test_item_info->p_buffer[j + 1 +
							       ARRAY_SIZE(p_open_test->cap1_dac_cmd)];
			}

			p_mp_test_items->spec_option = (u8) p_test_item_info->p_buffer[33];
			p_mp_test_items->type_option = (u8) p_test_item_info->p_buffer[34];
			p_mp_test_items->run = p_test_item_info->p_buffer[35];
			p_mp_test_items->max = p_test_item_info->p_buffer[36];
			p_mp_test_items->max_res = p_test_item_info->p_buffer[37];
			p_mp_test_items->min = p_test_item_info->p_buffer[38];
			p_mp_test_items->min_res = p_test_item_info->p_buffer[39];
			p_mp_test_items->frame_count = p_test_item_info->p_buffer[40];
			p_mp_test_items->trimmed_mean = p_test_item_info->p_buffer[41];
			p_mp_test_items->lowest_percentage = p_test_item_info->p_buffer[42];
			p_mp_test_items->highest_percentage = p_test_item_info->p_buffer[43];
			p_mp_test_items->v_tdf_1 = p_test_item_info->p_buffer[44];
			p_mp_test_items->v_tdf_2 = p_test_item_info->p_buffer[45];
			p_mp_test_items->h_tdf_1 = p_test_item_info->p_buffer[46];
			p_mp_test_items->h_tdf_2 = p_test_item_info->p_buffer[47];
			p_open_test->tvch = p_test_item_info->p_buffer[48];
			p_open_test->tvcl = p_test_item_info->p_buffer[49];
			p_open_test->gain = p_test_item_info->p_buffer[50];
			TPD_DEBUG("item: %d,"
				  "spec_option:%d,type_option:%d,"
				  "run:%d,max:%d,"
				  "max_res:%d,min:%d,"
				  "min_res:%d,frame_count:%d,"
				  "trimmed_mean:%d,lowest_percentage:%d,"
				  "highest_percentage:%d,v_tdf_1:%d,"
				  "v_tdf_2:%d,h_tdf_1:%d,h_tdf_2:%d\n",
				  "tvch:%d,tvcl:%d,gain:%d\n", item_index,
				  p_mp_test_items->spec_option, p_mp_test_items->type_option,
				  p_mp_test_items->run, p_mp_test_items->max,
				  p_mp_test_items->max_res, p_mp_test_items->min,
				  p_mp_test_items->min_res, p_mp_test_items->frame_count,
				  p_mp_test_items->trimmed_mean, p_mp_test_items->lowest_percentage,
				  p_mp_test_items->highest_percentage, p_mp_test_items->v_tdf_1,
				  p_mp_test_items->v_tdf_2, p_mp_test_items->h_tdf_1,
				  p_mp_test_items->h_tdf_2, p_open_test->tvch,
				  p_open_test->tvcl, p_open_test->gain);
			TPD_DEBUG("cap1_dac_cmd:%*ph\n", sizeof(p_open_test->cap1_dac_cmd),
				  p_open_test->cap1_dac_cmd);
			TPD_DEBUG("cap1_raw_cmd:%*ph\n", sizeof(p_open_test->cap1_raw_cmd),
				  p_open_test->cap1_raw_cmd);
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

static void dump_benchmark_data(void *chip_data, s32 *max_ptr, s32 *min_ptr)
{
	int i;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct core_mp_test_data *core_mp = NULL;

	core_mp = chip_info->core_mp;

	if (1) {
		TPD_INFO("Dump Benchmark Max\n");

		for (i = 0; i < core_mp->frame_len; i++) {
			pr_cont("%d, ", max_ptr[i]);

			if (i % core_mp->xch_len == core_mp->xch_len - 1) {
				pr_cont("\n");
			}
		}

		pr_cont("Dump Denchmark Min\n");

		for (i = 0; i < core_mp->frame_len; i++) {
			pr_cont("%d, ", min_ptr[i]);

			if (i % core_mp->xch_len == core_mp->xch_len - 1) {
				pr_cont("\n");
			}
		}
	}
}

static int run_open_test(void *chip_data, int index)
{
	int i, x, y, k, ret = 0;
	int border_x[] = { -1, 0, 1, 1, 1, 0, -1, -1};
	int border_y[] = { -1, -1, -1, 0, 1, 1, 1, 0};
	s32 *p_comb = NULL;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct core_mp_test_data *core_mp = NULL;

	core_mp = chip_info->core_mp;
	p_comb = core_mp->frame_buf;

	if (strcmp(tItems[index].name, "open_integration") == 0) {
		for (i = 0; i < core_mp->frame_len; i++) {
			tItems[index].buf[i] = p_comb[i];
		}

	} else if (strcmp(tItems[index].name, "open_cap") == 0) {
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

				avg = (sum + centre) / (count + 1);    /* plus 1 because of centre */
				tItems[index].buf[shift + x] = (centre * 100) / avg;
			}
		}
	}

	return ret;
}

static void mp_print_csv_header(void *chip_data, char *csv, int *csv_len,
				int *csv_line)
{
	int i;
	int tmp_len = *csv_len;
	int tmp_line = *csv_line;
	/*uint8_t data_buf[128];*/
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	/* header must has 19 line*/
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
			    "==============================================================================\n");

	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
			    "ILITek C-TP Utility V%s    %x : Driver Sensor Test\n",
			    DRIVER_VERSION, chip_info->core_mp->chip_pid);
	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
			    "OPLUS_%d\n", get_project());

	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
			    "Firmware Version ,0x%02x\n", (chip_info->core_mp->fw_ver >> 8) & 0xFF);

	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
			    "Panel information ,XCH=%d, YCH=%d\n",
			    chip_info->core_mp->xch_len,
			    chip_info->core_mp->ych_len);

	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
			    "Test Platform ,Mobile\n");

	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
			    "This is %s module\n",
			    chip_info->ts->panel_data.manufacture_info.manufacture);

	tmp_line++;
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len, "Test Item:\n");

	tmp_line++;

	for (i = 0; i < ARRAY_SIZE(tItems); i++) {
		if (tItems[i].run == 1) {
			if (tItems[i].item_result == MP_PASS) {
				tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
						    "      ---%s,OK\n", tItems[i].desp);

				tmp_line++;

			} else {
				TPD_INFO("---%s,NG\n", tItems[i].desp);
				tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
						    "      ---%s,NG\n", tItems[i].desp);

				tmp_line++;
			}
		}
	}

	/* print final result*/
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
			    "Final Result ,%s\n",
			    (chip_info->core_mp->final_result == MP_PASS) ? "PASS" : "FAIL");

	tmp_line++;

	while (tmp_line < 19) {
		tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len, "\n");
		tmp_line++;
	}

	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
			    "==============================================================================\n");

	tmp_line++;

	*csv_len = tmp_len;
	*csv_line = tmp_line;
}

static void mp_print_csv_cdc_cmd(void *chip_data, char *csv, int *csv_len,
				 int index)
{
	int tmp_len = *csv_len;
	char str[128] = {0};
	int i, j,  j_len = 0;
	char *name = tItems[index].desp;
	char *open_c_cmd[] = {"open cap1 dac", "open cap1 raw", "open cap2 dac", "open cap2 raw"};
	unsigned char open_c_cmd_data[16] = {0};/*OPEN CAP1 DAC*/
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct core_mp_test_data *core_mp = NULL;

	core_mp = chip_info->core_mp;

	if (strncmp(name, "open test_c", strlen(name)) == 0) {
		for (i = 0; i < ARRAY_SIZE(open_c_cmd); i++) {
			if (0 == i) {
				memcpy(open_c_cmd_data,  core_mp->open_c_spec.cap1_dac_cmd,
				       sizeof(open_c_cmd_data));

			} else {
				memcpy(open_c_cmd_data,  core_mp->open_c_spec.cap1_raw_cmd,
				       sizeof(open_c_cmd_data));
			}

			j_len = 0;
			memset(str, 0, sizeof(str));

			for (j = 0; j < ARRAY_SIZE(open_c_cmd_data);) {
				j_len += snprintf(str + j_len, sizeof(str) - j_len, "0x%x,",
						  open_c_cmd_data[j++]);
			}

			tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
					    "%s = ,%s\n", open_c_cmd[i], str);
		}

	} else {
		for (j = 0; j < ARRAY_SIZE(tItems[index].p5v_cmd);) {
			j_len += snprintf(str + j_len, sizeof(str) - j_len, "0x%x,",
					  tItems[index].p5v_cmd[j++]);
		}

		tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
				    "CDC command = ,%s\n", str);
	}

	*csv_len = tmp_len;
}

static void mp_compare_cdc_show_result(void *chip_data, int index, s32 *tmp,
				       char *csv,
				       int *csv_len, int type, s32 *max_ts,
				       s32 *min_ts, const char *desp)
{
	int x, y, tmp_len = *csv_len;
	int mp_result = MP_PASS;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct core_mp_test_data *core_mp = NULL;

	core_mp = chip_info->core_mp;

	if (ERR_ALLOC_MEM(tmp)) {
		TPD_INFO("The data of test item is null (%p)\n", tmp);
		mp_result = MP_FAIL;
		goto out;
	}

	/* print X raw only */
	for (x = 0; x < core_mp->xch_len; x++) {
		if (x == 0) {
			DUMP("\n %s ", desp);
			tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
					    "\n       %s ,", desp);
		}

		DUMP("  X_%d    ,", (x + 1));
		tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
				    "     X_%d  ,", (x + 1));
	}

	DUMP("\n");
	tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len, "\n");

	for (y = 0; y < core_mp->ych_len; y++) {
		DUMP("  Y_%d    ,", (y + 1));
		tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
				    "     Y_%d  ,", (y + 1));

		for (x = 0; x < core_mp->xch_len; x++) {
			int shift = y * core_mp->xch_len + x;

			/* In Short teset, we only identify if its value is low than min threshold. */
			if (tItems[index].catalog == SHORT_TEST) {
				if (tmp[shift] < min_ts[shift]) {
					DUMP(" #%7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
							    "#%7d,", tmp[shift]);
					mp_result = MP_FAIL;

				} else {
					DUMP(" %7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
							    " %7d, ", tmp[shift]);
				}

				continue;
			}

			if ((tmp[shift] <= max_ts[shift] && tmp[shift] >= min_ts[shift])
					|| (type != TYPE_JUGE)) {
				if ((tmp[shift] == INT_MAX || tmp[shift] == INT_MIN)
						&& (type == TYPE_BENCHMARK)) {
					DUMP("%s", "BYPASS,");
					tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
							    "BYPASS,");

				} else {
					DUMP(" %7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
							    " %7d, ", tmp[shift]);
				}

			} else {
				if (tmp[shift] > max_ts[shift]) {
					DUMP(" *%7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
							    "*%7d,", tmp[shift]);

				} else {
					DUMP(" #%7d ", tmp[shift]);
					tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
							    "#%7d,", tmp[shift]);
				}

				mp_result = MP_FAIL;
			}
		}

		DUMP("\n");
		tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
				    "\n");
	}

out:

	if (type == TYPE_JUGE) {
		if (mp_result == MP_PASS) {
			pr_info("\n Result : PASS\n");
			tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
					    "Result : PASS\n");

		} else {
			pr_info("\n Result : FAIL\n");
			tmp_len += snprintf(csv + tmp_len, CSV_FILE_SIZE - tmp_len,
					    "Result : FAIL\n");
		}
	}

	*csv_len = tmp_len;
}

void allnode_open_cdc_result(void *chip_data, int index, int *buf, int *dac,
			     int *raw)
{
	int i;
	char *name = tItems[index].name;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct core_mp_test_data *core_mp = NULL;

	core_mp = chip_info->core_mp;

	if (strncmp(name, "open test_c", strlen(name)) == 0) {
		for (i = 0; i < chip_info->core_mp->frame_len; i++)
			buf[i] = chip_info->chip->open_c_formula(dac[i], raw[i],
					core_mp->open_c_spec.tvch - core_mp->open_c_spec.tvcl,
					core_mp->open_c_spec.gain);
	}
}

static int codeToOhm(void *chip_data, s32 Code, u16 *v_tdf, u16 *h_tdf)
{
	int dou_tdf1 = 0;
	int dou_tdf2 = 0;
	int dou_tvch = 24;
	int dou_tvcl = 8;
	int dou_cint = 7;
	int dou_variation = 64;
	int dou_rinternal = 930;
	s32 temp = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (chip_info->core_mp->is_longv) {
		dou_tdf1 = *v_tdf;
		dou_tdf2 = *(v_tdf + 1);

	} else {
		dou_tdf1 = *h_tdf;
		dou_tdf2 = *(h_tdf + 1);
	}

	if (Code == 0) {
		TPD_INFO("code is invalid\n");

	} else {
		temp = ((dou_tvch - dou_tvcl) * dou_variation * (dou_tdf1 - dou_tdf2) * (1 << 12) /
			(9 * Code * dou_cint)) * 100;
		temp = (temp - dou_rinternal) / 1000;
	}

	/* Unit = M Ohm */
	return temp;
}

static int short_test(void *chip_data, int index, int frame_index)
{
	int j = 0, ret = 0;
	u16 v_tdf[2] = {0};
	u16 h_tdf[2] = {0};
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct core_mp_test_data *core_mp = NULL;

	core_mp = chip_info->core_mp;

	v_tdf[0] = tItems[index].v_tdf_1;
	v_tdf[1] = tItems[index].v_tdf_2;
	h_tdf[0] = tItems[index].h_tdf_1;
	h_tdf[1] = tItems[index].h_tdf_2;

	if (core_mp->protocol_ver >= PROTOCOL_VER_540) {
		/* Calculate code to ohm and save to tItems[index].buf */
		for (j = 0; j < core_mp->frame_len; j++) {
			tItems[index].buf[frame_index * core_mp->frame_len + j] = codeToOhm(chip_info,
					core_mp->frame_buf[j], v_tdf, h_tdf);
		}

	} else {
		for (j = 0; j < core_mp->frame_len; j++) {
			tItems[index].buf[frame_index * core_mp->frame_len + j] =
				core_mp->frame_buf[j];
		}
	}

	return ret;
}


static int mp_cdc_init_cmd_common(void *chip_data, u8 *cmd, int len, int index)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct core_mp_test_data *core_mp = NULL;

	core_mp = chip_info->core_mp;

	if (core_mp->protocol_ver >= PROTOCOL_VER_540) {
		core_mp->cdc_len = 15;

		if (ARRAY_SIZE(tItems[index].p5v_cmd) >= len) {
			memcpy(cmd, tItems[index].p5v_cmd, len);
		}

		return ret;
	}

	return -1;
}

static int allnode_open_cdc_data(void *chip_data, int mode, int *buf)
{
	int i = 0, ret = 0, len = 0;
	int in_dacp = 0, in_dacn = 0;
	u8 cmd[15] = {0};
	u8 *ori = NULL;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct core_mp_test_data *core_mp = NULL;

	core_mp = chip_info->core_mp;

	/* Multipling by 2 is due to the 16 bit in each node */
	len = (core_mp->xch_len * core_mp->ych_len * 2) + 2;

	TPD_DEBUG("Read X/Y Channel length = %d, mode = %d\n", len, mode);

	if (len <= 2) {
		TPD_INFO("Length is invalid\n");
		ret = -1;
		goto out;
	}

	if (4 == mode) {
		memcpy(cmd, core_mp->open_c_spec.cap1_dac_cmd, sizeof(cmd));

	} else if (5 == mode) {
		memcpy(cmd, core_mp->open_c_spec.cap1_raw_cmd, sizeof(cmd));
	}

	ilitek_dump_data(cmd, 8, sizeof(cmd), 0, "Open SP command");

	/*atomic_set(&idev->mp_int_check, ENABLE);*/
	chip_info->irq_wake_up_state = false;

	ret = chip_info->write(chip_info, cmd, core_mp->cdc_len);

	if (ret < 0) {
		TPD_INFO("Write CDC command failed\n");
		goto out;
	}

	/* Check busy */
	if (core_mp->busy_cdc == POLL_CHECK) {
		ret = ilitek_tddi_ic_check_busy(chip_info, 50, 50);

	} else if (core_mp->busy_cdc == INT_CHECK) {
		ret = ilitek_check_wake_up_state(MP_TMEOUT);

	} else if (core_mp->busy_cdc == DELAY_CHECK) {
		mdelay(600);
	}

	if (ret < 0) {
		goto out;
	}

	/* Prepare to get cdc data */
	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = P5_X_GET_CDC_DATA;

	ret = chip_info->write(chip_info, cmd, 2);

	if (ret < 0) {
		TPD_INFO("Write (0x%x, 0x%x) error\n", cmd[0], cmd[1]);
		goto out;
	}

	msleep(5);

	ret = chip_info->write(chip_info, &cmd[1], 1);

	if (ret < 0) {
		TPD_INFO("Write (0x%x) error\n", cmd[1]);
		goto out;
	}

	msleep(5);

	/* Allocate a buffer for the original */
	ori = kcalloc(len, sizeof(u8), GFP_KERNEL);

	if (ERR_ALLOC_MEM(ori)) {
		TPD_INFO("Failed to allocate ori, (%ld)\n", PTR_ERR(ori));
		goto out;
	}

	/* Get original frame(cdc) data */
	ret = chip_info->read(chip_info, ori, len);

	if (ret < 0) {
		TPD_INFO("Read cdc data error, len = %d\n", len);
		goto out;
	}

	ilitek_dump_data(ori, 8, len, 0, "Open SP CDC original");

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

			if (mode == 0) {
				buf[i] = (in_dacp + in_dacn) / 2;

			} else {
				buf[i] = in_dacp + in_dacn;
			}

		} else {
			/* H byte + L byte */
			s32 tmp32 = (ori[(2 * i) + 1] << 8) + ori[(1 + (2 * i)) + 1];

			if ((tmp32 & 0x8000) == 0x8000) {
				buf[i] = tmp32 - 65536;

			} else {
				buf[i] = tmp32;
			}
		}
	}

	ilitek_dump_data(buf, 10, core_mp->frame_len,  core_mp->xch_len,
			 "Open SP CDC combined");
out:
	tp_kfree((void **)&ori);
	return ret;
}

static int allnode_mutual_cdc_data(void *chip_data, int index)
{
	int i, ret = 0, len = 0;
	int in_dacp = 0, in_dacn = 0;
	u8 cmd[15] = {0};
	u8 *ori = NULL;

	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct core_mp_test_data *core_mp = NULL;

	core_mp = chip_info->core_mp;

	/* Multipling by 2 is due to the 16 bit in each node */
	len = (core_mp->xch_len * core_mp->ych_len * 2) + 2;

	TPD_DEBUG("Read X/Y Channel length = %d\n", len);

	if (len <= 2) {
		TPD_INFO("Length is invalid\n");
		ret = -1;
		goto out;
	}

	memset(cmd, 0xFF, sizeof(cmd));

	/* CDC init */
	ret = mp_cdc_init_cmd_common(chip_info, cmd, sizeof(cmd), index);

	if (ret < 0) {
		TPD_INFO("Failed to get cdc command\n");
		goto out;
	}

	TPD_DEBUG("cmd:%*ph\n", sizeof(cmd), cmd);

	ilitek_dump_data(cmd, 8, core_mp->cdc_len, 0, "Mutual CDC command");

	chip_info->irq_wake_up_state = false;

	ret = chip_info->write(chip_info, cmd, core_mp->cdc_len);

	if (ret < 0) {
		TPD_INFO("Write CDC command failed\n");
		goto out;
	}

	/* Check busy */
	if (core_mp->busy_cdc == POLL_CHECK) {
		ret = ilitek_tddi_ic_check_busy(chip_info, 50, 50);

	} else if (core_mp->busy_cdc == INT_CHECK) {
		ret = ilitek_check_wake_up_state(MP_TMEOUT);

	} else if (core_mp->busy_cdc == DELAY_CHECK) {
		mdelay(600);
	}

	if (ret < 0) {
		goto out;
	}

	/* Prepare to get cdc data */
	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = P5_X_GET_CDC_DATA;

	ret = chip_info->write(chip_info, cmd, 2);

	if (ret < 0) {
		TPD_INFO("Write (0x%x, 0x%x) error\n", cmd[0], cmd[1]);
		goto out;
	}

	msleep(5);

	ret = chip_info->write(chip_info, &cmd[1], 1);

	if (ret < 0) {
		TPD_INFO("Write (0x%x) error\n", cmd[1]);
		goto out;
	}

	msleep(5);

	/* Allocate a buffer for the original */
	ori = kcalloc(len, sizeof(u8), GFP_KERNEL);

	if (ERR_ALLOC_MEM(ori)) {
		TPD_INFO("Failed to allocate ori, (%ld)\n", PTR_ERR(ori));
		goto out;
	}

	/* Get original frame(cdc) data */
	ret = chip_info->read(chip_info, ori, len);

	if (ret < 0) {
		TPD_INFO("Read cdc data error, len = %d\n", len);
		goto out;
	}

	ilitek_dump_data(ori, 8, len, 0, "Mutual CDC original");

	if (core_mp->frame_buf == NULL) {
		core_mp->frame_buf = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

		if (ERR_ALLOC_MEM(core_mp->frame_buf)) {
			TPD_INFO("Failed to allocate FrameBuffer mem (%ld)\n",
				 PTR_ERR(core_mp->frame_buf));
			goto out;
		}

	} else {
		memset(core_mp->frame_buf, 0x0, core_mp->frame_len);
	}

	/* Convert original data to the physical one in each node */
	for (i = 0; i < core_mp->frame_len; i++) {
		if (tItems[index].cmd == CMD_MUTUAL_DAC) {
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

			core_mp->frame_buf[i] = (in_dacp + in_dacn) / 2;

		} else {
			/* H byte + L byte */
			s32 tmp = (ori[(2 * i) + 1] << 8) + ori[(1 + (2 * i)) + 1];

			if ((tmp & 0x8000) == 0x8000) {
				core_mp->frame_buf[i] = tmp - 65536;

			} else {
				core_mp->frame_buf[i] = tmp;
			}

			if (strncmp(tItems[index].name, "mutual_no_bk", strlen("mutual_no_bk")) == 0 ||
					strncmp(tItems[index].name, "mutual_no_bk_lcm_off",
						strlen("mutual_no_bk_lcm_off")) == 0) {
				core_mp->frame_buf[i] -= core_mp->no_bk_shift;
			}
		}
	}

	ilitek_dump_data(core_mp->frame_buf, 32, core_mp->frame_len,   core_mp->xch_len,
			 "Mutual CDC combined");

out:
	tp_kfree((void **)&ori);
	return ret;
}

static void compare_MaxMin_result(void *chip_data, int index, s32 *data)
{
	int x, y;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	for (y = 0; y < chip_info->core_mp->ych_len; y++) {
		for (x = 0; x < chip_info->core_mp->xch_len; x++) {
			int shift = y * chip_info->core_mp->xch_len;

			if (tItems[index].max_buf[shift + x] < data[shift + x]) {
				tItems[index].max_buf[shift + x] = data[shift + x];
			}

			if (tItems[index].min_buf[shift + x] > data[shift + x]) {
				tItems[index].min_buf[shift + x] = data[shift + x];
			}
		}
	}
}

static int create_mp_test_frame_buffer(void *chip_data, int index,
				       int frame_count)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_DEBUG("Create MP frame buffers (index = %d), count = %d\n",
		  index, frame_count);

	if (tItems[index].buf == NULL) {
		tItems[index].buf = vmalloc(frame_count * chip_info->core_mp->frame_len *
					    sizeof(s32));

		if (ERR_ALLOC_MEM(tItems[index].buf)) {
			TPD_INFO("Failed to allocate buf mem\n");
			tp_vfree((void **)&tItems[index].buf);
			return -ENOMEM;
		}
	}

	if (tItems[index].result_buf == NULL) {
		tItems[index].result_buf = kcalloc(chip_info->core_mp->frame_len, sizeof(s32),
						   GFP_KERNEL);

		if (ERR_ALLOC_MEM(tItems[index].result_buf)) {
			TPD_INFO("Failed to allocate result_buf mem\n");
			tp_kfree((void **)&tItems[index].result_buf);
			return -ENOMEM;
		}
	}

	if (tItems[index].max_buf == NULL) {
		tItems[index].max_buf = kcalloc(chip_info->core_mp->frame_len, sizeof(s32),
						GFP_KERNEL);

		if (ERR_ALLOC_MEM(tItems[index].max_buf)) {
			TPD_INFO("Failed to allocate max_buf mem\n");
			tp_kfree((void **)&tItems[index].max_buf);
			return -ENOMEM;
		}
	}

	if (tItems[index].min_buf == NULL) {
		tItems[index].min_buf = kcalloc(chip_info->core_mp->frame_len, sizeof(s32),
						GFP_KERNEL);

		if (ERR_ALLOC_MEM(tItems[index].min_buf)) {
			TPD_INFO("Failed to allocate min_buf mem\n");
			tp_kfree((void **)&tItems[index].min_buf);
			return -ENOMEM;
		}
	}

	return 0;
}

static int mutual_test(void *chip_data, int index)
{
	int i = 0, j = 0, x = 0, y = 0, ret = 0, get_frame_cont = 1;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_DEBUG("index = %d, name = %s, CMD = 0x%x, Frame Count = %d\n",
		  index, tItems[index].name, tItems[index].cmd, tItems[index].frame_count);

	/*
	 * We assume that users who are calling the test forget to config frame count
	 * as 1, so we just help them to set it up.
	 */
	if (tItems[index].frame_count <= 0) {
		TPD_INFO("Frame count is zero, which is at least set as 1\n");
		tItems[index].frame_count = 1;
	}

	ret = create_mp_test_frame_buffer(chip_info, index, tItems[index].frame_count);

	if (ret < 0) {
		goto out;
	}

	/* Init Max/Min buffer */
	for (y = 0; y < chip_info->core_mp->ych_len; y++) {
		for (x = 0; x < chip_info->core_mp->xch_len; x++) {
			tItems[index].max_buf[y * chip_info->core_mp->xch_len + x] = INT_MIN;
			tItems[index].min_buf[y * chip_info->core_mp->xch_len + x] = INT_MAX;
		}
	}

	if (tItems[index].catalog != PEAK_TO_PEAK_TEST) {
		get_frame_cont = tItems[index].frame_count;
	}

	for (i = 0; i < get_frame_cont; i++) {
		ret = allnode_mutual_cdc_data(chip_info, index);

		if (ret < 0) {
			TPD_INFO("Failed to initialise CDC data, %d\n", ret);
			goto out;
		}

		switch (tItems[index].catalog) {
		case OPEN_TEST:
			run_open_test(chip_info, index);
			break;

		case SHORT_TEST:
			short_test(chip_info, index, i);
			break;

		default:
			for (j = 0; j < chip_info->core_mp->frame_len; j++) {
				tItems[index].buf[i * chip_info->core_mp->frame_len + j] =
					chip_info->core_mp->frame_buf[j];
			}

			break;
		}

		compare_MaxMin_result(chip_info, index,
				      &tItems[index].buf[i * chip_info->core_mp->frame_len]);
	}

out:
	return ret;
}

static int open_test_cap(void *chip_data, int index)
{
	struct mp_test_open_c open[tItems[index].frame_count];
	int i = 0, x = 0, y = 0, ret = 0, addr = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct core_mp_test_data *core_mp = NULL;

	core_mp = chip_info->core_mp;

	if (tItems[index].frame_count <= 0) {
		TPD_INFO("Frame count is zero, which is at least set as 1\n");
		tItems[index].frame_count = 1;
	}

	ret = create_mp_test_frame_buffer(chip_info, index, tItems[index].frame_count);

	if (ret < 0) {
		goto out;
	}

	if (core_mp->cap_dac == NULL) {
		core_mp->cap_dac = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

		if (ERR_ALLOC_MEM(core_mp->cap_dac)) {
			TPD_INFO("Failed to allocate cap_dac buffer\n");
			return -ENOMEM;
		}

	} else {
		memset(core_mp->cap_dac, 0x0, core_mp->frame_len);
	}

	if (core_mp->cap_raw == NULL) {
		core_mp->cap_raw = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);

		if (ERR_ALLOC_MEM(core_mp->cap_raw)) {
			TPD_INFO("Failed to allocate cap_raw buffer\n");
			tp_kfree((void **)&core_mp->cap_dac);
			return -ENOMEM;
		}

	} else {
		memset(core_mp->cap_raw, 0x0, core_mp->frame_len);
	}

	/* Init Max/Min buffer */
	for (y = 0; y < core_mp->ych_len; y++) {
		for (x = 0; x < core_mp->xch_len; x++) {
			tItems[index].max_buf[y * core_mp->xch_len + x] = INT_MIN;
			tItems[index].min_buf[y * core_mp->xch_len + x] = INT_MAX;
		}
	}

	if (tItems[index].spec_option == BENCHMARK) {
		if (1) {
			dump_benchmark_data(chip_info, tItems[index].bench_mark_max,
					    tItems[index].bench_mark_min);
		}
	}

	TPD_DEBUG("open_test_c: frame_cont = %d, gain = %d, tvch = %d, tvcl = %d\n",
		  tItems[index].frame_count, core_mp->open_c_spec.gain,
		  core_mp->open_c_spec.tvch,
		  core_mp->open_c_spec.tvcl);

	for (i = 0; i < tItems[index].frame_count; i++) {
		open[i].cap_dac = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);
		open[i].cap_raw = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);
		open[i].dcl_cap = kcalloc(core_mp->frame_len, sizeof(s32), GFP_KERNEL);
	}

	for (i = 0; i < tItems[index].frame_count; i++) {
		ret = allnode_open_cdc_data(chip_info, 4, open[i].cap_dac);

		if (ret < 0) {
			TPD_INFO("Failed to get Open CAP DAC data, %d\n", ret);
			goto out;
		}

		ret = allnode_open_cdc_data(chip_info, 5, open[i].cap_raw);

		if (ret < 0) {
			TPD_INFO("Failed to get Open CAP RAW data, %d\n", ret);
			goto out;
		}

		allnode_open_cdc_result(chip_info, index, open[i].dcl_cap, open[i].cap_dac,
					open[i].cap_raw);

		/* record fist frame for debug */
		if (i == 0) {
			tp_memcpy(core_mp->cap_dac, core_mp->frame_len * sizeof(s32),
				  open[i].cap_dac, core_mp->frame_len * sizeof(s32),
				  core_mp->frame_len * sizeof(s32));
			tp_memcpy(core_mp->cap_raw, core_mp->frame_len * sizeof(s32),
				  open[i].cap_raw, core_mp->frame_len * sizeof(s32),
				  core_mp->frame_len * sizeof(s32));
		}

		ilitek_dump_data(open[i].dcl_cap, 10, core_mp->frame_len, core_mp->xch_len,
				 "DCL_Cap");

		addr = 0;

		for (y = 0; y < core_mp->ych_len; y++) {
			for (x = 0; x < core_mp->xch_len; x++) {
				tItems[index].buf[(i * core_mp->frame_len) + addr] = open[i].dcl_cap[addr];
				addr++;
			}
		}

		compare_MaxMin_result(chip_info, index,
				      &tItems[index].buf[i * core_mp->frame_len]);
	}

out:

	for (i = 0; i < tItems[index].frame_count; i++) {
		tp_kfree((void **)&open[i].cap_dac);
		tp_kfree((void **)&open[i].cap_raw);
		tp_kfree((void **)&open[i].dcl_cap);
	}

	return ret;
}

static int mp_test_data_sort_average(void *chip_data, s32 *oringin_data,
				     int index, s32 *avg_result)
{
	int i, j, k, x, y, len = 5;
	s32 u32temp;
	int u32up_frame, u32down_frame;
	s32 *u32sum_raw_data;
	s32 *u32data_buff;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (tItems[index].frame_count <= 1) {
		return 0;
	}


	if (ERR_ALLOC_MEM(oringin_data)) {
		TPD_INFO("Input wrong address\n");
		return -ENOMEM;
	}

	u32data_buff = kcalloc(chip_info->core_mp->frame_len *
			       tItems[index].frame_count, sizeof(s32), GFP_KERNEL);
	u32sum_raw_data = kcalloc(chip_info->core_mp->frame_len, sizeof(s32),
				  GFP_KERNEL);

	if (ERR_ALLOC_MEM(u32sum_raw_data) || (ERR_ALLOC_MEM(u32data_buff))) {
		TPD_INFO("Failed to allocate u32sum_raw_data FRAME buffer\n");
		return -ENOMEM;
	}

	for (i = 0; i < chip_info->core_mp->frame_len * tItems[index].frame_count;
			i++) {
		u32data_buff[i] = oringin_data[i];
	}

	u32up_frame = tItems[index].frame_count * tItems[index].highest_percentage /
		      100;
	u32down_frame = tItems[index].frame_count * tItems[index].lowest_percentage /
			100;
	TPD_DEBUG("Up=%d, Down=%d -%s\n", u32up_frame, u32down_frame,
		  tItems[index].desp);

	if (ipio_debug_level) {
		pr_cont("\n[Show Original frist%d and last%d node data]\n", len, len);

		for (i = 0; i < chip_info->core_mp->frame_len; i++) {
			for (j = 0; j < tItems[index].frame_count; j++) {
				if ((i < len) || (i >= (chip_info->core_mp->frame_len - len))) {
					pr_cont("%d,", u32data_buff[j * chip_info->core_mp->frame_len + i]);
				}
			}

			if ((i < len) || (i >= (chip_info->core_mp->frame_len - len))) {
				pr_cont("\n");
			}
		}
	}

	for (i = 0; i < chip_info->core_mp->frame_len; i++) {
		for (j = 0; j < tItems[index].frame_count - 1; j++) {
			for (k = 0; k < (tItems[index].frame_count - 1 - j); k++) {
				x = i + k * chip_info->core_mp->frame_len;
				y = i + (k + 1) * chip_info->core_mp->frame_len;

				if (*(u32data_buff + x) > *(u32data_buff + y)) {
					u32temp = *(u32data_buff + x);
					*(u32data_buff + x) = *(u32data_buff + y);
					*(u32data_buff + y) = u32temp;
				}
			}
		}
	}

	if (ipio_debug_level) {
		pr_cont("\n[After sorting frist%d and last%d node data]\n", len, len);

		for (i = 0; i < chip_info->core_mp->frame_len; i++) {
			for (j = u32down_frame; j < tItems[index].frame_count - u32up_frame; j++) {
				if ((i < len) || (i >= (chip_info->core_mp->frame_len - len))) {
					pr_cont("%d,", u32data_buff[i + j * chip_info->core_mp->frame_len]);
				}
			}

			if ((i < len) || (i >= (chip_info->core_mp->frame_len - len))) {
				pr_cont("\n");
			}
		}
	}

	for (i = 0; i < chip_info->core_mp->frame_len; i++) {
		u32sum_raw_data[i] = 0;

		for (j = u32down_frame; j < tItems[index].frame_count - u32up_frame; j++) {
			u32sum_raw_data[i] += u32data_buff[i + j * chip_info->core_mp->frame_len];
		}

		avg_result[i] = u32sum_raw_data[i] / (tItems[index].frame_count - u32down_frame
						      - u32up_frame);
	}

	if (ipio_debug_level) {
		pr_cont("\n[Average result frist%d and last%d node data]\n", len, len);

		for (i = 0; i < chip_info->core_mp->frame_len; i++) {
			if ((i < len) || (i >= (chip_info->core_mp->frame_len - len))) {
				pr_cont("%d,", avg_result[i]);
			}
		}

		if ((i < len) || (i >= (chip_info->core_mp->frame_len - len))) {
			pr_cont("\n");
		}
	}

	tp_kfree((void **)&u32data_buff);
	tp_kfree((void **)&u32sum_raw_data);
	return 0;
}

static void mp_compare_cdc_result(void *chip_data, int index, s32 *tmp,
				  s32 *max_ts, s32 *min_ts, int *result)
{
	int i;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (ERR_ALLOC_MEM(tmp)) {
		TPD_INFO("The data of test item is null (%p)\n", tmp);
		*result = MP_FAIL;
		return;
	}

	if (tItems[index].catalog == SHORT_TEST) {
		for (i = 0; i < chip_info->core_mp->frame_len; i++) {
			if (tmp[i] < min_ts[i]) {
				*result = MP_FAIL;
				return;
			}
		}

	} else {
		for (i = 0; i < chip_info->core_mp->frame_len; i++) {
			if (tmp[i] > max_ts[i] || tmp[i] < min_ts[i]) {
				*result = MP_FAIL;
				return;
			}
		}
	}
}

static int mp_comp_result_before_retry(void *chip_data, int index)
{
	int i, test_result = MP_PASS;
	s32 *max_threshold = NULL, *min_threshold = NULL;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	max_threshold = kcalloc(chip_info->core_mp->frame_len, sizeof(s32), GFP_KERNEL);

	if (ERR_ALLOC_MEM(max_threshold)) {
		TPD_INFO("Failed to allocate threshold FRAME buffer\n");
		tp_kfree((void **)&max_threshold);
		test_result = MP_FAIL;
		goto fail_alloc;
	}

	min_threshold = kcalloc(chip_info->core_mp->frame_len, sizeof(s32), GFP_KERNEL);

	if (ERR_ALLOC_MEM(min_threshold)) {
		TPD_INFO("Failed to allocate threshold FRAME buffer\n");
		tp_kfree((void **)&min_threshold);
		test_result = MP_FAIL;
		goto fail_alloc;
	}

	/* Show test result as below */
	if (ERR_ALLOC_MEM(tItems[index].buf) || ERR_ALLOC_MEM(tItems[index].max_buf) ||
			ERR_ALLOC_MEM(tItems[index].min_buf)
			|| ERR_ALLOC_MEM(tItems[index].result_buf)) {
		TPD_INFO("This test item (%s) has no data inside its buffer\n",
			 tItems[index].desp);
		test_result = MP_FAIL;
		goto out;
	}

	if (tItems[index].spec_option == BENCHMARK) {
		for (i = 0; i < chip_info->core_mp->frame_len; i++) {
			max_threshold[i] = tItems[index].bench_mark_max[i];
			min_threshold[i] = tItems[index].bench_mark_min[i];
		}

	} else {
		for (i = 0; i < chip_info->core_mp->frame_len; i++) {
			max_threshold[i] = tItems[index].max;
			min_threshold[i] = tItems[index].min;
		}
	}

	/* general result */
	if (tItems[index].trimmed_mean && tItems[index].catalog != PEAK_TO_PEAK_TEST) {
		mp_test_data_sort_average(chip_info, tItems[index].buf, index,
					  tItems[index].result_buf);
		mp_compare_cdc_result(chip_info, index, tItems[index].result_buf, max_threshold,
				      min_threshold, &test_result);

	} else {
		mp_compare_cdc_result(chip_info, index, tItems[index].buf, max_threshold,
				      min_threshold, &test_result);
		mp_compare_cdc_result(chip_info, index, tItems[index].buf, max_threshold,
				      min_threshold, &test_result);
	}

out:
	tp_kfree((void **)&max_threshold);
	tp_kfree((void **)&min_threshold);

fail_alloc:
	tItems[index].item_result = test_result;
	return test_result;
}

static void mp_do_retry(void *chip_data, int index, int count)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (count == 0) {
		TPD_INFO("Finish retry action\n");
		return;
	}

	TPD_INFO("retry = %d, item = %s\n", count, tItems[index].desp);

	if (tItems[index].do_test) {
		tItems[index].do_test(chip_info, index);
	}

	if (mp_comp_result_before_retry(chip_info, index) == MP_FAIL) {
		return mp_do_retry(chip_info, index, count - 1);
	}
}

static void mp_show_result(void *chip_data, struct auto_testdata *testdata)
{
	int  i, j, csv_len = 0, pass_item_count = 0, line_count = 0, get_frame_cont = 1;
	s32 *max_threshold = NULL, *min_threshold = NULL;
	char *csv = NULL;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct core_mp_test_data *core_mp = NULL;

	core_mp = chip_info->core_mp;

	if (!testdata) {
		TPD_INFO("%s:testdata is null \n", __func__);
	}

	csv = vmalloc(CSV_FILE_SIZE);

	if (ERR_ALLOC_MEM(csv)) {
		TPD_INFO("Failed to allocate CSV mem\n");
		goto fail_open;
	}

	max_threshold = kcalloc(chip_info->core_mp->frame_len, sizeof(s32), GFP_KERNEL);
	min_threshold = kcalloc(chip_info->core_mp->frame_len, sizeof(s32), GFP_KERNEL);

	if (ERR_ALLOC_MEM(max_threshold) || ERR_ALLOC_MEM(min_threshold)) {
		TPD_INFO("Failed to allocate threshold FRAME buffer\n");
		goto fail_open;
	}

	for (i = 0; i < ARRAY_SIZE(tItems); i++) {
		if (tItems[i].run) {
			if (tItems[i].item_result == MP_FAIL) {
				pass_item_count = 0;
				break;
			}

			pass_item_count++;
		}
	}

	if (pass_item_count == 0) {
		chip_info->core_mp->final_result = MP_FAIL;

	} else {
		chip_info->core_mp->final_result = MP_PASS;
	}

	mp_print_csv_header(chip_info, csv, &csv_len, &line_count);

	for (i = 0; i < ARRAY_SIZE(tItems); i++) {
		get_frame_cont = 1;

		if (tItems[i].run != 1) {
			continue;
		}

		if (tItems[i].item_result == MP_PASS) {
			pr_info("\n\n[%s],OK \n", tItems[i].desp);
			csv_len += snprintf(csv + csv_len, CSV_FILE_SIZE - csv_len,
					    "\n\n[%s],OK\n", tItems[i].desp);

		} else {
			pr_info("\n\n[%s],NG \n", tItems[i].desp);
			csv_len += snprintf(csv + csv_len, CSV_FILE_SIZE - csv_len,
					    "\n\n[%s],NG\n", tItems[i].desp);
		}

		mp_print_csv_cdc_cmd(chip_info, csv, &csv_len, i);

		pr_info("Frame count = %d\n", tItems[i].frame_count);
		csv_len += snprintf(csv + csv_len, CSV_FILE_SIZE - csv_len,
				    "Frame count = %d\n", tItems[i].frame_count);

		if (tItems[i].trimmed_mean && tItems[i].catalog != PEAK_TO_PEAK_TEST) {
			pr_info("lowest percentage = %d\n", tItems[i].lowest_percentage);
			csv_len += snprintf(csv + csv_len, CSV_FILE_SIZE - csv_len,
					    "lowest percentage = %d\n", tItems[i].lowest_percentage);

			pr_info("highest percentage = %d\n", tItems[i].highest_percentage);
			csv_len += snprintf(csv + csv_len, CSV_FILE_SIZE - csv_len,
					    "highest percentage = %d\n", tItems[i].highest_percentage);
		}

		/* Show result of benchmark max and min */
		if (tItems[i].spec_option == BENCHMARK) {
			for (j = 0; j < chip_info->core_mp->frame_len; j++) {
				max_threshold[j] = tItems[i].bench_mark_max[j];
				min_threshold[j] = tItems[i].bench_mark_min[j];
			}

			mp_compare_cdc_show_result(chip_info, i, tItems[i].bench_mark_max, csv,
						   &csv_len, TYPE_BENCHMARK, max_threshold, min_threshold, "Max_Bench");
			mp_compare_cdc_show_result(chip_info, i, tItems[i].bench_mark_min, csv,
						   &csv_len, TYPE_BENCHMARK, max_threshold, min_threshold, "Min_Bench");

		} else {
			for (j = 0; j < chip_info->core_mp->frame_len; j++) {
				max_threshold[j] = tItems[i].max;
				min_threshold[j] = tItems[i].min;
			}

			pr_info("Max = %d\n", tItems[i].max);
			csv_len += snprintf(csv + csv_len, CSV_FILE_SIZE - csv_len,
					    "Max = %d\n", tItems[i].max);

			pr_info("Min = %d\n", tItems[i].min);
			csv_len += snprintf(csv + csv_len, CSV_FILE_SIZE - csv_len,
					    "Min = %d\n", tItems[i].min);
		}

		if (strcmp(tItems[i].name, "open test_c") == 0) {
			mp_compare_cdc_show_result(chip_info, i, core_mp->cap_dac, csv, &csv_len,
						   TYPE_NO_JUGE, max_threshold, min_threshold, "CAP_DAC");
			mp_compare_cdc_show_result(chip_info, i, core_mp->cap_raw, csv, &csv_len,
						   TYPE_NO_JUGE, max_threshold, min_threshold, "CAP_RAW");
		}

		if (ERR_ALLOC_MEM(tItems[i].buf) || ERR_ALLOC_MEM(tItems[i].max_buf) ||
				ERR_ALLOC_MEM(tItems[i].min_buf)) {
			TPD_INFO("This test item (%s) has no data inside its buffer\n", tItems[i].desp);
			continue;
		}

		/* Show test result as below */
		/* general result */
		if (tItems[i].trimmed_mean && tItems[i].catalog != PEAK_TO_PEAK_TEST) {
			mp_compare_cdc_show_result(chip_info, i, tItems[i].result_buf, csv, &csv_len,
						   TYPE_JUGE, max_threshold, min_threshold, "Mean result");

		} else {
			mp_compare_cdc_show_result(chip_info, i, tItems[i].max_buf, csv, &csv_len,
						   TYPE_JUGE, max_threshold, min_threshold, "Max Hold");
			mp_compare_cdc_show_result(chip_info, i, tItems[i].min_buf, csv, &csv_len,
						   TYPE_JUGE, max_threshold, min_threshold, "Min Hold");
		}

		if (tItems[i].catalog != PEAK_TO_PEAK_TEST) {
			get_frame_cont = tItems[i].frame_count;
		}

		/* result of each frame */
		for (j = 0; j < get_frame_cont; j++) {
			char frame_name[128] = {0};
			snprintf(frame_name, 128, "Frame %d", (j + 1));
			mp_compare_cdc_show_result(chip_info, i,
						   &tItems[i].buf[(j * chip_info->core_mp->frame_len)], csv, &csv_len,
						   TYPE_NO_JUGE, max_threshold, min_threshold, frame_name);
		}
	}

	TPD_INFO("Open CSV succeed, its length = %d\n ", csv_len);

	if (csv_len >= CSV_FILE_SIZE) {
		TPD_INFO("The length saved to CSV is too long !\n");
		goto fail_open;
	}

	tp_test_write(testdata->fp, testdata->length, csv, csv_len, testdata->pos);
	TPD_INFO("Writing Data into CSV succeed\n");

fail_open:
	tp_vfree((void **)&csv);
	tp_kfree((void **)&max_threshold);
	tp_kfree((void **)&min_threshold);
}

static void ilitek_tddi_mp_init_item(void *chip_data)
{
	int i = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct core_mp_test_data *core_mp = NULL;

	memset(chip_info->core_mp, 0, sizeof(struct core_mp_test_data));
	core_mp = chip_info->core_mp;

	core_mp->chip_pid = chip_info->chip->pid;
	core_mp->fw_ver = chip_info->chip->fw_ver;
	core_mp->core_ver = chip_info->chip->core_ver;
	core_mp->protocol_ver = chip_info->protocol->ver;
	core_mp->cdc_len = chip_info->protocol->cdc_len;
	core_mp->no_bk_shift = chip_info->chip->no_bk_shift;
	core_mp->xch_len = chip_info->xch_num;/*tx*/
	core_mp->ych_len = chip_info->ych_num;/*rx*/
	core_mp->frame_len = core_mp->xch_len * core_mp->ych_len;
	core_mp->stx_len = 0;
	core_mp->srx_len = 0;
	core_mp->key_len = 0;
	core_mp->st_len = 0;
	core_mp->tdf = 240;
	core_mp->busy_cdc = INT_CHECK;
	core_mp->retry = false;
	core_mp->final_result = MP_FAIL;

	TPD_INFO("CHIP = 0x%x\n", core_mp->chip_pid);
	TPD_INFO("Firmware version = %x\n", core_mp->fw_ver);
	TPD_INFO("Protocol version = %x\n", core_mp->protocol_ver);
	TPD_INFO("Read CDC Length = %d\n", core_mp->cdc_len);
	TPD_INFO("X length = %d, Y length = %d\n", core_mp->xch_len, core_mp->ych_len);
	TPD_INFO("Frame length = %d\n", core_mp->frame_len);
	TPD_INFO("Check busy method = %d\n", core_mp->busy_cdc);

	for (i = 0; i < MP_TEST_ITEM; i++) {
		tItems[i].spec_option = 0;
		tItems[i].type_option = 0;
		tItems[i].run = false;
		tItems[i].max = 0;
		tItems[i].max_res = MP_FAIL;
		tItems[i].item_result = MP_PASS;
		tItems[i].min = 0;
		tItems[i].min_res = MP_FAIL;
		tItems[i].frame_count = 0;
		tItems[i].trimmed_mean = 0;
		tItems[i].lowest_percentage = 0;
		tItems[i].highest_percentage = 0;
		tItems[i].v_tdf_1 = 0;
		tItems[i].v_tdf_2 = 0;
		tItems[i].h_tdf_1 = 0;
		tItems[i].h_tdf_2 = 0;
		tItems[i].result_buf = NULL;
		tItems[i].buf = NULL;
		tItems[i].max_buf = NULL;
		tItems[i].min_buf = NULL;
		tItems[i].bench_mark_max = NULL;
		tItems[i].bench_mark_min = NULL;
		tItems[i].node_type = NULL;
		memset(tItems[i].p5v_cmd, 0, sizeof(tItems[i].p5v_cmd));

		if (tItems[i].catalog == MUTUAL_TEST) {
			tItems[i].do_test = mutual_test;

		} else if (tItems[i].catalog == OPEN_TEST) {
			if (strcmp(tItems[i].name, "open test_c") == 0) {
				tItems[i].do_test = open_test_cap;

			} else {
				tItems[i].do_test = mutual_test;
			}

		} else if (tItems[i].catalog == PEAK_TO_PEAK_TEST) {
			tItems[i].do_test = mutual_test;

		} else if (tItems[i].catalog == SHORT_TEST) {
			tItems[i].do_test = mutual_test;
		}

		tItems[i].result = kmalloc(16, GFP_KERNEL);
		snprintf(tItems[i].result, 16, "%s", "FAIL");
	}

	tItems[0].cmd = CMD_MUTUAL_DAC;
	tItems[1].cmd = CMD_MUTUAL_BG;
	tItems[2].cmd = CMD_MUTUAL_SIGNAL;
	tItems[3].cmd = CMD_MUTUAL_NO_BK;
	tItems[4].cmd = CMD_MUTUAL_HAVE_BK;
	tItems[5].cmd = CMD_MUTUAL_BK_DAC;
	tItems[6].cmd = CMD_SELF_DAC;
	tItems[7].cmd = CMD_SELF_BG;
	tItems[8].cmd = CMD_SELF_SIGNAL;
	tItems[9].cmd = CMD_SELF_NO_BK;
	tItems[10].cmd = CMD_SELF_HAVE_BK;
	tItems[11].cmd = CMD_SELF_BK_DAC;
	tItems[12].cmd = CMD_KEY_DAC;
	tItems[13].cmd = CMD_KEY_BG;
	tItems[14].cmd = CMD_KEY_NO_BK;
	tItems[15].cmd = CMD_KEY_HAVE_BK;
	tItems[16].cmd = CMD_KEY_OPEN;
	tItems[17].cmd = CMD_KEY_SHORT;
	tItems[18].cmd = CMD_ST_DAC;
	tItems[19].cmd = CMD_ST_BG;
	tItems[20].cmd = CMD_ST_NO_BK;
	tItems[21].cmd = CMD_ST_HAVE_BK;
	tItems[22].cmd = CMD_ST_OPEN;
	tItems[23].cmd = CMD_TX_SHORT;
	tItems[24].cmd = CMD_RX_SHORT;
	tItems[25].cmd = CMD_RX_OPEN;
	tItems[26].cmd = CMD_CM_DATA;
	tItems[27].cmd = CMD_CS_DATA;
	tItems[28].cmd = CMD_TX_RX_DELTA;
	tItems[29].cmd = CMD_MUTUAL_SIGNAL;
	tItems[30].cmd = CMD_MUTUAL_NO_BK;
	tItems[31].cmd = CMD_MUTUAL_HAVE_BK;
	tItems[32].cmd = CMD_RX_SHORT;
	tItems[33].cmd = CMD_RX_SHORT;
	tItems[34].cmd = CMD_PEAK_TO_PEAK;
}


static void mp_test_run(void *chip_data, char *item,
			struct auto_testdata *testdata)
{
	int i;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (item == NULL || strncmp(item, " ", strlen(item)) == 0
			|| chip_info->core_mp->frame_len == 0) {
		chip_info->core_mp->final_result = MP_FAIL;
		TPD_INFO("Invaild string (%s) or frame length (%d)\n", item,
			 chip_info->core_mp->frame_len);
		return;
	}

	TPD_DEBUG("Test item = %s\n", item);

	for (i = 0; i < MP_TEST_ITEM; i++) {
		if (strncmp(item, tItems[i].desp, strlen(item)) == 0) {
			if (strlen(item) != strlen(tItems[i].desp)) {
				continue;
			}

			if (strncmp("open test_c", tItems[i].desp, strlen("open test_c")) == 0) {
				ilitek_get_para_openshort(testdata, &tItems[i],
							  &chip_info->core_mp->open_c_spec, tItems[i].test_index);

			} else {
				ilitek_get_item_para(testdata, &tItems[i], tItems[i].test_index);
			}

			TPD_DEBUG("%s: run = %d, max = %d, min = %d, frame_count = %d\n",
				  tItems[i].desp,
				  tItems[i].run, tItems[i].max, tItems[i].min, tItems[i].frame_count);

			TPD_DEBUG("v_tdf_1 = %d, v_tdf_2 = %d, h_tdf_1 = %d, h_tdf_2 = %d\n",
				  tItems[i].v_tdf_1,
				  tItems[i].v_tdf_2, tItems[i].h_tdf_1, tItems[i].h_tdf_2);

			if (!tItems[i].run) {
				continue;
			}

			TPD_INFO("index: %d,Run MP Test Item : %s\n", i, tItems[i].desp);

			if (tItems[i].do_test) {
				tItems[i].do_test(chip_info, i);
			}

			TPD_INFO("index: %d,Run MP Test Item : %s end\n", i, tItems[i].desp);

			/* Check result before do retry (if enabled)  */
			if (mp_comp_result_before_retry(chip_info, i) == MP_FAIL) {
				if (chip_info->core_mp->retry) {
					TPD_INFO("MP failed, doing retry\n");
					mp_do_retry(chip_info, i, RETRY_COUNT);
				}
			}
		}
	}
}

static void mp_test_free(void *chip_data)
{
	int i;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("Free all allocated mem for MP\n");

	chip_info->core_mp->final_result = MP_FAIL;

	for (i = 0; i < ARRAY_SIZE(tItems); i++) {
		tItems[i].run = false;
		tItems[i].max_res = MP_FAIL;
		tItems[i].min_res = MP_FAIL;
		tItems[i].item_result = MP_PASS;
		snprintf(tItems[i].result, 16, "%s", "FAIL");
		tp_kfree((void **)&tItems[i].result_buf);
		tp_kfree((void **)&tItems[i].max_buf);
		tp_kfree((void **)&tItems[i].min_buf);
		tp_vfree((void **)&tItems[i].buf);
	}

	tp_kfree((void **)&chip_info->core_mp->frame_buf);
}

/* The method to copy results to user depends on what APK needs */
static void mp_copy_ret_to_apk(void *chip_data, char *buf, struct seq_file *s,
			       char *message, int msg_size)
{
	int i, run = 0, count = 0;
	int test_failed_items = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (!buf) {
		TPD_INFO("apk buffer is null\n");
		return;
	}

	for (i = 0; i < MP_TEST_ITEM; i++) {
		buf[i] = 2;

		if (tItems[i].run) {
			if (tItems[i].item_result == MP_FAIL) {
				test_failed_items++;
				buf[i] = 1;

				if (message != NULL) {
					if (count < msg_size) {
						count += snprintf(message + count, msg_size - count,
								  "%s FAIL;        ", tItems[i].desp);
					}
				}

				if (!ERR_ALLOC_MEM(s)) {
					seq_printf(s, "%s FAIL;        ", tItems[i].desp);
				}

			} else {
				buf[i] = 0;
			}

			run++;
		}
	}

	chip_info->mp_result_count = test_failed_items;
}



/*
static int ilitek_noise_peak_to_peak_with_panel(struct seq_file *s, void *chip_data,
	struct auto_testdata *ilitek_testdata, struct test_item_info *p_test_item_info)
{
    int ret = 0;
    struct test_item_info *p_test_item_info = NULL;

    p_test_item_info = get_test_item_info(ilitek_testdata->fw, TYPE_TEST2);
    if (!p_test_item_info) {
        TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST2);
        ret = -1;
    } else {
	    if (p_test_item_info->p_buffer[0] == 1) {
			p_test_item_info->p_buffer[0];
	    }
    }

    ilitek_get_item_para1(ilitek_testdata, &tItems[i], TYPE_TEST3);


    tp_kfree((void **)&p_test_item_info);
    return ret;
}
*/

static int mp_get_timing_info(void *chip_data, struct auto_testdata *testdata)
{
	int ret = 0;
	struct test_item_info *p_test_item_info = NULL;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	p_test_item_info = get_test_item_info(testdata->fw, TYPE_TIMEING_INFO);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TIMEING_INFO);
		ret = -1;

	} else {
		if (p_test_item_info->p_buffer[0] == 1) {
			chip_info->core_mp->is_longv = p_test_item_info->p_buffer[7];
		}
	}

	TPD_INFO("p_test_item_info->p_buffer[7]%d\n", p_test_item_info->p_buffer[7]);
	TPD_INFO("DDI Mode = %s\n",
		 (chip_info->core_mp->is_longv ? "Long V" : "Long H"));

	return ret;
}

int ilitek_tddi_mp_test_main(char *apk, struct seq_file *s,
			     char *message, int msg_size,
			     bool lcm_on, void *chip_data,
			     struct auto_testdata *testdata)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	ilitek_tddi_mp_init_item(chip_info);
	/* Read timing info from ini file */
	ret = mp_get_timing_info(chip_info, testdata);

	if (ret < 0) {
		TPD_INFO("Failed to get timing info from ini\n");

		if (!ERR_ALLOC_MEM(message)) {
			snprintf(message, msg_size, "get timing info failed\n");
		}

		if (!ERR_ALLOC_MEM(s)) {
			seq_printf(s, "get timing info failed\n");
		}

		goto out;
	}

	/* Do not chang the sequence of test */
	if (chip_info->protocol->ver >= PROTOCOL_VER_540) {
		if (lcm_on) {
			mp_test_run(chip_info, "noise peak to peak(with panel)", testdata);
			mp_test_run(chip_info, "noise peak to peak(ic only)", testdata);
			mp_test_run(chip_info, "short test -ili9881",
				    testdata);  /*compatible with old ini version.*/
			mp_test_run(chip_info, "short test", testdata); /*ini not set*/
			mp_test_run(chip_info, "open test(integration)_sp", testdata);/*ini not set*/
			mp_test_run(chip_info, "raw data(no bk)", testdata);
			mp_test_run(chip_info, "raw data(have bk)", testdata);/*ini not set*/
			mp_test_run(chip_info, "calibration data(dac)", testdata);
			mp_test_run(chip_info, "doze raw data", testdata);
			mp_test_run(chip_info, "doze peak to peak", testdata);
			mp_test_run(chip_info, "open test_c", testdata);
			mp_test_run(chip_info, "touch deltac", testdata);/*ini not set*/

		} else {
			mp_test_run(chip_info, "raw data(have bk) (lcm off)", testdata);/*ini not set*/
			mp_test_run(chip_info, "raw data(no bk) (lcm off)", testdata);
			mp_test_run(chip_info, "noise peak to peak(with panel) (lcm off)", testdata);
			mp_test_run(chip_info, "noise peak to peak(ic only) (lcm off)",
				    testdata);/*ini not set*/
			mp_test_run(chip_info, "raw data_td (lcm off)", testdata);
			mp_test_run(chip_info, "peak to peak_td (lcm off)", testdata);
		}
	}

	mp_show_result(chip_info, testdata);
	mp_copy_ret_to_apk(chip_info, apk, s, message, msg_size);
	mp_test_free(chip_info);

out:

	return ret;
};

int ilitek_tddi_mp_test_handler(char *apk, struct seq_file *s,
				char *message, int msg_size,
				bool lcm_on, void *chip_data,
				struct auto_testdata *testdata)
{
	int ret = 0;
	u8 tp_mode = P5_X_FW_TEST_MODE;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	enable_irq(chip_info->irq_num);/*because oplus disable*/

	if (atomic_read(&chip_info->fw_stat)) {
		TPD_INFO("fw upgrade processing, ignore\n");

		if (!ERR_ALLOC_MEM(message)) {
			snprintf(message, msg_size, "fw upgrade processing, ignore\n");
		}

		if (!ERR_ALLOC_MEM(s)) {
			seq_printf(s, "fw upgrade processing, ignore\n");
		}

		return 0;
	}

	if (!chip_info->chip->open_c_formula ||
			!chip_info->chip->open_sp_formula) {
		TPD_INFO("formula is null\n");

		if (!ERR_ALLOC_MEM(message)) {
			snprintf(message, msg_size, "formula is null\n");
		}

		if (!ERR_ALLOC_MEM(s)) {
			seq_printf(s, "formula is null\n");
		}

		return -1;
	}

	chip_info->esd_check_enabled = false;
	mutex_lock(&chip_info->touch_mutex);
	atomic_set(&chip_info->mp_stat, ENABLE);
	chip_info->need_judge_irq_throw = true;

	if (chip_info->actual_tp_mode != P5_X_FW_TEST_MODE) {
		if (ilitek_tddi_switch_mode(chip_info, &tp_mode) < 0) {
			if (!ERR_ALLOC_MEM(message)) {
				snprintf(message, msg_size, "switch test mode failed\n");
			}

			if (!ERR_ALLOC_MEM(s)) {
				seq_printf(s, "switch test mode failed\n");
			}

			goto out;
		}
	}

	ret = ilitek_tddi_mp_test_main(apk, s, message, msg_size,
				       lcm_on, chip_info, testdata);

out:
	/* Set tp as demo mode and reload code if it's iram. */
	chip_info->actual_tp_mode = P5_X_FW_DEMO_MODE;

	if (lcm_on) {
		ilitek_tddi_fw_upgrade(idev);
	}

	atomic_set(&chip_info->mp_stat, DISABLE);
	chip_info->need_judge_irq_throw = false;
	mutex_unlock(&chip_info->touch_mutex);
	chip_info->esd_check_enabled = true;
	disable_irq_nosync(chip_info->irq_num);
	return ret;
}

static int ili9881_auto_test_preoperation(struct seq_file *s,
		void *chip_data,
		struct auto_testdata *testdata)
{
	char apk_ret[100] = {0};
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("s->size = %d  s->count = %d\n", (int)s->size, (int)s->count);
	chip_info->mp_result_count = 0;
	ilitek_tddi_mp_test_handler(apk_ret, s, NULL, 0, ON, chip_info, testdata);
	TPD_INFO("chip_info->mp_result_count = %d\n", chip_info->mp_result_count);

	return chip_info->mp_result_count;
}

static int ili9881_black_screen_preoperation(char *msg, int msg_size,
		void *chip_data, struct auto_testdata *testdata)
{
	char apk_ret[100] = {0};
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	chip_info->mp_result_count = 0;
	ilitek_tddi_mp_test_handler(apk_ret, NULL, msg, msg_size, OFF, chip_info,
				    testdata);
	TPD_INFO("chip_info->mp_result_count = %d\n", chip_info->mp_result_count);

	return chip_info->mp_result_count;
}

struct ilitek_test_operations ilitek_9881_test_ops = {
	.auto_test_preoperation = ili9881_auto_test_preoperation,
	.black_screen_preoperation = ili9881_black_screen_preoperation,
};

struct engineer_test_operations ilitek_9881_engineer_test_ops = {
	.auto_test               = ilitek_auto_test,
	.black_screen_test = ilitek_black_screen_test,
};
