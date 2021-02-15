#include <filesystem>
#include <fstream>
#include <sstream>

#include "Data.h"
#include "HmxAsset.h"
#include "MidiFileResource.h"
#include "SMF.h"
#include "mogg/VorbisEncrypter.h"
#include "mogg/CCallbacks.h"

#define VERSION "0.5"

int doMidi(std::ifstream& file){
  try {
    auto midi = MidiFile::ReadMidi(file);
    printf("Format: %d, Duration: %f, TPQN: %d, %d tracks.\n",
      midi.format(),
      midi.duration(),
      midi.ticks_per_qn(),
      midi.tracks().size());
    for(const auto& track : midi.tracks()) {
      printf("Track ticks: %lld # events: %d name: %s\n",
        track.total_ticks,
        track.events.size(),
        track.name.c_str());
    }
    return 0;
  } catch (const std::exception& ex) {
    printf("Could not read midi: %s\n", ex.what());
    return -1;
  }
}

int doMidiCopy(std::ifstream& file, const char* out) {
  try {
    auto midi = MidiFile::ReadMidi(file);
    std::ofstream outfile(out, std::ios::out | std::ios::binary);
    if (!outfile.is_open()) {
      printf("Could not open output file\n");
      return 1;
    }
    midi.WriteMidi(outfile);
    printf("Wrote output to %s\n", out);
    return 0;
  } catch (const std::exception& ex) {
    printf("Could not copy midi: %s\n", ex.what());
    return 1;
  }
}

int doMidiFileResource(std::ifstream& file) {
  try {
    auto midi = MidiFileResource::Deserialize(file);
    printf("Format: %d, %d tracks.\n",
      midi.magic_,
      midi.tracks_.size());
    for(const auto& wtrack : midi.tracks_) {
      printf("Track ticks: %lld # events: %d name: %s\n",
        wtrack.track.total_ticks,
        wtrack.track.events.size(),
        wtrack.track.name.c_str());
    }
    for(const auto& chord : midi.chords_) {
      printf("Chord name: %s start: %d end: %d\n", chord.name.c_str(), chord.start, chord.end);
    }
    return 0;
  } catch (const std::exception& ex) {
    printf("Could not read midi: %s\n", ex.what());
    return -1;
  }
}

int doMidiFileResourceCopy(std::ifstream& file, const char* out) {
  try {
    auto midi = MidiFileResource::Deserialize(file);
    std::ofstream outfile(out, std::ios::out | std::ios::binary);
    if (!outfile.is_open()) {
      printf("Could not open output file\n");
      return 1;
    }
    midi.Serialize(outfile);
    printf("Wrote output to %s\n", out);
    return 0;
  } catch (const std::exception& ex) {
    printf("Could not copy MidiFileResource: %s\n", ex.what());
    return 1;
  }
}

int doMidiFileResourceConvert(std::ifstream& file, const char* out) {
  try {
    auto midi = MidiFile::ReadMidi(file);
    std::ofstream outfile(out, std::ios::out | std::ios::binary);
    if (!outfile.is_open()) {
      printf("Could not open output file\n");
      return 1;
    }
    MidiFileResource mfr = MidiFileResource::FromMidi(midi);
    mfr.Serialize(outfile);
    printf("Wrote output to %s\n", out);
    return 0;
  } catch (const std::exception& ex) {
    printf("Could not convert midi: %s\n", ex.what());
    return 1;
  }
}

int doMidiFileResourceExtract(std::ifstream& file, const char* out) {
  try {
    auto midi = MidiFileResource::Deserialize(file);
    std::ofstream outfile(out, std::ios::out | std::ios::binary);
    if (!outfile.is_open()) {
      printf("Could not open output file\n");
      return 1;
    }
    MidiFile mf = midi.ExtractMidi();
    mf.WriteMidi(outfile);
    printf("Wrote output to %s\n", out);
    return 0;
  } catch (const std::exception& ex) {
    printf("Could not extract midi: %s\n", ex.what());
    return 1;
  }
}

int doUexp(std::ifstream& file) {
  try {
    auto uexp = HmxAsset::LoadAsset(file);
    int i = 1;
    for(const auto& resource : uexp.files_) {
      printf("%2d %s: %s\n",
        i++,
        resource.type.c_str(),
        resource.filename.c_str());
    }
    return 0;
  } catch (const std::exception& ex) {
    printf("Could not read uexp: %s\n", ex.what());
    return -1;
  }
}

int doExtractUexp(std::ifstream& file, char* path) {
  try {
    auto uexp = HmxAsset::LoadAsset(file);
    auto resources = uexp.GetResourcesOfType("MidiFileResource");
    if (resources.size() == 0) {
      printf("There are no MidiFileResources in the uexp file.");
      return -1;
    }
    std::filesystem::path uexp_path(path);
    auto dir = uexp_path.parent_path() / uexp_path.stem();
    std::filesystem::create_directory(dir);
    for(auto* resource : resources) {
      auto last_slash = resource->filename.find_last_of('/');
      auto last_backslash = resource->filename.find_last_of('\\');
      auto resource_path = dir / resource->filename.substr((last_slash == std::string::npos ? last_backslash : last_slash) + 1);
      std::ofstream outfile(resource_path, std::ios::out | std::ios::binary);
      if (!outfile.is_open()) {
        printf("Could not open output file %s\n", resource_path.string().c_str());
        return 1;
      }
      outfile.write(resource->data.data(), resource->data.size());
      printf("Saved %s\n", resource_path.string().c_str());
    }
    return 0;
  } catch (const std::exception& ex) {
    printf("Could not read uexp: %s\n", ex.what());
    return -1;
  }
}

int doDta(std::ifstream& file) {
  try {
    auto root = DataReadStream(file);
    root->Print(std::cout);
    std::cout << std::endl;
    return 0;
  } catch (const std::exception& ex) {
    printf("Could not read dta: %s\n", ex.what());
    return -1;
  }
}
int doDtb(std::ifstream& file) {
  try {
    DataArray root;
    root.Load(file);
    root.Print(std::cout);
    std::cout << std::endl;
    return 0;
  } catch (const std::exception& ex) {
    printf("Could not read dtb: %s\n", ex.what());
    return -1;
  }
}
int doDta2Dtb(std::ifstream& file, const char* out) {
  try {
    auto root = DataReadStream(file);
    std::ofstream outfile(out, std::ios::out | std::ios::binary);
    if (!outfile.is_open()) {
      printf("Could not open output file\n");
      return 1;
    }
    root->Save(outfile);
    printf("Wrote output to %s\n", out);
    return 0;
  }
  catch (const std::exception& ex) {
    printf("Could not read dta: %s\n", ex.what());
    return -1;
  }
}
int doDtb2Dta(std::ifstream& file, const char* out) {
  try {
    DataArray root;
    root.Load(file);
    std::ofstream outfile(out, std::ios::out | std::ios::binary);
    if (!outfile.is_open()) {
      printf("Could not open output file\n");
      return 1;
    }
    root.Print(outfile);
    printf("Wrote output to %s\n", out);
    return 0;
  }
  catch (const std::exception& ex) {
    printf("Could not read dtb: %s\n", ex.what());
    return -1;
  }
}

int doOgg2Mogg(std::ifstream& file, const char* out) {
	std::ofstream outfile(out, std::ios::out | std::ios::binary);
	if (!outfile.is_open()) {
		puts("Could not open output file");
    return 1;
	}
	try {
		VorbisEncrypter ve(&file, 0x10, cppCallbacks<std::ifstream>);
    printf("Sample rate: %u\n", ve.sample_rate);
    printf("Mogg size: %zd bytes\n", ve.LengthRaw());
		char buf[8192];
		size_t read = 0;
		do {
			read = ve.ReadRaw(buf, 1, 8192);
			outfile.write(buf, read);
		} while (read != 0);
    return 0;
	} catch(std::exception& e) {
		printf("Error: %s\n", e.what());
		return 1;
	}
}

int doConsole() {
  while(1) {
    try {
      std::cout << "> " << std::flush;
      std::string buf;
      std::getline(std::cin, buf);
      if (buf.length() == 0)
        continue;
      std::istringstream iss{buf};
      auto root = DataReadStream(iss);
      for (const auto& node : root->nodes()) {
        node.Evaluate().Print(std::cout, 0);
        std::cout << std::endl;
      }
    } catch (const std::exception& ex) {
      std::cout << "ERROR! Message: " << ex.what() << std::endl;
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    usage:
    puts("fuser-util v" VERSION);
    printf("Usage: %s <verb> <input file> [<output file>]\n", argv[0]);
    puts(" convert : Convert a mid to a MidiFileResource");
    puts(" extract : Extracts the midi from a MidiFileResource");
    puts(" mid/mfr : Show some debug info for a mid or MidiFileResource");
    puts(" midcopy : Copy a mid to a mid (tests that serialization/deserialization is OK)");
    puts(" mfrcopy : Copy a MidiFileResource to a MidiFileResource (tests that serialization/deserialization is OK)");
    puts(" uexp    : Print debug info about a .uexp");
    puts(" uexp_ex : Extract the MidiFileResources from a .uexp if it contains HmxMidiFileAssets.");
    puts(" dtb     : Print debug info about a dtb.");
    puts(" dta     : Print debug info about a dta.");
    puts(" dta2dtb : Serialize data for FUSER midisongs.");
    puts(" dtb2dta : Deserialize FUSER midisong array.");
    puts(" ogg2mogg: Encrypt and map an ogg vorbis file to a mogg file.");
    puts(" console : Start a basic Data REPL.");
    return 1;
  }
  DataInitFuncs();

  // 0-file actions
  if (!strcmp("console", argv[1])) {
    return doConsole();
  } else if (argc < 3) {
    goto usage;
  }

  // 1-file actions
  auto file = std::ifstream(argv[2], std::ios::binary | std::ios::in);
  if (!file.is_open()){
    printf("Could not open file %s\n", argv[2]);
    return 1;
  } else if (!strcmp("mid", argv[1])) {
    return doMidi(file);
  } else if (!strcmp("mfr", argv[1])) {
    return doMidiFileResource(file);
  } else if (!strcmp("uexp", argv[1])) {
    return doUexp(file);
  } else if (!strcmp("dtb", argv[1])) {
    return doDtb(file);
  } else if (!strcmp("dta", argv[1])) {
    return doDta(file);
  }
  // 2-file actions
  else if (!strcmp("uexp_ex", argv[1]) && argc > 3) {
    return doExtractUexp(file, argv[2]);
  } else if (!strcmp("mfrcopy", argv[1]) && argc > 3) {
    return doMidiFileResourceCopy(file, argv[3]);
  } else if (!strcmp("midcopy", argv[1]) && argc > 3) {
    return doMidiCopy(file, argv[3]);
  } else if (!strcmp("convert", argv[1]) && argc > 3) {
    return doMidiFileResourceConvert(file, argv[3]);
  } else if (!strcmp("extract", argv[1]) && argc > 3) {
    return doMidiFileResourceExtract(file, argv[3]);
  } else if (!strcmp("dta2dtb", argv[1]) && argc > 3) {
    return doDta2Dtb(file, argv[3]);
  } else if (!strcmp("dtb2dta", argv[1]) && argc > 3) {
    return doDtb2Dta(file, argv[3]);
  } else if (!strcmp("ogg2mogg", argv[1]) && argc > 3) {
    return doOgg2Mogg(file, argv[3]);
  } else {
    goto usage;
  }
}