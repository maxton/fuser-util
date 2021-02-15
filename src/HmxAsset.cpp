#include "HmxAsset.h"

#include "stream-helpers.h"

ResourceFile LoadResource(std::istream& stream) {
  auto unk1 = read<int32_t>(stream);
  auto filename = read_ue4text(stream);
  auto unk2 = read<int32_t>(stream);
  auto type = read_ue4text(stream);
  uint64_t size = read<uint64_t>(stream);
  if (size > SIZE_MAX)
    throw std::exception("Resource was way too big.");
  auto data = std::string((size_t)size, '\0');
  stream.read(data.data(), data.size());
  return {unk1, filename, unk2, type, data};
}

constexpr int SUPPORTED_VERSION = 7;
HmxAsset HmxAsset::LoadAsset(std::istream& stream) {
  HmxAsset asset;
  asset.version_ = read<uint64_t>(stream);
  if (asset.version_ != SUPPORTED_VERSION)
    throw std::exception("Unsupported asset type");
  asset.subtype_ = (AssetSubtype)read<uint64_t>(stream);
  if (asset.subtype_ != AssetSubtype::Patch_Loop && asset.subtype_ != AssetSubtype::Patch_Transition)
    throw std::exception("Unrecognized asset subtype :( Try using a _mid.uexp or _fusion.uexp!");
  asset.unk_3_ = read<int32_t>(stream);
  asset.filename_hash_ = read<int64_t>(stream);
  asset.original_filename_ = read_ue4text(stream);
  asset.unk_4_ = read<int32_t>(stream);
  asset.unk_5_ = read<int32_t>(stream);
  int64_t num_files;
  if (asset.unk_4_ == 1 && asset.unk_5_ == 0) {
    num_files = 1;
  } else {
    num_files = read<int64_t>(stream);
  }
  for(int64_t i = 0; i < num_files; i++) {
    asset.files_.push_back(LoadResource(stream));
  }
  asset.magic_footer_ = read<uint32_t>(stream);
  if (asset.magic_footer_ != MAGIC) {
    throw std::exception("Unexpected asset footer bytes.");
  }
  return asset;
}

std::vector<const ResourceFile*> HmxAsset::GetResourcesOfType(const std::string& type) {
  std::vector<const ResourceFile*> ret;
  for(const auto& rf : files_) {
    if (rf.type == type) {
      ret.push_back(&rf);
    }
  }
  return ret;
}