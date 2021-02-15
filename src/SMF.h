#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Oh, how I wish C++ had sum types. std::variant<...> will have to do.

struct MidiTrack;
struct TimeSigTempoEvent;
struct TrackEvent;

enum class MidiFormat : uint16_t {
  // One track
  SingleTrack = 0x0,
  // Multiple simultaneous tracks
  MultiTrack = 0x1,
  // Multiple sequential stracks
  MultiSequence = 0x2
};

class MidiFile {
public:
  // Attempts to read a standard Midi file from the given stream.
  // Throws an exception if there's an issue.
  static MidiFile ReadMidi(std::istream& stream);
  void WriteMidi(std::ostream& stream);

  MidiFile(MidiFormat format, std::vector<MidiTrack>& tracks, uint16_t ticks_per_qn)
    : format_(format), tracks_(tracks), ticks_per_qn_(ticks_per_qn) {
    duration_ = ProcessTempoMap();
  }

  const MidiTrack* GetTrackByName(std::string& name) const;

  MidiFormat format() const { return format_; }
  double duration() const { return duration_; }
  uint16_t ticks_per_qn() const { return ticks_per_qn_; }
  const std::vector<TimeSigTempoEvent>& tempo_timesig_map() const { return tempo_timesig_map_; }
  const std::vector<MidiTrack>& tracks() const { return tracks_; }

private:
  MidiFormat format_;
  double duration_;
  std::vector<MidiTrack> tracks_;
  std::vector<TimeSigTempoEvent> tempo_timesig_map_;
  uint16_t ticks_per_qn_;
  // Process the tempo map, also calculate the duration of the file.
  double ProcessTempoMap();
};

struct TimeSigTempoEvent {
  // The time, in seconds, where this tempo change occurs.
  double time;
  // The MIDI tick at which this tempo change occurs.
  int64_t tick;
  // The tempo that follows this marker.
  double bpm;
  // True if this marker defines a new time signature.
  bool new_time_sig;
  // True if this marker defines a new tempo.
  bool new_tempo;
  // The numerator of the time signature, if this marker defines a new time signature.
  uint8_t numerator;
  // The denominator of the time signature, if this marker defines a new time signature.
  uint8_t denominator;
};

struct MidiTrack {
  std::string name;
  int64_t total_ticks;
  std::vector<TrackEvent> events;
};

enum class EventType : uint8_t {
  NoteOff = 0x80,
  NoteOn = 0x90,
  NotePresure = 0xA0,
  Controller = 0xB0,
  ProgramChange = 0xC0,
  ChannelPressure = 0xD0,
  PitchBend = 0xE0,
  Meta = 0xFF,
  Sysex = 0xF0,
  SysexRaw = 0xF7
};

enum class MetaEventType : uint8_t {
  SequenceNumber = 0x00,
  Text = 0x01,
  CopyrightNotice = 0x02,
  TrackName = 0x03,
  InstrumentName = 0x04,
  Lyric = 0x05,
  Marker = 0x06,
  CuePoint = 0x07,
  ProgramName = 0x08,
  DeviceName = 0x09,
  ChannelPrefix = 0x20,
  Port = 0x21,
  EndOfTrack = 0x2F,
  TempoEvent = 0x51,
  SmpteOffset = 0x54,
  TimeSignature = 0x58,
  KeySignature = 0x59,
  SequencerSpecific = 0x7F
};
inline bool IsTextEvent(MetaEventType t) { return t >= MetaEventType::Text && t <= MetaEventType::DeviceName; }

typedef struct {
  uint8_t h;
  uint8_t m;
  uint8_t s;
  uint8_t f;
  uint8_t frame_hundredths;
} SmpteOffsetEvent;

typedef struct {
  uint8_t numerator;
  uint8_t denominator;
  uint8_t clocks_per_tick;
  uint8_t thirtysecond_notes_per_24_clocks;
} TimeSignatureEvent;

typedef struct {
  uint8_t sharps;
  uint8_t tonality;
} KeySignatureEvent;

struct MetaEvent {
  // Sequence Number event
  MetaEvent(MetaEventType t, uint16_t seqnum) : type(t), event(seqnum) {
    _ASSERT(type == MetaEventType::SequenceNumber);
  }
  // Text events
  MetaEvent(MetaEventType t, std::string str) : type(t), event(str) {
    _ASSERT(IsTextEvent(t));
  }
  // Channel prefix, port event
  MetaEvent(MetaEventType t, uint8_t channel): type(t), event(channel) {
    _ASSERT(type == MetaEventType::ChannelPrefix || type == MetaEventType::Port);
  }
  // End of Track
  MetaEvent(MetaEventType t) : type(t) {
    _ASSERT(type == MetaEventType::EndOfTrack);
  }
  // Tempo event
  MetaEvent(MetaEventType t, uint32_t microsPerQn) : type(t), event(microsPerQn) {
    _ASSERT(type == MetaEventType::TempoEvent);
  }
  // SMPTE Offset event
  MetaEvent(MetaEventType t, SmpteOffsetEvent offset) : type(t), event(offset) {
    _ASSERT(type == MetaEventType::SmpteOffset);
  }
  // Time signature event
  MetaEvent(MetaEventType t, TimeSignatureEvent ts) : type(t), event(ts) {
    _ASSERT(type == MetaEventType::TimeSignature);
  }
  // Key signature event
  MetaEvent(MetaEventType t, KeySignatureEvent ks) : type(t), event(ks) {
    _ASSERT(type == MetaEventType::KeySignature);
  }
  // Sequencer specific event
  MetaEvent(MetaEventType t, std::vector<uint8_t> data) : type(t), event(data) {
    _ASSERT(type == MetaEventType::SequencerSpecific);
  }
  MetaEventType type;
  std::variant<
    uint16_t, // sequence_number
    std::string, // text_event
    uint8_t, // channel, port
    uint32_t, // micros_per_qn
    SmpteOffsetEvent, // smpte_offset
    TimeSignatureEvent, // time_signature
    KeySignatureEvent, // key_signature
    std::vector<uint8_t> // sequencer_specific_data
  > event;
};

struct MidiEvent {
  // For byte-for-byte rewriting of MIDI files, set this to true if we expected
  // to use running status but a status byte was provided anyway.
  bool force_status{false};
  uint8_t channel;
  union {
    struct {
      uint8_t key;
      union {
        uint8_t velocity;
        uint8_t pressure;
      };
    } note;
    struct {
      uint8_t controller;
      uint8_t value;
    } controller;
    uint8_t program;
    uint8_t pressure;
    uint16_t bend;
  };
};

struct SysexEvent {
  std::vector<uint8_t> data;
};

struct TrackEvent {
  uint32_t delta_time;
  EventType type;
  std::variant<MidiEvent, MetaEvent, SysexEvent> inner_event;
};