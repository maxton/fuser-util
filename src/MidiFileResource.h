#pragma once

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "SMF.h"

typedef uint32_t tick_t;

class MidiFileResource
{
public:
  static MidiFileResource Deserialize(std::istream& stream);
  static MidiFileResource FromMidi(MidiFile& midi);
  void Serialize(std::ostream& stream) const;
  MidiFile ExtractMidi() const;

  struct Tempo
  {
    float start_millis;
    tick_t start_ticks;
    int32_t tempo;
  };
  struct TimeSig
  {
    int32_t measure;
    tick_t tick;
    int16_t numerator;
    int16_t denominator;
  };
  struct Beat
  {
    tick_t tick;
    bool downbeat;
  };
  struct Chord
  {
    std::string name;
    tick_t start;
    tick_t end;
  };
  struct TrackWrapper
  {
    int32_t unk;
    MidiTrack track;
  };

  int32_t magic_{ 2 };
  uint32_t last_track_final_tick_{};
  std::vector<TrackWrapper> tracks_;

  int fuser_revision_{};
  uint32_t final_tick_{};
  uint32_t measures_{};
  std::array<uint32_t, 6> unknown_ints_{};
  uint32_t final_tick_minus_one_{};
  std::array<float, 4> unknown_floats_{};
  std::vector<Tempo> tempos_;
  std::vector<TimeSig> time_sigs_;
  std::vector<Beat> beats_;
  int32_t unknown_zero_{};
  int32_t fuser_revision_2_{};
  std::vector<Chord> chords_;
  std::vector<std::string> track_names_;
};