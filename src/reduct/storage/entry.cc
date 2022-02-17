// Copyright 2022 Alexey Timin

#include "reduct/storage/entry.h"

#include <fmt/core.h>
#include <google/protobuf/util/time_util.h>

#include <filesystem>
#include <fstream>
#include <ranges>

#include "reduct/core/logger.h"
#include "reduct/proto/storage/entry.pb.h"

namespace reduct::storage {

using core::Error;
using google::protobuf::util::TimeUtil;

auto to_time_t = IEntry::Time::clock::to_time_t;

namespace fs = std::filesystem;

class Entry : public IEntry {
 public:
  /**
   * Create a new entry
   * @param options
   */
  explicit Entry(Options options) : options_(std::move(options)) {
    full_path_ = options_.path / options_.name;
    if (fs::exists(full_path_)) {
      throw std::runtime_error(fmt::format("Initialization error: directory {} already exists", full_path_.string()));
    }

    fs::create_directories(full_path_);

    proto::EntrySettings settings;
    settings.set_max_block_size(options_.max_block_size);

    auto settings_path = full_path_ / kSettingsName;
    std::ofstream settings_file(settings_path, std::ios::binary);
    if (settings_file) {
      settings.SerializeToOstream(&settings_file);
    } else {
      throw std::runtime_error(
          fmt::format("Initialization error: Failed to save settings to {}", settings_path.string()));
    }

    descriptor_.mutable_created_at()->CopyFrom(TimeUtil::GetCurrentTime());
    descriptor_.set_size(0);

    current_block_ = descriptor_.add_blocks();
    current_block_->set_id(0);
    current_block_->set_size(0);

    auto descriptor_path = full_path_ / kDescriptorName;
    std::ofstream descriptor_file(descriptor_path, std::ios::binary);
    if (descriptor_file) {
      descriptor_.SerializeToOstream(&descriptor_file);
    } else {
      throw std::runtime_error(
          fmt::format("Initialization error: Failed to save descriptor to {}", descriptor_path.string()));
    }
  }

  /**
   * Restore entry from folder
   * @param full_path
   */
  explicit Entry(fs::path full_path) : full_path_(std::move(full_path)) {
    auto settings_path = full_path_ / kSettingsName;
    std::ifstream settings_file(settings_path, std::ios::binary);
    if (!settings_file) {
      throw std::runtime_error(
          fmt::format("Initialization error: Failed to open settings file: {}", settings_path.string()));
    }

    proto::EntrySettings settings;
    if (!settings.ParseFromIstream(&settings_file)) {
      throw std::runtime_error(
          fmt::format("Initialization error: Failed to parse settings from {}", settings_path.string()));
    }

    options_ = {.name = full_path_.filename().string(),
                .path = full_path_.parent_path(),
                .max_block_size = settings.max_block_size()};

    auto descriptor_path = full_path_ / kDescriptorName;
    std::ifstream descriptor_file(descriptor_path, std::ios::binary);
    if (!descriptor_file) {
      throw std::runtime_error(
          fmt::format("Initialization error: Failed to open descriptor file: {}", descriptor_path.string()));
    }

    if (!descriptor_.ParseFromIstream(&descriptor_file)) {
      throw std::runtime_error(
          fmt::format("Initialization error: Failed to parse descriptor from {}", descriptor_path.string()));
    }
    current_block_ = descriptor_.mutable_blocks(descriptor_.blocks_size() - 1);
  }

  [[nodiscard]] Error Write(std::string_view blob, const Time& time) override {
    enum class RecordType { kLatest, kBelated, kBelatedFirst };

    const auto proto_ts = FromTimePoint(time);

    auto block = current_block_;
    RecordType type = RecordType::kLatest;
    if (descriptor_.has_latest_record_time() && descriptor_.latest_record_time() >= proto_ts) {
      LOG_DEBUG("Timestamp {} is belated. Finding proper block", TimeUtil::ToString(proto_ts));

      if (descriptor_.oldest_record_time() > proto_ts) {
        LOG_DEBUG("Timestamp earlier than first record");
        block = descriptor_.mutable_blocks(0);
        type = RecordType::kBelatedFirst;
      } else {
        auto block_index = FindBlock(proto_ts);
        if (block_index == -1) {
          return {.code = 500, .message = "No proper block"};
        }

        block = descriptor_.mutable_blocks(block_index);
        type = RecordType::kBelated;
      }
    }

    if (!block->has_begin_time()) {
      LOG_DEBUG("First record_entry for current block");
      block->mutable_begin_time()->CopyFrom(proto_ts);
      if (block->id() == descriptor_.blocks(0).id()) {
        descriptor_.mutable_oldest_record_time()->CopyFrom(proto_ts);
      }
    }

    if (type == RecordType::kLatest && current_block_->size() > options_.max_block_size) {
      LOG_DEBUG("Block {} is full. Create a new one", current_block_->id());
      current_block_ = StartNextBlock(current_block_->latest_record_time());
      block = current_block_;
    }

    const auto block_path = full_path_ / fmt::format(kBlockNameFormat, block->id());

    LOG_DEBUG("Write a record_entry for ts={} ({} kB) to {}", TimeUtil::ToString(proto_ts), blob.size() / 1024,
              block_path.string());
    std::ofstream block_file(block_path, std::ios::app | std::ios::binary);
    if (!block_file) {
      return {.code = 500, .message = "Failed open a block_file for writing"};
    }

    proto::EntryRecord record_entry;
    record_entry.set_blob(std::string{blob});
    record_entry.mutable_meta_data()->Clear();

    std::string data;
    if (!record_entry.SerializeToString(&data)) {
      return {.code = 500, .message = "Failed write a record_entry to a block_file"};
    }

    LOG_TRACE("Record {} bytes to {}", data.size(), block_path.string());
    block_file << data;

    auto record = block->add_records();
    record->mutable_timestamp()->CopyFrom(proto_ts);
    record->set_begin(block->size());
    record->set_end(block->size() + data.size());

    block->set_size(block->size() + data.size());
    descriptor_.set_size(descriptor_.size() + data.size());

    switch (type) {
      case RecordType::kLatest:
        assert(current_block_ == block);
        current_block_->mutable_latest_record_time()->CopyFrom(proto_ts);
        descriptor_.mutable_latest_record_time()->CopyFrom(proto_ts);
        break;
      case RecordType::kBelatedFirst:
        block->mutable_begin_time()->CopyFrom(proto_ts);
        descriptor_.mutable_oldest_record_time()->CopyFrom(proto_ts);
        break;
      case RecordType::kBelated:
        break;
    }

    if (auto err = SaveDescriptor()) {
      return err;
    }
    return {};
  }

  [[nodiscard]] ReadResult Read(const Time& time) const override {
    const auto proto_ts = FromTimePoint(time);

    LOG_DEBUG("Read a record for ts={}", TimeUtil::ToString(proto_ts));

    if (!descriptor_.has_oldest_record_time() ||
        (proto_ts < descriptor_.oldest_record_time() || proto_ts > descriptor_.latest_record_time())) {
      return {{}, {.code = 404, .message = "No records for this timestamp"}, time};
    }

    int block_index = FindBlock(proto_ts);
    if (block_index == -1) {
      LOG_ERROR("No block in entry '{}' for ts={}", options_.name, TimeUtil::ToString(proto_ts));
      return {{}, {.code = 500, .message = "Failed to find the needed block in descriptor"}, time};
    }

    auto record_index = -1;
    const auto& block = descriptor_.blocks(block_index);
    for (int i = 0; i < block.records_size(); ++i) {
      const auto& current_record = block.records(i);
      if (current_record.timestamp() == proto_ts) {
        record_index = i;
        break;
      }
    }

    if (record_index == -1) {
      return {{}, {.code = 404, .message = "No records for this timestamp"}, time};
    }

    const auto block_path = full_path_ / fmt::format(kBlockNameFormat, block.id());
    LOG_DEBUG("Found block {} with needed record", block_path.string());

    std::ifstream block_file(block_path, std::ios::binary);
    if (!block_file) {
      return {{}, {.code = 500, .message = "Failed open a block for reading"}, time};
    }

    auto record = block.records(record_index);
    auto data_size = record.end() - record.begin();
    std::string data(data_size, '\0');
    block_file.seekg(record.begin());
    block_file.read(data.data(), data_size);

    proto::EntryRecord entry_record;
    if (!entry_record.ParseFromString(data)) {
      return {{}, {.code = 500, .message = "Failed parse a block"}, time};
    }

    return {entry_record.blob(), {}, time};
  }

  ListResult List(const Time& start, const Time& stop) const override {
    auto start_ts = FromTimePoint(start);
    auto stop_ts = FromTimePoint(stop);
    LOG_DEBUG("List records for interval: ({}, {})", TimeUtil::ToString(start_ts), TimeUtil::ToString(stop_ts));
    if (start_ts > stop_ts) {
      return {{}, {.code = 422, .message = "Start timestamp cannot be older stop timestamp"}};
    }

    if (!descriptor_.has_oldest_record_time()) {
      return {{}, {.code = 404, .message = "No records in the entry"}};
    }

    if (stop_ts < descriptor_.oldest_record_time() || start_ts > descriptor_.latest_record_time()) {
      return {{}, {.code = 404, .message = "No records for time interval"}};
    }

    int start_block_index = FindBlock(std::max(descriptor_.oldest_record_time(), start_ts));
    if (start_block_index == -1) {
      LOG_ERROR("No block in entry '{}' for ts={}", options_.name, TimeUtil::ToString(start_ts));
      return {{}, {.code = 500, .message = "Failed to find the needed block in descriptor"}};
    }

    int stop_block_index = FindBlock(std::min(descriptor_.latest_record_time(), stop_ts), start_block_index);
    if (stop_block_index == -1) {
      LOG_ERROR("No block in entry '{}' for ts={}", options_.name, TimeUtil::ToString(stop_ts));
      return {{}, {.code = 500, .message = "Failed to find the needed block in descriptor"}};
    }

    std::vector<RecordInfo> records;
    for (auto block_index = start_block_index; block_index <= stop_block_index; ++block_index) {
      auto block = descriptor_.blocks(block_index);
      for (auto record_index = 0; record_index < block.records_size(); ++record_index) {
        const auto& record = block.records(record_index);
        if (record.timestamp() >= start_ts && record.timestamp() < stop_ts) {
          records.push_back(RecordInfo{.time = ToTimePoint(record.timestamp()), .size = record.end() - record.begin()});
        }
      }
    }

    if (records.empty()) {
      return {{}, {.code = 404, .message = "No records for time interval"}};
    }

    std::ranges::sort(records, {}, &RecordInfo::time);
    return {records, {}};
  }

  Error RemoveOldestBlock() override {
    if (descriptor_.blocks_size() == 1) {
      return {};
    }

    auto& block = descriptor_.blocks(0);
    fs::remove(full_path_ / fmt::format(kBlockNameFormat, block.id()));

    // Don't remove the last block
    const auto& next_block = descriptor_.blocks(1);
    descriptor_.mutable_oldest_record_time()->CopyFrom(next_block.latest_record_time());

    descriptor_.set_size(descriptor_.size() - descriptor_.blocks(0).size());
    descriptor_.mutable_blocks()->DeleteSubrange(0, 1);

    return SaveDescriptor();
  }

  Info GetInfo() const override {
    size_t record_count = 0;
    for (int i = 0; i < descriptor_.blocks_size(); ++i) {
      record_count += descriptor_.blocks(i).records_size();
    }

    return {
        .block_count = static_cast<size_t>(descriptor_.blocks_size()),
        .record_count = record_count,
        .bytes = descriptor_.size(),
        .oldest_record_time = ToTimePoint(descriptor_.oldest_record_time()),
        .latest_record_time = ToTimePoint(descriptor_.latest_record_time()),
    };
  }

  const Options& GetOptions() const override { return options_; }

 private:
  proto::EntryDescriptor::Block* StartNextBlock(const google::protobuf::Timestamp& proto_ts) {
    auto id = current_block_->id();
    auto block = descriptor_.add_blocks();
    block->set_id(id + 1);
    block->mutable_begin_time()->CopyFrom(proto_ts);
    return block;
  }

  int FindBlock(const google::protobuf::Timestamp& proto_ts, int start_from = 0) const {
    auto block_index = -1;
    for (auto i = 0; i < descriptor_.blocks_size(); ++i) {
      const auto& current_block = descriptor_.blocks(i);
      if (proto_ts >= current_block.begin_time() && proto_ts <= current_block.latest_record_time()) {
        block_index = i;
        break;
      }
    }
    return block_index;
  }

  static google::protobuf::Timestamp FromTimePoint(const Time& time) {
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count();
    return TimeUtil::MicrosecondsToTimestamp(microseconds);
  }

  static Time ToTimePoint(const google::protobuf::Timestamp& time) {
    return Time() + std::chrono::microseconds(TimeUtil::TimestampToMicroseconds(time));
  }

  Error SaveDescriptor() const {
    auto descriptor_path = full_path_ / kDescriptorName;
    std::ofstream descriptor_file(descriptor_path);
    if (descriptor_file) {
      descriptor_.SerializeToOstream(&descriptor_file);
    } else {
      return {.code = 500, .message = "Failed save a descriptor"};
    }

    return Error::kOk;
  }

  static constexpr std::string_view kBlockNameFormat = "{:08d}.block";
  static constexpr std::string_view kSettingsName = ".settings";
  static constexpr std::string_view kDescriptorName = ".descriptor";

  Options options_;
  fs::path full_path_;
  proto::EntryDescriptor descriptor_;
  proto::EntryDescriptor::Block* current_block_;
};

std::unique_ptr<IEntry> IEntry::Build(IEntry::Options options) { return std::make_unique<Entry>(std::move(options)); }

std::unique_ptr<IEntry> IEntry::Restore(std::filesystem::path full_path) {
  return std::make_unique<Entry>(std::move(full_path));
}

/**
 * Streams
 */

std::ostream& operator<<(std::ostream& os, const IEntry::ReadResult& result) {
  os << fmt::format("<IEntry::ReadResult data={}  error={} time={}>", result.blob, result.error.ToString(),
                    IEntry::Time::clock::to_time_t(result.time));
  return os;
}

std::ostream& operator<<(std::ostream& os, const IEntry::Info& info) {
  os << fmt::format(
      "<IEntry::Info block_count={}  record_count={} bytes={} oldest_record_time={} latest_record_time={}>",
      info.block_count, info.record_count, info.bytes, to_time_t(info.oldest_record_time),
      to_time_t(info.latest_record_time));
  return os;
}

std::ostream& operator<<(std::ostream& os, const IEntry::RecordInfo& info) {
  os << fmt::format("<IEntry::RecordInfo time={}, size={}>", to_time_t(info.time), info.size);
  return os;
}
};  // namespace reduct::storage
