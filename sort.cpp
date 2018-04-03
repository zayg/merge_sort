#include <sort.h>

Status SortDriver::Sort() {   
  switch (meta_.type_) {
  case BlockStorageType::BLOCK_STORAGE_SSD:
    return sortSSD();
  case BlockStorageType::BLOCK_STORAGE_MEM:
    return sortMem();
  }
}

// Observation:
// 1. Speed of Memory/Cache access >> Speed of SSD Read
// 2. Speed of SSD random Read is fast.
// 3. Average Speed of SSD Sequential Write > Average Speed of SSD Random Write
// A possible solution may be:
// 1. Sort in memory can be done in a single thread.
// 2. Block reads are done asynchronously in some parallel degree.
// 3. Sort results are dumped sequentially.
Status SortDriver::sortSSD() {
  vector<IBlockLoaderPtr> loaders;
  loaders.reserve(blocks_.size());
  for (const auto& block : blocks_) {
    loaders.push_back(BlockLoaderBuilder::NewBlockLoader(block));
    auto rc = loaders.back().Init();
  }
  auto ssdChannelNum = 8;
  BlockLoadTaskExecuter executer(ssdChannelNum);
  vector<RecordType> buffer;
  vector<int> indexes;
  int idx = 0;
  // initialize load order for BlockLoadTaskExecuter
  for (auto& loader : loaders) {
    loader.Load(1, buffer);
    indexes.push_back(idx++);
  }
  auto cmp = [&] (int l, int r) {
    return buffer[l] < buffer[r];
  };
  sort(indexes.begin(), indexes.end(), cmp);
  // loading begin
  for (const auto idx : indexes) {
    executer.AddTask(vector<IBlockLoader>{loaders[i]});
  }
  executer.Start();

  // Since blocks in each partition are sorted. We create a LoserTree with
  // the lowest layer contains more than meta_.partNum_ nodes. Each loserTreeNode
  // of the lowest layer will link to a nullptr or a IBlockLoaderPtr. And it will
  // be relink to the next IBlockLoaderPtr based on the indexes order (when the 
  // current one is done fetching).
  // By using LoserTree, we need log(2 * meta_.partNum_ - 1) time in memory for
  // each round to get a sorted record. Then we just put this record to the
  // sinker. It will dump a group records asynchronously.
  LoserTreeSorter sorter(loaders, indexes, meta_.partNum_);
  // sink sorted result to some target device.
  SortSinker sinker;
  // the Sorter will be empty when every loader is fetched to the end of its
  // block.
  while (sorter.empty()) {
    // Add the sort result into the sinker.
    // The LoserTreeSorter::Next will block until a new sorted result is popped
    // out. Internally, it may be blocked by The IBlockLoader::Fetch.
    sinker.Add(sorter.Next()); 
  }

  // loading end
  executer.Stop();
  sinker.Close();
}

// We assume the result can be stored inside memory
// A possible solution could be:
// 1. Merge pairs of Blocks in parallel.
// 2. Banlance the merge tasks. A very long merge task can be split to several
//    small ones. (use binary search to determine the merge boundaries.)
// 3. Inside one merge, judge the minimum and the maximum key between two
//    to-merge-block first. (since blocks are ordered among one partition)
Status SortDriver::sortMem() {
  // A MergeTaskPlanner generates merge plans, every plan is associated with a
  // FutureGroup. Each plan may split to one or more sub tasks if current merge
  // plan is too big. It maintains a list of blocks. It is finished until there
  // is 0 block in the list.
  MergeTaskPlanner planner(blocks_);
  // MergeTaskRunner does the actual merge
  MergeTaskRunner runner;
  runner.Start();
  // FutureGroup can be defined recursively. It derives from Future.
  FutureGroup fg;
  // sorted result, stored in a single block
  BlockPtr sorted;
  do {
    while (!planner.IsFinish()) {
      auto task = planner.PopTask();
      fg.AddFuture(task.GetFuture());
      runner.AddTask(task);
    }
    // wait util 2 tasks are done
    auto futureVec = fg.Wait(2, -1);
    if (futureVec.size() == 1) {
      // get final block
      sorted = futureVec[0].get();
      break;
    } else if (futureVec.size() == 0) {
      break;
    }
    ASSERTION(futureVec.size() == 2, "Logical Error");
    planner.Add(futureVec[0].get());
    planner.Add(futureVec[1].get());
  } while (1);

  // sink sorted result to some target device.
  SortSinker sinker;
  auto loader = BlockLoaderBuilder::NewBlockLoader(sorted);
  loader.Init();
  while (loader.Fetch(1, buffers) != Status::kEof) {
    sinker.Add(buffers[0]);
  }
}
