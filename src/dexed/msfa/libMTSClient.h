#pragma once
// Minimal stub for MTS-ESP client header to satisfy Dexed msfa build without JUCE.
// We are not using MTS in this embedded build.
struct MTSClient {};
inline MTSClient* MTS_RegisterClient(void*, void*) { return nullptr; }
inline void MTS_DeregisterClient(MTSClient*) {}
inline bool MTS_HasMaster(MTSClient*) { return false; }
inline double MTS_NoteToFrequency(MTSClient*, int /*note*/, double baseHz) { return baseHz; }
