/* 
 * Maxim ModelGauge ICs fuel gauge driver header file 
 * 
 * Author: Vladimir Barinov <sou...@cogentembedded.com> 
 * Copyright (C) 2013 Cogent Embedded, Inc. 
 * 
 * This program is free software; you can redistribute  it and/or modify it 
 * under  the terms of  the GNU General  Public License as published by the 
 * Free Software Foundation;  either version 2 of the  License, or (at your 
 * option) any later version. 
 */ 
 
#ifndef __BATTERY_MODELGAUGE_H_ 
#define __BATTERY_MODELGAUGE_H_ 
 
#define MODELGAUGE_TABLE_SIZE        64 
 
struct modelgauge_platform_data { 
	u8	empty_adjustment;
	u8	full_adjustment;
	u8	rcomp0; 
	int	temp_co_up; 
	int	temp_co_down; 
	u16	ocvtest; 
	u8	soc_check_a; 
	u8	soc_check_b; 
	u8	bits; 
	u16	rcomp_seg; 
	u8	*model_data; 
	int	(*get_temperature)(void); 
	int	(*get_charging_status)(void); 
}; 
#endif