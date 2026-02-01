#pragma once
// Minimal stub for Dexed msfa dependency. The full Dexed project uses a
// microtuning library that provides Tunings::Tuning. We only need a placeholder
// here to satisfy includes in tuning.h until we wire a real implementation.

namespace Tunings {
class Tuning {
public:
    // Minimal interface used by Dexed msfa code paths we compile.
    // Extend as needed when we add actual tuning support.
    Tuning() = default;
};
}
