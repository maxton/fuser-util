#pragma once

#include <stdint.h>
#include <iostream>
#include <vector>

struct ResourceFile;

// ??? Seems like 
enum class AssetSubtype : uint64_t {
  // Seen in trans_*_mid and trans_*_fusion
  Patch_Transition = 6,
  // Seen in *_mid and *_fusion
  Patch_Loop = 7,
  // Seen in trans_midisong
  Midisong_Transition = 0xC,
  // Seen in *_midisong
  Midisong_Loop = 0xE,
};
constexpr uint32_t MAGIC = 0x9E2A83C1;
class HmxAsset {
public:
  // Loads an asset and its resources entirely into memory.
  static HmxAsset LoadAsset(std::istream& stream);
  std::vector<const ResourceFile*> GetResourcesOfType(const std::string& type);
  uint64_t version_{};
  AssetSubtype subtype_{};
  int32_t unk_3_{};
  int64_t filename_hash_{};
  std::string original_filename_;
  int32_t unk_4_{};
  int32_t unk_5_{};
  std::vector<ResourceFile> files_;
  uint32_t magic_footer_{ MAGIC };
};

struct ResourceFile {
  int32_t unk_1;
  std::string filename;
  int32_t unk_2;
  std::string type;
  std::string data;
};