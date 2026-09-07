# Input ownership and patch delivery

RtMidi publishes three-byte note, control-change and pitch-bend packets to a
1024-entry single-producer/single-consumer ring (1023 usable entries). SDL atomics
publish indices after packet writes and after consumer copies. The callback
does not read configuration, edit patterns, call synths, allocate, log or wait.
Malformed lengths and data bytes are rejected instead of becoming note-offs.

The application thread drains a bounded snapshot of the queue in normal input
processing and system-request loops. Each accepted note owns the tracker channel
assigned at note-on, indexed by the original MIDI channel and wire note. Changing
transpose, cursor, selected instrument or input filter does not change that
release address. A new input note invalidates the previous input owner of a
stolen tracker channel. Sustain defers release until pedal-up on that MIDI
channel; pedal-up remains accepted after filter changes.

Closing the selected device disables publication, closes the RtMidi port, flushes
the queue, and releases its input-owned notes without recording release edits.
Queue overflow latches a recovery condition: the producer stops publishing until
the consumer discards pending packets and releases input-owned notes. This can
interrupt an audition under overload, but does not strand a lost note-off.
Physical unplug detection without a backend close notification remains unverified.

Dispatch is currently tied to the UI loop. At 60 Hz this adds up to approximately
one frame of scheduling delay under normal load; a blocked application thread can
delay it longer. Sample-accurate scheduling and device latency measurements remain
release work. Main-thread ownership also does not eliminate the need to synchronize
tracker/audio state or distinguish input notes from subsequent sequencer retriggers.

Dexed patch writers serialize edits under their existing mutex and publish 39
atomic 32-bit words between odd/even revision changes. Sequentially consistent
access prevents acceptance of a torn snapshot. Render/note-start attempt one read:
if a writer overlaps, the existing render patch remains active until a later call.
These readers never retry, wait on the writer, or allocate patch storage. New voices
initialize after snapshot adoption, preserving their attack envelopes. Updates
between reads coalesce to the latest patch rather than accumulating in a queue.
This fixes reader-side delivery; automation that calls a patch writer from the
audio thread still needs a separate nonblocking path.

Regression coverage lives in `tests/midi_tests.inc` and `tests/dexed_tests.cpp` and
runs through the existing stability CTest. It includes concurrent MIDI publication,
overflow recovery, channel/filter/transpose changes, sustain, malformed packets,
device close, concurrent patch publication, and immediate-versus-primed attacks.
