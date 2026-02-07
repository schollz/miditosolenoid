#include "generative/generative_controller.h"

#include <stdio.h>
#include <string.h>

#include "grids/pattern_generator.h"
#include "avrlib/random.h"

namespace generative {

GenerativeController::GenerativeController()
    : verbose_(false),
      bpm_tenths_(1200),
      us_per_pulse_(0),
      us_accumulator_(0),
      current_step_(0),
      pulse_in_step_(0),
      step_evaluated_(false),
      rng_state_(0x12345678) {
    memset(channels_, 0, sizeof(channels_));
}

uint32_t GenerativeController::SimpleRand() {
    rng_state_ = rng_state_ * 1664525u + 1013904223u;
    return rng_state_;
}

void GenerativeController::Init(uint32_t seed, uint32_t bpm_tenths) {
    rng_state_ = seed;
    bpm_tenths_ = bpm_tenths;

    // Initialize the Grids pattern generator
    grids::PatternGenerator::Init();

    // Seed the avrlib RNG used by Grids internally
    avrlib::Random::Seed(SimpleRand());

    // Set default Grids settings
    grids::PatternGeneratorSettings* settings =
        grids::PatternGenerator::mutable_settings();
    settings->options.drums.x = 128;
    settings->options.drums.y = 128;
    settings->options.drums.randomness = 0;
    for (int i = 0; i < grids::kNumParts; ++i) {
        settings->density[i] = 128;
    }

    // Reset timing
    us_accumulator_ = 0;
    current_step_ = 0;
    pulse_in_step_ = 0;
    step_evaluated_ = false;
    UpdateUsPerPulse();

    // Randomize channel assignments
    Randomize();
}

void GenerativeController::SetBpm(uint32_t bpm_tenths) {
    bpm_tenths_ = bpm_tenths;
    UpdateUsPerPulse();
}

void GenerativeController::UpdateUsPerPulse() {
    // 24 PPQN: us_per_pulse = 60_000_000 / (bpm * 24)
    // With bpm in tenths: us_per_pulse = 600_000_000 / (bpm_tenths * 24)
    // = 25_000_000 / bpm_tenths
    if (bpm_tenths_ > 0) {
        us_per_pulse_ = 25000000u / bpm_tenths_;
    }
}

void GenerativeController::Randomize() {
    for (uint8_t i = 0; i < kNumChannels; ++i) {
        ChannelState& ch = channels_[i];
        ch.drum_part = SimpleRand() % 3;  // BD, SD, or HH
        ch.x = SimpleRand() & 0xFF;
        ch.y = SimpleRand() & 0xFF;
        ch.density = 100 + (SimpleRand() % 100); // 100-199 range for moderate density

        // Generate random 32-bit velocity pattern
        ch.velocity_bits = SimpleRand();

        ch.velocity_step = 0;
    }

    // Reset step position on randomize
    current_step_ = 0;
    pulse_in_step_ = 0;
    step_evaluated_ = false;
    grids::PatternGenerator::Init();

    // Re-seed Grids RNG
    avrlib::Random::Seed(SimpleRand());

    // Restore Grids settings
    grids::PatternGeneratorSettings* settings =
        grids::PatternGenerator::mutable_settings();
    settings->options.drums.x = 128;
    settings->options.drums.y = 128;
    settings->options.drums.randomness = 0;
    for (int i = 0; i < grids::kNumParts; ++i) {
        settings->density[i] = 128;
    }

    if (verbose_) {
        PrintPatterns();
    }
}

FireEvent GenerativeController::Tick() {
    FireEvent event;
    event.gpio_mask = 0;
    memset(event.duration_ms, 0, sizeof(event.duration_ms));

    us_accumulator_ += 1000;  // 1 ms per tick

    if (us_accumulator_ < us_per_pulse_) {
        return event;
    }
    us_accumulator_ -= us_per_pulse_;

    // Advance the Grids engine by 1 pulse
    grids::PatternGenerator::TickClock(1);

    // Wrap at 32 steps
    if (grids::PatternGenerator::step() >= grids::kStepsPerPattern) {
        grids::PatternGenerator::set_step(0);
    }

    pulse_in_step_++;
    if (pulse_in_step_ >= grids::kPulsesPerStep) {
        pulse_in_step_ = 0;
        current_step_ = grids::PatternGenerator::step();
        step_evaluated_ = false;
    }

    // Only evaluate triggers at the start of each step
    if (!step_evaluated_ && pulse_in_step_ == 0) {
        step_evaluated_ = true;

        // Evaluate each channel against the Grids drum map
        for (uint8_t i = 0; i < kNumChannels; ++i) {
            ChannelState& ch = channels_[i];

            uint8_t level = grids::PatternGenerator::GetDrumMapLevel(
                current_step_, ch.drum_part, ch.x, ch.y);

            uint8_t threshold = 255 - ch.density;
            if (level > threshold) {
                // Trigger! Check velocity pattern
                bool high_vel = (ch.velocity_bits >> ch.velocity_step) & 1;
                uint8_t duration = high_vel ? 100 : 1;

                event.gpio_mask |= (1 << i);
                event.duration_ms[i] = duration;

                // Advance velocity step (only on trigger)
                ch.velocity_step = (ch.velocity_step + 1) % kPatternSteps;
            }
        }

        // Single compact line per step with all triggers
        if (verbose_ && event.gpio_mask) {
            printf("S%02u:", current_step_);
            for (uint8_t i = 0; i < kNumChannels; ++i) {
                if (event.gpio_mask & (1 << i)) {
                    printf(" %u%c", i, event.duration_ms[i] > 1 ? 'H' : 'L');
                }
            }
            printf("\n");
        }
    }

    grids::PatternGenerator::IncrementPulseCounter();

    return event;
}

void GenerativeController::PrintChannel(uint8_t ch) const {
    const ChannelState& c = channels_[ch];
    const char* part_names[] = {"BD", "SD", "HH"};

    // Compute trigger pattern for display
    printf("  CH%u %s x=%3u y=%3u d=%3u T:", ch, part_names[c.drum_part],
           c.x, c.y, c.density);

    for (uint8_t step = 0; step < kPatternSteps; ++step) {
        uint8_t level = grids::PatternGenerator::GetDrumMapLevel(
            step, c.drum_part, c.x, c.y);
        uint8_t threshold = 255 - c.density;
        printf("%c", level > threshold ? 'x' : '-');
    }

    printf(" V:");
    for (uint8_t step = 0; step < kPatternSteps; ++step) {
        printf("%c", (c.velocity_bits >> step) & 1 ? '1' : '0');
    }
    printf("\n");
}

void GenerativeController::PrintPatterns() const {
    printf("[GEN] Pattern dump (BPM=%u.%u):\n",
           bpm_tenths_ / 10, bpm_tenths_ % 10);
    for (uint8_t i = 0; i < kNumChannels; ++i) {
        PrintChannel(i);
    }
}

}  // namespace generative
