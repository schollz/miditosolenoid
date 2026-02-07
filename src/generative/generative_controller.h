#ifndef GENERATIVE_CONTROLLER_H_
#define GENERATIVE_CONTROLLER_H_

#include <stdint.h>

namespace generative {

static const uint8_t kNumChannels = 8;
static const uint8_t kPatternSteps = 32;

// Per-channel state for trigger + velocity dual patterns
struct ChannelState {
    uint8_t drum_part;       // 0=BD, 1=SD, 2=HH (which Grids part to follow)
    uint8_t x;               // Grids X position (0-255)
    uint8_t y;               // Grids Y position (0-255)
    uint8_t density;         // Trigger density threshold (0-255)
    uint32_t velocity_bits;  // 32-step binary velocity pattern (bit=1 -> high vel)
    uint8_t velocity_step;   // current position in velocity pattern (advances on trigger)
};

// Returned by Update() to tell main.cpp which solenoids to fire
struct FireEvent {
    uint8_t gpio_mask;       // bitmask: bit N = GPIO (N + GPIO_BASE) should fire
    uint8_t duration_ms[kNumChannels]; // pulse duration per channel (0 = no fire)
};

class GenerativeController {
public:
    GenerativeController();

    // Initialize: seed RNG, randomize patterns, set tempo
    void Init(uint32_t seed, uint32_t bpm_tenths);

    // Set tempo in tenths of BPM (e.g. 1200 = 120.0 BPM)
    void SetBpm(uint32_t bpm_tenths);

    // Call every 1 ms from main loop. Returns fire events when triggers occur.
    FireEvent Tick();

    // Re-roll all x/y positions, drum parts, and velocity patterns
    void Randomize();

    // Enable/disable verbose UART logging
    void SetVerbose(bool v) { verbose_ = v; }

    // Get current step (0-31) for display
    uint8_t step() const { return current_step_; }

    // Get channel state for display
    const ChannelState& channel(uint8_t ch) const { return channels_[ch]; }

    // Print all channel patterns to UART
    void PrintPatterns() const;

private:
    ChannelState channels_[kNumChannels];
    bool verbose_;

    // Timing
    uint32_t bpm_tenths_;         // tempo in 0.1 BPM units
    uint32_t us_per_pulse_;       // microseconds per PPQN pulse
    uint32_t us_accumulator_;     // accumulated microseconds (incremented by 1000 each Tick)

    // Grids sequencer state
    uint8_t current_step_;        // 0..31
    uint8_t pulse_in_step_;       // 0..2 (kPulsesPerStep = 3)
    bool step_evaluated_;         // has current step been evaluated yet?

    // Internal helpers
    void UpdateUsPerPulse();
    void EvaluateStep();
    uint32_t SimpleRand();
    uint32_t rng_state_;

    // Print a single channel's trigger/velocity patterns
    void PrintChannel(uint8_t ch) const;
};

}  // namespace generative

#endif  // GENERATIVE_CONTROLLER_H_
