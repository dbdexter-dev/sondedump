#include "protocol.h"

uint8_t ims100_bch_roots[] = {0x02, 0x04, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

extern uint16_t IMS100Frame_seq(const IMS100Frame *frame);

extern uint16_t IMS100FrameEven_ms(const IMS100FrameEven *frame);
extern uint8_t IMS100FrameEven_hour(const IMS100FrameEven *frame);
extern uint8_t IMS100FrameEven_min(const IMS100FrameEven *frame);
extern time_t IMS100FrameEven_time(const IMS100FrameEven *frame);
extern float IMS100FrameEven_lat(const IMS100FrameEven *frame);
extern float IMS100FrameEven_lon(const IMS100FrameEven *frame);
extern float IMS100FrameEven_alt(const IMS100FrameEven *frame);
