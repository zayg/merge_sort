#include <block.h>

#pragma once

// IBlockLoader supports block read operations
class IBlockLoader {
public:
  IBlockLoader(std::shared_ptr<BlockPtr> blockPtr);
  // Initialize the loader based on the blockPtr.
  // Return specific error while processing the block data reference.
  virtual Status Init() = 0;
  // Load recordCnt records from the blockPtr. The result will be buffered
  // inside this class. the result will also be appended in buffer if buffer is
  // not nullptr.
  virtual Status Load(int recordCnt, vector<RecordType>* buffer) = 0;
  // Fetch recordCnt records from the internal buffer. Read Cursor will be
  // advanced once succeed.
  // Wait the uncoming result for timeoutUS.
  virtual Status Fetch(int recordCnt, vector<RecordType*>* buffer,
      int timeoutUS) = 0;
  // Block size of byte
  virtual Status Size(int32_t* size) = 0;
};
using IBlockLoaderPtr = std::shared_ptr<IBlockLoader>;

// ExternalBlockLoader will use a ring buffer to cache the Load() result.
// Every time Fetch() succeed, the ring buffer will invalidate the fetched
// records. Load() will block if ring buffer is full.
class ExternalBlockLoader : public IBlockLoader {
};

class BlockLoaderBuilder {
public:
  static BlockLoaderPtr NewBlockLoader(BlockPtr& block);
};

// BlockLoadTaskExecuter 
class BlockLoadTaskExecuter {
public:
  BlockLoadTaskExecuter(int workerNum);
  void AddTasks(const vector<IBlockLoader>& tasks);
  void Start();
  void Stop();

public:
  vector<IBlockLoader> taskQueue_;
};
