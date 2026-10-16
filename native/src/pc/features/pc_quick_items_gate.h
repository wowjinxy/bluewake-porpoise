// Small value-only gate shared by the SDL adapter and its lifecycle tests.
#ifndef PC_QUICK_ITEMS_GATE_H
#define PC_QUICK_ITEMS_GATE_H
#include <stdint.h>

namespace pc::quick_items {
constexpr uint16_t left = 0x0001, right = 0x0002, down = 0x0004, up = 0x0008;
constexpr uint16_t directions = left | right | down | up;
constexpr uint16_t claimable = left | right | up;

// SDL-independent profile policy. A physical D-pad is safe to claim only when
// all four sources map once to their matching directions, with no aliases, and
// neither it nor the modifier also supplies a native button-backed axis.
class PhysicalProfile {
    uint32_t bumper_, nativeUp_, nativeLeft_, nativeRight_, nativeDown_;
    uint16_t seen_ = 0;
    bool valid_ = true;
    uint64_t fingerprint_ = UINT64_C(14695981039346656037);

    void mix(uint32_t value) {
        fingerprint_ = (fingerprint_ ^ value) * UINT64_C(1099511628211);
    }

public:
    PhysicalProfile(uint32_t bumper, uint32_t nativeUp, uint32_t nativeLeft,
                    uint32_t nativeRight, uint32_t nativeDown)
        : bumper_(bumper), nativeUp_(nativeUp), nativeLeft_(nativeLeft),
          nativeRight_(nativeRight), nativeDown_(nativeDown) {}

    void observe(uint32_t native, uint16_t mapped) {
        mix(0);
        mix(native);
        mix(mapped);
        if (native == bumper_) valid_ = false;
        const uint16_t expected = native == nativeUp_ ? up : native == nativeLeft_ ? left :
                                  native == nativeRight_ ? right : native == nativeDown_ ? down : 0;
        if (expected != 0) {
            if (mapped != expected || (seen_ & expected) != 0) valid_ = false;
            seen_ |= expected;
        } else if ((mapped & directions) != 0) {
            valid_ = false;
        }
    }
    void observeAxis(int32_t nativeAxis, int32_t sign, int32_t nativeButton, uint16_t mapped) {
        mix(1);
        mix((uint32_t)nativeAxis);
        mix((uint32_t)sign);
        mix((uint32_t)nativeButton);
        mix(mapped);
        if (nativeAxis == -1 && nativeButton >= 0) {
            const uint32_t button = (uint32_t)nativeButton;
            if (button == bumper_ || button == nativeUp_ || button == nativeLeft_ ||
                button == nativeRight_ || button == nativeDown_) valid_ = false;
        }
    }
    bool canonicalMapping() const { return valid_ && seen_ == directions; }
    uint64_t fingerprint() const { return fingerprint_; }
};

// Keyboard scancodes are global. For this first slice the shortcut keys must
// be unbound in every active native keyboard button/axis profile.
class KeyboardProfile {
    int32_t tab_, up_, left_, right_, down_;
    bool available_ = true;
    uint64_t fingerprint_ = UINT64_C(14695981039346656037);

    void mix(uint32_t value) {
        fingerprint_ = (fingerprint_ ^ value) * UINT64_C(1099511628211);
    }
    void observe(uint32_t port, int32_t key, uint32_t kind, uint16_t mapped, int16_t influence) {
        mix(port);
        mix((uint32_t)key);
        mix(kind);
        mix(mapped);
        mix((uint16_t)influence);
        if (key == tab_ || key == up_ || key == left_ || key == right_ || key == down_) available_ = false;
    }

public:
    KeyboardProfile(int32_t tab, int32_t up, int32_t left, int32_t right, int32_t down)
        : tab_(tab), up_(up), left_(left), right_(right), down_(down) {}
    void observeButton(uint32_t port, int32_t key, uint16_t button) { observe(port, key, 0, button, 0); }
    void observeAxis(uint32_t port, int32_t key, uint16_t axis, int16_t influence) {
        observe(port, key, 1, axis, influence);
    }
    bool available() const { return available_; }
    uint64_t fingerprint() const { return fingerprint_; }
};

struct Sample {
    uint64_t context = 0;
    uint64_t epoch = 0;
    uint16_t direction = 0;
    bool enabled = false;
    bool modifier = false;
    bool blocked = false;
};
struct Intent {
    uint16_t claimed = 0;
    unsigned pressed = 0;
    unsigned held = 0;
    uint64_t context = 0;
};

class Gate {
    bool seen_ = false, ready_ = false, conflict_ = false;
    bool enabled_ = false, blocked_ = false;
    uint64_t context_ = 0, epoch_ = 0;
    uint16_t previous_ = 0, claimed_ = 0;
    unsigned active_ = 0;

public:
    uint16_t claimed() const { return claimed_; }
    uint16_t release_claims(uint16_t held) {
        claimed_ &= held & claimable;
        return claimed_;
    }
    Intent sample(const Sample& input) {
        const uint16_t current = input.direction & directions;
        release_claims(current);
        const bool changed = !seen_ || context_ != input.context || epoch_ != input.epoch ||
                             enabled_ != input.enabled || blocked_ != input.blocked;
        seen_ = true;
        context_ = input.context;
        epoch_ = input.epoch;
        enabled_ = input.enabled;
        blocked_ = input.blocked;
        if (changed) {
            ready_ = false;
            conflict_ = false;
            active_ = 0;
        }
        Intent result{claimed_, 0, 0, input.context};
        // Own a new modifier chord even while the actor/menu/focus is blocked.
        // Releasing the modifier first must not leak its held direction later.
        if (input.enabled && input.modifier) claimed_ |= current & claimable;
        result.claimed = claimed_;
        if (!input.enabled || input.blocked || input.context == 0) {
            ready_ = false;
            active_ = 0;
            previous_ = current;
            return result;
        }
        // A held shortcut cannot turn into a map press while the release gate
        // is closed. Down is always left to the original game.
        if (!ready_) {
            if (!input.modifier && current == 0) ready_ = true;
            previous_ = current;
            return result;
        }
        if (current == 0) {
            conflict_ = false;
            active_ = 0;
        } else if ((current & (current - 1)) != 0) {
            // Resolving a diagonal by releasing one direction is not a new
            // press. Require a neutral D-pad before accepting another action.
            conflict_ = true;
            active_ = 0;
        } else if (!input.modifier || conflict_) {
            active_ = 0;
        } else {
            const unsigned action = current == up ? 1 : current == left ? 2 : current == right ? 3 : 0;
            if ((current & ~previous_) != 0) {
                active_ = action;
                result.pressed = action;
            } else if (active_ != action) {
                active_ = 0;
            }
            result.held = active_;
        }
        previous_ = current;
        return result;
    }
};
} // namespace pc::quick_items
#endif
