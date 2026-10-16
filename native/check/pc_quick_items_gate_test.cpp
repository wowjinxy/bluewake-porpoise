// Value-only release/claim/epoch checks: no window, SDK, game or input devices.
#include "../src/pc/features/pc_quick_items_gate.h"
#include <cstdio>

using namespace pc::quick_items;
namespace {
int failures = 0;
#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); ++failures; } } while (0)

void neutral(Gate& gate, Sample& sample) {
    sample.direction = 0;
    sample.modifier = false;
    const auto result = gate.sample(sample);
    CHECK(result.pressed == 0 && result.held == 0 && result.claimed == 0);
}

PhysicalProfile canonical_profile() {
    PhysicalProfile profile(20, 10, 11, 12, 13);
    profile.observe(10, up);
    profile.observe(11, left);
    profile.observe(12, right);
    profile.observe(13, down);
    return profile;
}

void physical_profile_policy() {
    auto profile = canonical_profile();
    CHECK(profile.canonicalMapping());
    const uint64_t initialFingerprint = profile.fingerprint();
    profile.observe(21, 0x0100); // unrelated A mapping remains supported
    CHECK(profile.canonicalMapping() && profile.fingerprint() != initialFingerprint);
    auto sameProfile = canonical_profile();
    sameProfile.observe(21, 0x0100);
    CHECK(sameProfile.fingerprint() == profile.fingerprint());

    profile.observe(20, 0x0040); // LB already supplies native L
    CHECK(!profile.canonicalMapping());
    profile = canonical_profile();
    profile.observe(20, 0x0400); // any native LB binding, including X, conflicts
    CHECK(!profile.canonicalMapping());
    profile = canonical_profile();
    profile.observe(11, 0x0400); // physical Left also supplies native X
    CHECK(!profile.canonicalMapping());
    profile = canonical_profile();
    profile.observe(11, left); // duplicate physical source is also ambiguous
    CHECK(!profile.canonicalMapping());
    profile = canonical_profile();
    profile.observe(21, up); // another physical button aliases a direction
    CHECK(!profile.canonicalMapping());

    PhysicalProfile swapped(20, 10, 11, 12, 13);
    swapped.observe(10, up);
    swapped.observe(11, right);
    swapped.observe(12, left);
    swapped.observe(13, down);
    CHECK(!swapped.canonicalMapping());
    PhysicalProfile missing(20, 10, 11, 12, 13);
    missing.observe(10, up);
    missing.observe(11, left);
    missing.observe(12, right);
    CHECK(!missing.canonicalMapping());
    PhysicalProfile combined(20, 10, 11, 12, 13);
    combined.observe(10, up);
    combined.observe(11, left | 0x0400);
    combined.observe(12, right);
    combined.observe(13, down);
    CHECK(!combined.canonicalMapping());

    profile = canonical_profile();
    const uint64_t noAxesFingerprint = profile.fingerprint();
    profile.observeAxis(4, 1, -1, 8); // ordinary analog LT
    profile.observeAxis(5, 1, -1, 9); // ordinary analog RT
    CHECK(profile.canonicalMapping() && profile.fingerprint() != noAxesFingerprint);
    auto analogProfile = canonical_profile();
    analogProfile.observeAxis(4, 1, -1, 8);
    analogProfile.observeAxis(5, 1, -1, 9);
    CHECK(analogProfile.fingerprint() == profile.fingerprint());
    profile.observeAxis(0, -1, 20, 1); // an analog source ignores the stale nativeButton field
    CHECK(profile.canonicalMapping());
    profile.observeAxis(-1, 1, 21, 0); // unrelated button-backed axis
    CHECK(profile.canonicalMapping());
    const int shortcutButtons[] = {20, 10, 11, 12, 13};
    for (int button : shortcutButtons) {
        auto conflict = canonical_profile();
        conflict.observeAxis(-1, 1, button, 8); // LB or any D-pad also supplies a native axis
        CHECK(!conflict.canonicalMapping());
    }
}

void keyboard_profile_policy() {
    KeyboardProfile keyboard(10, 11, 12, 13, 14);
    CHECK(keyboard.available()); // default inactive/unbound keyboard
    keyboard.observeButton(0, -1, 0x0100);
    keyboard.observeAxis(0, -1, 1, 100);
    keyboard.observeButton(0, 20, 0x0100); // unrelated native key
    keyboard.observeAxis(0, 21, 1, 100);
    CHECK(keyboard.available());
    const uint64_t initialFingerprint = keyboard.fingerprint();
    keyboard.observeButton(1, 22, 0x0200);
    CHECK(keyboard.available() && keyboard.fingerprint() != initialFingerprint);
    KeyboardProfile same(10, 11, 12, 13, 14);
    same.observeButton(0, -1, 0x0100);
    same.observeAxis(0, -1, 1, 100);
    same.observeButton(0, 20, 0x0100);
    same.observeAxis(0, 21, 1, 100);
    CHECK(same.fingerprint() == initialFingerprint);

    for (int key = 10; key <= 14; ++key) {
        KeyboardProfile button(10, 11, 12, 13, 14);
        button.observeButton(0, key, 0x0400); // Tab or arrow also activates native X
        CHECK(!button.available());
        KeyboardProfile axis(10, 11, 12, 13, 14);
        axis.observeAxis(0, key, 1, 100);
        CHECK(!axis.available());
        KeyboardProfile otherPort(10, 11, 12, 13, 14);
        otherPort.observeButton(1, key, 0x0100); // global keys affect other ports too
        CHECK(!otherPort.available());
    }
    KeyboardProfile canonicalArrow(10, 11, 12, 13, 14);
    canonicalArrow.observeButton(0, 11, up);
    CHECK(!canonicalArrow.available()); // conservative first slice declines any binding
}

void ordinary_and_claimed_input() {
    Gate gate;
    Sample sample{1, 1, up, false, true, false};
    CHECK(gate.sample(sample).claimed == 0); // fresh disabled path is transparent
    sample.enabled = true;
    CHECK(gate.sample(sample).pressed == 0); // enabling during a hold is not a press
    neutral(gate, sample);
    sample.direction = down;
    sample.modifier = true;
    auto result = gate.sample(sample);
    CHECK(result.claimed == 0 && result.pressed == 0 && result.held == 0);
    neutral(gate, sample);
    sample.direction = up;
    sample.modifier = true;
    result = gate.sample(sample);
    CHECK(result.pressed == 1 && result.held == 1 && result.claimed == up);
    CHECK((uint16_t)((up | 0x0100 | 0x0040) & ~result.claimed) == (0x0100 | 0x0040));
    result = gate.sample(sample);
    CHECK(result.pressed == 0 && result.held == 1);
    sample.modifier = false;
    result = gate.sample(sample);
    CHECK(result.claimed == up && result.pressed == 0 && result.held == 0);
    sample.modifier = true;
    result = gate.sample(sample);
    CHECK(result.claimed == up && result.pressed == 0 && result.held == 0);
    neutral(gate, sample);
    sample.direction = left;
    sample.modifier = true;
    result = gate.sample(sample);
    CHECK(result.pressed == 2 && result.held == 2 && result.claimed == left);
    neutral(gate, sample);
    sample.direction = right;
    sample.modifier = true;
    result = gate.sample(sample);
    CHECK(result.pressed == 3 && result.held == 3 && result.claimed == right);
    // Disabling preserves an already claimed direction until its release.
    sample.enabled = false;
    result = gate.sample(sample);
    CHECK(result.claimed == right && result.pressed == 0 && result.held == 0);
    CHECK(gate.release_claims(right) == right);
    CHECK(gate.release_claims(0) == 0);
}

void conflicting_directions() {
    Gate gate;
    Sample sample{1, 1, 0, true, false, false};
    neutral(gate, sample);
    sample.direction = up | down;
    sample.modifier = true;
    auto result = gate.sample(sample);
    CHECK(result.pressed == 0 && result.held == 0 && result.claimed == up);
    sample.direction = up; // resolving a diagonal does not invent a press
    result = gate.sample(sample);
    CHECK(result.pressed == 0 && result.held == 0);
    sample.direction = 0; // modifier can stay held between ordinary shortcuts
    gate.sample(sample);
    sample.direction = left | right;
    result = gate.sample(sample);
    CHECK(result.pressed == 0 && result.held == 0 && result.claimed == (left | right));
    sample.direction = 0;
    gate.sample(sample);
    sample.direction = up;
    CHECK(gate.sample(sample).pressed == 1);
}

void invalidation_requires_release() {
    for (unsigned cause = 0; cause != 4; ++cause) {
        Gate gate;
        Sample sample{1, 1, 0, true, false, false};
        neutral(gate, sample);
        sample.direction = right;
        sample.modifier = true;
        CHECK(gate.sample(sample).pressed == 3);
        if (cause == 0) ++sample.epoch; // device or source replacement
        if (cause == 1) ++sample.context; // player/scene ownership replacement
        if (cause == 2) sample.blocked = true; // menu/focus loss
        if (cause == 3) sample.context = 0; // unsafe player state
        auto result = gate.sample(sample);
        CHECK(result.pressed == 0 && result.held == 0 && result.claimed == right);
        sample.blocked = false;
        if (sample.context == 0) sample.context = 2;
        result = gate.sample(sample);
        CHECK(result.pressed == 0 && result.held == 0);
        sample.direction = 0; // releasing only D-pad is insufficient after invalidation
        gate.sample(sample);
        sample.direction = right;
        CHECK(gate.sample(sample).pressed == 0);
        neutral(gate, sample);
        sample.modifier = true;
        sample.direction = right;
        CHECK(gate.sample(sample).pressed == 3);
    }
}

void blocked_chords_and_preheld_directions() {
    Gate gate;
    Sample sample{1, 1, 0, true, false, false};
    neutral(gate, sample);
    sample.blocked = true;
    sample.context = 0;
    sample.modifier = true;
    sample.direction = up;
    auto result = gate.sample(sample);
    CHECK(result.claimed == up && result.pressed == 0 && result.held == 0);
    sample.modifier = false;
    result = gate.sample(sample);
    CHECK(result.claimed == up);
    sample.blocked = false;
    sample.context = 1;
    result = gate.sample(sample);
    CHECK(result.claimed == up && result.pressed == 0 && result.held == 0);
    neutral(gate, sample);
    sample.direction = right; // e.g. keyboard arrow held before Tab
    result = gate.sample(sample);
    CHECK(result.claimed == 0 && result.pressed == 0 && result.held == 0);
    sample.modifier = true;
    result = gate.sample(sample);
    CHECK(result.claimed == right && result.pressed == 0 && result.held == 0);
    sample.modifier = false;
    CHECK(gate.sample(sample).claimed == right);
    neutral(gate, sample);
    sample.modifier = true;
    sample.direction = right;
    CHECK(gate.sample(sample).pressed == 3);
}
} // namespace

int main() {
    physical_profile_policy();
    keyboard_profile_policy();
    ordinary_and_claimed_input();
    conflicting_directions();
    invalidation_requires_release();
    blocked_chords_and_preheld_directions();
    if (failures != 0) return 1;
    std::puts("quick-items gate: ok");
    return 0;
}
