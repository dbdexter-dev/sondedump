#include "protocol.h"

uint8_t ims100_bch_roots[] = {0x2, 0x4, 0x8, 0xf, 0x10, 0x1a, 0x21, 0x27, 0x2a, 0x2d, 0x34, 0x3e};

extern uint16_t IMS100Frame_seq(const IMS100Frame *frame);

extern uint16_t IMS100FrameEven_ms(const IMS100FrameEven *frame);
extern uint8_t IMS100FrameEven_hour(const IMS100FrameEven *frame);
extern uint8_t IMS100FrameEven_min(const IMS100FrameEven *frame);
extern time_t IMS100FrameEven_time(const IMS100FrameEven *frame);
