#ifndef imet4_subframe_h
#define imet4_subframe_h

#include <stdlib.h>
#include "protocol.h"

/**
 * Get the size of the given subframe
 *
 * @param sf    subframe to analyze
 * @return      size of the data within the subframe, including sof, type, and crc bytes
 */
size_t imet4_subframe_len(IMET4Subframe *sf);

#endif
