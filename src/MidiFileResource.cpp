#include "MidiFileResource.h"

#include <cmath>
#include <sstream>

#include "stream-helpers.h"
constexpr int TICKS_PER_QN = 480;

MidiFileResource MidiFileResource::FromMidi(MidiFile& mf) {
  MidiFileResource mfr;
  if (mf.ticks_per_qn() != TICKS_PER_QN) {
    throw std::exception("Midi must use 480 ticks per quarter note");
  }
  mfr.magic_ = 2;
  mfr.last_track_final_tick_ = (uint32_t)mf.tracks().back().total_ticks;
  tick_t final_tick = 0;
  for (const auto& track : mf.tracks()) {
    mfr.tracks_.emplace_back(track.name == "samplemidi" ? 0 : -1, track);
    mfr.track_names_.push_back(track.name);
    if (track.total_ticks > final_tick)
      final_tick = (tick_t)track.total_ticks;
    if (track.name == "chords") {
      tick_t current_tick = 0;
      for (auto& event : track.events) {
        current_tick += event.delta_time;
        if (event.type != EventType::Meta) continue;
        auto meta = std::get<MetaEvent>(event.inner_event);
        if (meta.type == MetaEventType::Text) {
          if (mfr.chords_.size() > 0) {
            mfr.chords_.back().end = current_tick - 1;
          }
          mfr.chords_.emplace_back(std::get<std::string>(meta.event), current_tick, -1);
        }
      }
    }
  }

  std::vector<uint32_t> measure_ticks;
  measure_ticks.push_back(0);
  auto last_time_sig = mf.tempo_timesig_map()[0];
  int measure = 0;
  for (auto& tempo : mf.tempo_timesig_map())
  {
    if (tempo.new_tempo)
      mfr.tempos_.emplace_back(
        (float)(tempo.time * 1000.0),
        (tick_t)tempo.tick,
        (int32_t)(60000000 / (float)tempo.bpm));
    if (tempo.new_time_sig)
    {
      if (tempo.tick > 0)
      {
        auto elapsed = tempo.tick - last_time_sig.tick;
        auto ticksPerBeat = (TICKS_PER_QN * 4) / last_time_sig.denominator;
        measure += (int)(elapsed / ticksPerBeat / last_time_sig.numerator);
        auto last_measure_tick = measure_ticks.back();
        for (int i = measure_ticks.size(); i < measure; i++)
        {
          last_measure_tick += TICKS_PER_QN * last_time_sig.numerator * 4 / last_time_sig.denominator;
          measure_ticks.push_back(last_measure_tick);
        }
      }
      mfr.time_sigs_.emplace_back(measure, (tick_t)tempo.tick, tempo.numerator, tempo.denominator);
      last_time_sig = tempo;
    }
  };
  uint32_t last_timesig_ticks_per_measure = TICKS_PER_QN * last_time_sig.numerator * 4 / last_time_sig.denominator;
  for (uint32_t last_measure_tick2 = measure_ticks.back() + last_timesig_ticks_per_measure;
      last_measure_tick2 < final_tick;
      last_measure_tick2 += last_timesig_ticks_per_measure) {
    measure_ticks.push_back(last_measure_tick2);
  }
  mfr.fuser_revision_ = 2;
  mfr.measures_ = measure_ticks.size();
  mfr.final_tick_ = final_tick;
  mfr.unknown_ints_ = {0, 0, 0, 0, 0, 0};
  mfr.final_tick_minus_one_ = mfr.final_tick_ - 1;
  mfr.unknown_floats_ = { -1.f, -1.f, -1.f, -1.f };
  mfr.unknown_zero_ = 0;
  
  mfr.fuser_revision_2_ = mfr.chords_.size() == 0 ? -1 : 2;
  // BEATS would go here, but Fuser doesn't have beats?
  // song sections would go here, but fuser doesn't have song sections?
  return mfr;
}

MidiFile MidiFileResource::ExtractMidi() const {
  std::vector<MidiTrack> tracks;
  for(const auto& wtrack : tracks_) {
    tracks.push_back(wtrack.track);
  }
  return MidiFile(MidiFormat::MultiTrack, tracks, TICKS_PER_QN);
}

MidiFileResource::Tempo ReadTempo(std::istream& stream) {
  return {read<float>(stream), read<tick_t>(stream), read<int32_t>(stream)};
}
MidiFileResource::TimeSig ReadTimeSig(std::istream& stream) {
  return {read<int32_t>(stream), read<tick_t>(stream), read<int16_t>(stream), read<int16_t>(stream)};
}
MidiFileResource::Beat ReadBeat(std::istream& stream) {
  return {read<tick_t>(stream), (bool)read<uint8_t>(stream)};
}
MidiFileResource::Chord ReadChord(std::istream& stream) {
  return {read_symbol(stream), read<tick_t>(stream), read<tick_t>(stream)};
}
enum class HmxEventType : uint8_t {
  Midi = 1,
  Tempo = 2,
  TimeSignature = 4,
  Meta = 8
};
TrackEvent ReadEvent(
    std::istream& stream,
    uint32_t& midi_tick,
    std::string& track_name,
    std::vector<std::string>& track_strings) {
  auto tick = read<uint32_t>(stream);
  auto deltaTime = tick - midi_tick;
  midi_tick = tick;
  auto kind = read<uint8_t>(stream);
  switch ((HmxEventType)kind)
  {
    // Midi Messages
    case HmxEventType::Midi: {
      uint8_t tc = read<uint8_t>(stream);
      uint8_t channel = tc & 0x0F;
      EventType type = (EventType)(tc & 0xF0);
      uint8_t note = read<uint8_t>(stream);
      uint8_t velocity = read<uint8_t>(stream);
      switch (type)
      {
        case EventType::NoteOff:
          return TrackEvent{deltaTime, type, MidiEvent{false, channel, {note, velocity}}};
        case EventType::NoteOn:
          return TrackEvent{deltaTime, type, MidiEvent{false, channel, {note, velocity}}};
        case EventType::Controller: // seen in touchofgrey and others, assuming ctrl chg
          return TrackEvent{deltaTime, type, MidiEvent{false, channel, {note, velocity}}};
        case EventType::ProgramChange: // seen in foreplaylongtime, assuming prgmchg
          return TrackEvent{deltaTime, type, MidiEvent{false, channel, note}};
        case EventType::ChannelPressure: // seen in huckleberrycrumble, assuming channel pressure
          return TrackEvent{deltaTime, type, MidiEvent{false, channel, note}};
        case EventType::PitchBend: // seen in theballadofirahayes, assuming pitch bend
          return TrackEvent{deltaTime, type, MidiEvent{.force_status = false, .channel = channel, .bend = (uint16_t)(note | (velocity << 8))}};
        default:
          // Can't be too sure about whether the types are 1:1
          throw std::exception("Unknown midi message type encountered");
      }
    }
    case HmxEventType::Tempo: {
      uint32_t tempo_msb = read<uint8_t>(stream);
      uint32_t tempo_lsb = read<uint16_t>(stream);
      return TrackEvent{deltaTime, EventType::Meta, MetaEvent(MetaEventType::TempoEvent, tempo_msb << 16 | tempo_lsb)};
    }
    case HmxEventType::TimeSignature: {
      auto num = read<uint8_t>(stream);
      auto denom = read<uint8_t>(stream);
      auto denom_pow2 = (uint8_t)log2(denom);
      stream.get(); // skip 1 byte?
      return TrackEvent{deltaTime, EventType::Meta, MetaEvent(MetaEventType::TimeSignature, TimeSignatureEvent{num, denom_pow2, 24, 8})};
    }
    case HmxEventType::Meta: {
      auto ttype = read<uint8_t>(stream);
      auto string_index = read<uint16_t>(stream);
      MetaEventType t = (MetaEventType)ttype;
      if (t < MetaEventType::Text || t > MetaEventType::CuePoint) {
        throw std::exception("Invalid text event type");
      }
      if (t == MetaEventType::TrackName) {
        track_name = track_strings[string_index];
      }
      return TrackEvent{deltaTime, EventType::Meta, MetaEvent(t, track_strings[string_index])};
    }
    default:
      throw std::exception("Unknown midi track event");
  }
}

MidiFileResource::TrackWrapper ReadMidiTrack(std::istream& stream) {
  auto unk = read<uint8_t>(stream);
  auto unk2 = read<int32_t>(stream);
  auto num_events = read<uint32_t>(stream);

  std::string events_data(num_events * 8, '\0');
  stream.read(events_data.data(), num_events * 8ULL);
  std::vector<std::string> track_strings;
  read_vector<std::string>(stream, track_strings, read_symbol);
  std::stringstream ss(events_data);

  uint32_t midi_tick = 0;
  std::string track_name = "";
  std::vector<TrackEvent> events;
  for(auto i = 0u; i < num_events; i++) {
    events.push_back(ReadEvent(ss, midi_tick, track_name, track_strings));
  }
  events.emplace_back(0, EventType::Meta, MetaEvent(MetaEventType::EndOfTrack));
  return {unk2, {track_name, midi_tick, events}};
}

MidiFileResource MidiFileResource::Deserialize(std::istream& stream) {
  MidiFileResource r;
  r.magic_ = read<int32_t>(stream);
  if (r.magic_ != 2) {
    throw std::exception("Only MidiFileResource rev 2 is supported");
  }
  r.last_track_final_tick_ = read<uint32_t>(stream);
  read_vector<TrackWrapper>(stream, r.tracks_, ReadMidiTrack);
  auto finalTickOrRev = read<uint32_t>(stream);
  if (finalTickOrRev == 0x56455223) { // '#REV'
    r.fuser_revision_ = read<int32_t>(stream);
    r.final_tick_ = read<uint32_t>(stream);
  } else {
    r.fuser_revision_ = 0;
    r.final_tick_ = finalTickOrRev;
  }
  if (r.tracks_.size() > 0) {
    // Hack: It seems like we need some way to preserve the final_tick for round-trip conversion
    // to work. The original midi's end-of-track events are not saved, so the best we can do is
    // set the first track's end-of-track event to the final tick.
    auto& track = r.tracks_[0].track;
    track.events.back().delta_time = (uint32_t)(r.final_tick_ - track.total_ticks);
    track.total_ticks = r.final_tick_;
  }
  r.measures_ = read<uint32_t>(stream);
  read_array<uint32_t>(stream, r.unknown_ints_.data(), r.unknown_ints_.size(), read<uint32_t>);
  r.final_tick_minus_one_ = read<uint32_t>(stream);
  read_array<float>(stream, r.unknown_floats_.data(), r.unknown_floats_.size(), read<float>);
  read_vector<Tempo>(stream, r.tempos_, ReadTempo);
  read_vector<TimeSig>(stream, r.time_sigs_, ReadTimeSig);
  read_vector<Beat>(stream, r.beats_, ReadBeat);
  r.unknown_zero_ = read<int32_t>(stream);
  if (r.fuser_revision_ > 1)
  {
    r.fuser_revision_2_ = read<int32_t>(stream);
    read_vector<Chord>(stream, r.chords_, ReadChord);
  }
  read_vector<std::string>(stream, r.track_names_, read_symbol);
  return r;
}
void WriteTempo(std::ostream& stream, const MidiFileResource::Tempo& tempo) {
  write(stream, tempo.start_millis);
  write(stream, tempo.start_ticks);
  write(stream, tempo.tempo);
}
void WriteTimeSig(std::ostream& stream, const MidiFileResource::TimeSig& ts) {
  write(stream, ts.measure);
  write(stream, ts.tick);
  write(stream, ts.numerator);
  write(stream, ts.denominator);
}
void WriteBeat(std::ostream& stream, const MidiFileResource::Beat& beat) {
  write(stream, beat.tick);
  write(stream, (uint8_t)beat.downbeat);
}
void WriteChord(std::ostream& stream, const MidiFileResource::Chord& chord) {
  write_symbol(stream, chord.name);
  write(stream, chord.start);
  write(stream, chord.end);
}

void WriteTrack(std::ostream& stream, const MidiFileResource::TrackWrapper& wrapper) {
  const auto& track = wrapper.track;
  write<uint8_t>(stream, 1);
  write(stream, wrapper.unk);
  
  // Subtract 1 for the end-of-track event
  write(stream, track.events.size() - 1);
  std::vector<std::string> track_strings;
  uint32_t ticks = 0;
  for (const auto& event : track.events) {
    uint8_t kind, d1, d2, d3;
    if(event.type >= EventType::NoteOff && event.type <= EventType::PitchBend) {
      auto midi_event = std::get<MidiEvent>(event.inner_event);
      kind = 1;
      d1 = (uint8_t)event.type | midi_event.channel;
      switch (event.type) {
        case EventType::NoteOff:
        case EventType::NoteOn:
          d2 = midi_event.note.key;
          d3 = midi_event.note.velocity;
          break;
        case EventType::Controller:
          d2 = midi_event.controller.controller;
          d3 = midi_event.controller.value;
          break;
        case EventType::ProgramChange:
          d2 = midi_event.program;
          d3 = 0;
          break;
        case EventType::ChannelPressure:
          d2 = midi_event.pressure;
          d3 = 0;
          break;
        case EventType::PitchBend:
          d2 = midi_event.bend & 0xFF;
          d3 = midi_event.bend >> 8;
          break;
      }
    }
    else if(event.type == EventType::Meta) {
      auto meta = std::get<MetaEvent>(event.inner_event);
      if (meta.type == MetaEventType::TempoEvent) {
        kind = 2;
        uint32_t tempo = std::get<uint32_t>(meta.event);
        d1 = (tempo >> 16) & 0xFF;
        d2 = tempo & 0xFF;
        d3 = (tempo >> 8) & 0xFF;
      } else if (meta.type == MetaEventType::TimeSignature) {
        kind = 4;
        auto ts = std::get<TimeSignatureEvent>(meta.event);
        d1 = ts.numerator;
        d2 = 1 << ts.denominator;
        d3 = 0;
      } else if (meta.type >= MetaEventType::Text && meta.type <= MetaEventType::CuePoint) {
        uint16_t idx = (uint16_t)track_strings.size();
        track_strings.push_back(std::get<std::string>(meta.event));
        kind = 8;
        d1 = (uint8_t)meta.type;
        d2 = idx & 0xff;
        d3 = idx >> 8;
      } else if (meta.type == MetaEventType::EndOfTrack) {
        // MidiFileResource does not save end-of-track events.
        continue;
      } else {
        throw std::exception("Unhandled meta event type");
      }
    } else {
      throw std::exception("Unhandled event type");
    }
    ticks += event.delta_time;
    write(stream, ticks);
    write(stream, kind);
    write(stream, d1);
    write(stream, d2);
    write(stream, d3);
  }
  write_vector<std::string>(stream, track_strings, write_symbol);
}

void MidiFileResource::Serialize(std::ostream& stream) const {
  write(stream, magic_);
  write(stream, last_track_final_tick_);
  write_vector<TrackWrapper>(stream, tracks_, WriteTrack);
  if (fuser_revision_ != 0) {
    write(stream, 0x56455223);
    write(stream, fuser_revision_);
  }
  write(stream, final_tick_);
  write(stream, measures_);
  write_array<uint32_t>(stream, unknown_ints_.data(), unknown_ints_.size(), write<uint32_t>);
  write(stream, final_tick_minus_one_);
  write_array<float>(stream, unknown_floats_.data(), unknown_floats_.size(), write<float>);
  write_vector<Tempo>(stream, tempos_, WriteTempo);
  write_vector<TimeSig>(stream, time_sigs_, WriteTimeSig);
  write_vector<Beat>(stream, beats_, WriteBeat);
  write(stream, unknown_zero_);
  if (fuser_revision_ != 0) {
    write(stream, fuser_revision_2_);
    write_vector<Chord>(stream, chords_, WriteChord);
  }
  write_vector<std::string>(stream, track_names_, write_symbol);
}