#ifndef ims100_frame_h
#define ims100_frame_h

#include "decode/ecc/rs.h"
#include "protocol.h"

typedef struct {
	uint16_t ref, temp, rh, rh_temp;
} IMS100FrameADC;


/**
 * Reorder bits within the frame so that they make sense
 *
 * @param frame frame to descramble
 */
void ims100_frame_descramble(IMS100ECCFrame *frame);

/**
 * Perform error correction on the frame
 *
 * @param frame frame to correct
 * @param rs Reed-Solomon decoder to use
 * @return -1 if uncorrectable errors were detected
 *         number of errors corrected otherwise
 */
int ims100_frame_error_correct(IMS100ECCFrame *frame, const RSDecoder *rs);

/**
 * Strip error-correcting bits from the given frame, reconstructing the payload
 *
 * @param dst destination buffer to write the compacted frame to
 * @param src source buffer containing the raw frame (including RS bits)
 */
void ims100_frame_unpack(IMS100Frame *dst, const IMS100ECCFrame *src);


/* Accessors for various subfields */
uint16_t ims100_frame_seq(const IMS100Frame *frame);

float ims100_frame_temp(const IMS100FrameADC *adc, const IMS100Calibration *calib);
float ims100_frame_rh(const IMS100FrameADC *adc, const IMS100Calibration *calib);

float rs11g_frame_temp(const IMS100FrameADC *adc, const RS11GCalibration *calib);
float rs11g_frame_rh(const IMS100FrameADC *adc, const RS11GCalibration *calib);
#endif
