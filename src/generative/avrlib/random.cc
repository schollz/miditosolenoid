// Implementation of Random class for bare-metal RP2040
// No time() available; seed with a constant (caller should re-seed from hardware timer)

#include "avrlib/random.h"

namespace avrlib {

uint32_t Random::rng_state_ = 0x12345678;

}  // namespace avrlib
