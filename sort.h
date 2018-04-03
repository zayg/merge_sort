#include <block_operator.h>

#pragma once

// SortMeta contains needed metas for scheduling a sorting.
struct SortMeta {
  // sorted partition number
  int partitionNum_;
  // total block number
  int blockNum_;
  BlockStorageType type_;
};

// SortDriver controls the entire sorting progress.
class SortDriver {
public:
  SortDriver(const SortMeta& meta, const vector<BlockPtr>& blocks);
  Status Sort();

private:
  Status sortSSD();
  Status sortMem();

public:
  SortMeta meta_;
  vector<BlockPtr> blocks_;
};
