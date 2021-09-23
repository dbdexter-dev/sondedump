#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ecc/crc.h"
#include "subframe.h"

static void print_status(RS41Subframe_Status *status);
static void print_unknown(RS41Subframe *sf);

void
rs41_parse_subframe(RS41Subframe *sub)
{
	uint16_t expected_crc;

	/* Validate subframe */
	expected_crc = sub->data[sub->len] | (uint16_t)sub->data[sub->len + 1] << 8;
	if (crc16_ccitt_false(sub->data, sub->len) != expected_crc) {
		return;
	}

	switch (sub->type) {
		case RS41_SFTYPE_EMPTY:
			/* Empty subframe, used for padding */
			break;
		case RS41_SFTYPE_INFO:
			/* Frame sequence number, serial no. */
			print_status((RS41Subframe_Status*)sub);
			break;
		default:
			/* Unknown */
			//print_unknown(sub);
			break;
	}
}

static void
print_status(RS41Subframe_Status *status)
{
	char serial_str[8+1];

	strncpy(serial_str, status->serial, 8);
	serial_str[8] = 0;

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
