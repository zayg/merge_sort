#pragma once

using RecordType = int64_t;

enum class BlockStorageType {
  BLOCK_STORAGE_MEM = 0,
  BLOCK_STORAGE_SSD = 1,
};

// Block contains all information to access the block data
struct Block {
  BlockStorageType blkType_; 
  // RefType refType_;
  // Block data reference. BlockReader will use the reference to read the
  // actual data.
  std::string ref_;
};

using BlockPtr = std::shared_ptr<Block>;
