// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2023 Oplus. All rights reserved.
 */

#ifndef __OPLUS_SH366002_H__
#define __OPLUS_SH366002_H__

#define DTSI_MODEL_NAME   "sino_default_cell_model"

s32 sh366002_read_gaugeinfo_block(struct chip_bq27541 *chip);
s32 sh366002_init(struct chip_bq27541 *chip);

s32 fg_gauge_calibrate_board(struct chip_bq27541 *chip);
s32 fg_gauge_check_cell_model(struct chip_bq27541 *chip, char *profile_name);
s32 fg_gauge_restore_cell_model(struct chip_bq27541 *chip, char *profile_name);
s32 fg_gauge_check_por_soc(struct chip_bq27541 *chip);
s32 fg_gauge_enable_sleep_mode(struct chip_bq27541 *chip);

#endif /* __OPLUS_SH366002_H__ */
