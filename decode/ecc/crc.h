#ifndef crc_h
#define crc_h

#include <stdint.h>
#include <stdlib.h>

/**
 * Compute a CRC16 on data using the CCITT-FALSE parameters
 *
 * @param data pointer to the data to compute the CRC on
 * @param len data length
 * @return the resulting CRC value
 */
uint16_t crc16_ccitt_false(const void *data, size_t len);

/**
 * Compute a CRC16 on data using the AUG-CCITT parameters
 *
 * @param data pointer to the data to compute the CRC on
 * @param len data length
 * @return the resulting CRC value
 */
uint16_t crc16_aug_ccitt(const void *data, size_t len);

/**
 * Compute a Fletcher16 checksum
 *
 * @param data pointer to the data to compute the FCS on
 * @param len data length
 * @return FCS16 checksum
 */
uint16_t fcs16(const void *data, size_t len);

/*
 * Compute a CRC16 on data using the MODBUS parameters
 *
 * @param data pointer to the data to compute the CRC on
 * @param len data length
 * @return the resulting CRC value
 */
uint16_t crc16_modbus(const void *v_data, size_t len);
#endif
