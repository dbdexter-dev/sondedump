#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ecc/crc.h"
#include "subframe.h"
#include "utils.h"

static void print_status(RS41Subframe_Status *status);
static void print_ptu(RS41Subframe_PTU *status);
static void print_unknown(RS41Subframe *sf);

float
rs41_subframe_temp(RS41Subframe_PTU *ptu)
{
	uint32_t temp_main = (uint32_t)ptu->temp_main[0]
	                   | (uint32_t)ptu->temp_main[1] << 8
	                   | (uint32_t)ptu->temp_main[2] << 16;
	uint32_t temp_ref1 = (uint32_t)ptu->temp_ref1[0]
	                   | (uint32_t)ptu->temp_ref1[1] << 8
	                   | (uint32_t)ptu->temp_ref1[2] << 16;
	uint32_t temp_ref2 = (uint32_t)ptu->temp_ref2[0]
	                   | (uint32_t)ptu->temp_ref2[1] << 8
	                   | (uint32_t)ptu->temp_ref2[2] << 16;
}

float
rs41_subframe_humidity(RS41Subframe_PTU *ptu)
{
	uint32_t humidity_main = (uint32_t)ptu->humidity_main[0]
	                       | (uint32_t)ptu->humidity_main[1] << 8
	                       | (uint32_t)ptu->humidity_main[2] << 16;
	uint32_t humidity_ref1 = (uint32_t)ptu->humidity_ref1[0]
	                       | (uint32_t)ptu->humidity_ref1[1] << 8
	                       | (uint32_t)ptu->humidity_ref1[2] << 16;
	uint32_t humidity_ref2 = (uint32_t)ptu->humidity_ref2[0]
	                       | (uint32_t)ptu->humidity_ref2[1] << 8
	                       | (uint32_t)ptu->humidity_ref2[2] << 16;
}

float
rs41_subframe_temp_humidity(RS41Subframe_PTU *ptu)
{
	uint32_t temp_humidity_main = (uint32_t)ptu->temp_humidity_main[0]
	                            | (uint32_t)ptu->temp_humidity_main[1] << 8
	                            | (uint32_t)ptu->temp_humidity_main[2] << 16;
	uint32_t temp_humidity_ref1 = (uint32_t)ptu->temp_humidity_ref1[0]
	                            | (uint32_t)ptu->temp_humidity_ref1[1] << 8
	                            | (uint32_t)ptu->temp_humidity_ref1[2] << 16;
	uint32_t temp_humidity_ref2 = (uint32_t)ptu->temp_humidity_ref2[0]
	                            | (uint32_t)ptu->temp_humidity_ref2[1] << 8
	                            | (uint32_t)ptu->temp_humidity_ref2[2] << 16;
}

float
rs41_subframe_pressure(RS41Subframe_PTU *ptu)
{
	uint32_t pressure_main = (uint32_t)ptu->pressure_main[0]
	                       | (uint32_t)ptu->pressure_main[1] << 8
	                       | (uint32_t)ptu->pressure_main[2] << 16;
	uint32_t pressure_ref1 = (uint32_t)ptu->pressure_ref1[0]
	                       | (uint32_t)ptu->pressure_ref1[1] << 8
	                       | (uint32_t)ptu->pressure_ref1[2] << 16;
	uint32_t pressure_ref2 = (uint32_t)ptu->pressure_ref2[0]
	                       | (uint32_t)ptu->pressure_ref2[1] << 8
	                       | (uint32_t)ptu->pressure_ref2[2] << 16;
}

float
rs41_subframe_pressure_temp(RS41Subframe_PTU *ptu)
{
	return ptu->pressure_temp/100.0;
}

/* Static functions {{{ */
static void
print_status(RS41Subframe_Status *status)
{
	char serial_str[RS41_SERIAL_LEN+1];

	strncpy(serial_str, status->serial, 8);
	serial_str[LEN(serial_str)-1] = 0;

	printf("[%s] %05d, %2.1fV, %s. Board temp: %d'C\n",
			serial_str,
			status->frame_seq,
			status->bat_voltage/10.0f,
			status->flight_status & RS41_FLIGHT_STATUS_DESCEND_MSK ? "descending" : "ascending",
			status->pcb_temp
		  );
}

static void
print_unknown(RS41Subframe *sub)
{
	int i;

	printf("Unknown 0x%02x, length %d: ", sub->type, sub->len);
	for (i=0; i<sub->len; i++) {
		printf("%02x ", sub->data[i]);
	}
	printf("\n");
}

static void
print_ptu(RS41Subframe_PTU *ptu)
{
	uint32_t temp_main = (uint32_t)ptu->temp_main[0]
	                   | (uint32_t)ptu->temp_main[1] << 8
	                   | (uint32_t)ptu->temp_main[2] << 16;
	uint32_t temp_ref1 = (uint32_t)ptu->temp_ref1[0]
	                   | (uint32_t)ptu->temp_ref1[1] << 8
	                   | (uint32_t)ptu->temp_ref1[2] << 16;
	uint32_t temp_ref2 = (uint32_t)ptu->temp_ref2[0]
	                   | (uint32_t)ptu->temp_ref2[1] << 8
	                   | (uint32_t)ptu->temp_ref2[2] << 16;
	uint32_t humidity_main = (uint32_t)ptu->humidity_main[0]
	                       | (uint32_t)ptu->humidity_main[1] << 8
	                       | (uint32_t)ptu->humidity_main[2] << 16;
	uint32_t humidity_ref1 = (uint32_t)ptu->humidity_ref1[0]
	                       | (uint32_t)ptu->humidity_ref1[1] << 8
	                       | (uint32_t)ptu->humidity_ref1[2] << 16;
	uint32_t humidity_ref2 = (uint32_t)ptu->humidity_ref2[0]
	                       | (uint32_t)ptu->humidity_ref2[1] << 8
	                       | (uint32_t)ptu->humidity_ref2[2] << 16;
	uint32_t temp_humidity_main = (uint32_t)ptu->temp_humidity_main[0]
	                            | (uint32_t)ptu->temp_humidity_main[1] << 8
	                            | (uint32_t)ptu->temp_humidity_main[2] << 16;
	uint32_t temp_humidity_ref1 = (uint32_t)ptu->temp_humidity_ref1[0]
	                            | (uint32_t)ptu->temp_humidity_ref1[1] << 8
	                            | (uint32_t)ptu->temp_humidity_ref1[2] << 16;
	uint32_t temp_humidity_ref2 = (uint32_t)ptu->temp_humidity_ref2[0]
	                            | (uint32_t)ptu->temp_humidity_ref2[1] << 8
	                            | (uint32_t)ptu->temp_humidity_ref2[2] << 16;
	uint32_t pressure_main = (uint32_t)ptu->pressure_main[0]
	                       | (uint32_t)ptu->pressure_main[1] << 8
	                       | (uint32_t)ptu->pressure_main[2] << 16;
	uint32_t pressure_ref1 = (uint32_t)ptu->pressure_ref1[0]
	                       | (uint32_t)ptu->pressure_ref1[1] << 8
	                       | (uint32_t)ptu->pressure_ref1[2] << 16;
	uint32_t pressure_ref2 = (uint32_t)ptu->pressure_ref2[0]
	                       | (uint32_t)ptu->pressure_ref2[1] << 8
	                       | (uint32_t)ptu->pressure_ref2[2] << 16;


	printf("Temp: %d %d %d\t Humidity: %d %d %d\t Temp humidity: %d %d %d\t Pressure: %d %d %d\n",
			temp_main, temp_ref1, temp_ref2,
			humidity_main, humidity_ref1, humidity_ref2,
			temp_humidity_main, temp_humidity_ref1, temp_humidity_ref2,
			pressure_main, pressure_ref1, pressure_ref2);

}
/* }}} */
