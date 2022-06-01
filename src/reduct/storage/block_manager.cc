// Copyright 2022 Alexey Timin

#include "reduct/storage/block_manager.h"

#include <fcntl.h>
#include <fmt/core.h>
#include <google/protobuf/util/time_util.h>

#include <fstream>
#include <utility>

namespace reduct::storage {

namespace fs = std::filesystem;
using google::protobuf::Timestamp;
using google::protobuf::util::TimeUtil;

using core::Error;

std::filesystem::path BlockPath(const fs::path& parent, const proto::Block& block, std::string_view ext) {
  auto block_path = parent / fmt::format("{}{}", TimeUtil::TimestampToMicroseconds(block.begin_time()), ext);
  return block_path;
}

class BlockManager : public IBlockManager {
 public:
  explicit BlockManager(fs::path parent) : parent_(std::move(parent)) {}

  core::Result<BlockSPtr> LoadBlock(const Timestamp& proto_ts) override {
    if (latest_loaded_ && latest_loaded_->begin_time() == proto_ts) {
      return {latest_loaded_, Error::kOk};
    }

    auto file_name = parent_ / fmt::format("{}{}", TimeUtil::TimestampToMicroseconds(proto_ts), kMetaExt);
    std::ifstream file(file_name);
    if (!file) {
      return {
          nullptr,
          {
              .code = 500,
              .message =
                  fmt::format("Failed to load a block descriptor {}: {}", file_name.string(), std::strerror(errno)),
          },
      };
    }

    latest_loaded_ = std::make_shared<proto::Block>();
    if (!latest_loaded_->ParseFromIstream(&file)) {
      latest_loaded_ = nullptr;
      return {nullptr, {.code = 500, .message = fmt::format("Failed to parse meta: {}", file_name.string())}};
    }

    return {latest_loaded_, Error::kOk};
  }

  core::Result<BlockSPtr> StartBlock(const Timestamp& proto_ts, size_t max_block_size) override {
    // allocate the whole block
    latest_loaded_ = std::make_shared<proto::Block>();
    latest_loaded_->mutable_begin_time()->CopyFrom(proto_ts);

    auto block_path = BlockPath(parent_, *latest_loaded_, kBlockExt);
    int fd = open(block_path.c_str(), O_WRONLY | O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd < 0) {
      return {{}, {.code = 500, .message = "Failed create a new block for writing"}};
    }
    if (posix_fallocate(fd, 0, max_block_size) != 0) {
      close(fd);
      return {{}, {.code = 500, .message = fmt::format("Failed allocate a new block: {}", std::strerror(errno))}};
    }

    close(fd);



    auto err = SaveBlock(latest_loaded_);
    if (err) {
      return {{}, err};
    }

    return {latest_loaded_, Error::kOk};
  }

  core::Error SaveBlock(const BlockSPtr& block) const override {
    auto file_name = BlockPath(parent_, *block, kMetaExt);
    std::ofstream file(file_name);
    if (file) {
      block->SerializeToOstream(&file);
      return {};
    } else {
      return {.code = 500, .message = "Failed to save a block descriptor"};
    }
  }

 private:
  fs::path parent_;
  BlockSPtr latest_loaded_;
};

std::unique_ptr<IBlockManager> IBlockManager::Build(const std::filesystem::path& parent) {
  return std::make_unique<BlockManager>(parent);
}
}  // namespace reduct::storage
