// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include <map>
#include <set>
#include <functional>

#include "DataStore.hpp"
#include "STDigest.hpp"
#include "Logger.hpp"
#include "kvstream.h"

using std::map;

namespace bftEngine {
namespace bcst {
namespace impl {

class InMemoryDataStore : public DataStore {
 public:
  explicit InMemoryDataStore(uint32_t sizeOfReservedPage);
  ~InMemoryDataStore() override {
    deleteAllPendingPages();
    for (auto& p : pages) {
      ::free(p.second.page);
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // config
  //////////////////////////////////////////////////////////////////////////

  bool initialized() override;
  void setAsInitialized() override;

  void setReplicas(const set<uint16_t> replicas) override;
  set<uint16_t> getReplicas() override;

  void setMyReplicaId(uint16_t id) override;
  uint16_t getMyReplicaId() override;

  void setFVal(uint16_t fVal) override;
  uint16_t getFVal() override;

  void setMaxNumOfStoredCheckpoints(uint64_t numChecks) override;
  uint64_t getMaxNumOfStoredCheckpoints() const override;

  void setNumberOfReservedPages(uint32_t numResPages) override;
  uint32_t getNumberOfReservedPages() override;

  //////////////////////////////////////////////////////////////////////////
  // first/last checkpoint number which are currently maintained
  //////////////////////////////////////////////////////////////////////////

  void setLastStoredCheckpoint(uint64_t c) override;
  uint64_t getLastStoredCheckpoint() override;

  void setFirstStoredCheckpoint(uint64_t c) override;
  uint64_t getFirstStoredCheckpoint() override;

  //////////////////////////////////////////////////////////////////////////
  // Checkpoints
  //////////////////////////////////////////////////////////////////////////

  void setCheckpointDesc(uint64_t checkpoint, const CheckpointDesc& desc) override;
  CheckpointDesc getCheckpointDesc(uint64_t checkpoint) override;
  bool hasCheckpointDesc(uint64_t checkpoint) override;
  void deleteDescOfSmallerCheckpoints(uint64_t checkpoint) override;

  //////////////////////////////////////////////////////////////////////////
  // Fetching status
  //////////////////////////////////////////////////////////////////////////

  void setIsFetchingState(bool b) override;
  bool getIsFetchingState() override;

  void setCheckpointBeingFetched(const CheckpointDesc& c) override;
  CheckpointDesc getCheckpointBeingFetched() override;
  bool hasCheckpointBeingFetched() override;
  void deleteCheckpointBeingFetched() override;

  void setFirstRequiredBlock(uint64_t i) override;
  uint64_t getFirstRequiredBlock() override;

  void setLastRequiredBlock(uint64_t i) override;
  uint64_t getLastRequiredBlock() override;

  //////////////////////////////////////////////////////////////////////////
  // reserved pages
  //////////////////////////////////////////////////////////////////////////

  void setPendingResPage(uint32_t inPageId, const char* inPage, uint32_t inPageLen) override;
  bool hasPendingResPage(uint32_t inPageId) override;
  void getPendingResPage(uint32_t inPageId, char* outPage, uint32_t pageLen) override;
  uint32_t numOfAllPendingResPage() override;
  set<uint32_t> getNumbersOfPendingResPages() override;
  void deleteAllPendingPages() override;

  void associatePendingResPageWithCheckpoint(uint32_t inPageId,
                                             uint64_t inCheckpoint,
                                             const STDigest& inPageDigest) override;

  void setResPage(uint32_t inPageId, uint64_t inCheckpoint, const STDigest& inPageDigest, const char* inPage) override;

  bool getResPage(uint32_t inPageId,
                  uint64_t inCheckpoint,
                  uint64_t* outActualCheckpoint,
                  STDigest* outPageDigest,
                  char* outPage,
                  uint32_t copylength) override;

  void deleteCoveredResPageInSmallerCheckpoints(uint64_t inCheckpoint) override;

  ResPagesDescriptor* getResPagesDescriptor(uint64_t inCheckpoint) override;
  void free(ResPagesDescriptor*) override;

  class NullTransaction : public ITransaction {
   public:
    NullTransaction() : concord::storage::ITransaction(0) {}
    void commit() override {}
    void rollback() override {}
    void put(const concordUtils::Sliver& key, const concordUtils::Sliver& value) override {}
    std::string get(const concordUtils::Sliver& key) override { return ""; }
    void del(const concordUtils::Sliver& key) override {}
  };

  DataStoreTransaction* beginTransaction() override {
    // DBDataStore and IDBClient are not thread safe or reentrant. There can
    // only be one transaction at a time.
    return new DataStoreTransaction(this->shared_from_this(), new NullTransaction());
  }
  void setEraseDataStoreFlag() override {}

 protected:
  const uint32_t sizeOfReservedPage_;

  bool wasInit_ = false;

  set<uint16_t> replicas_;

  uint16_t myReplicaId_ = UINT16_MAX;

  uint16_t fVal_ = UINT16_MAX;

  uint64_t maxNumOfStoredCheckpoints_ = UINT64_MAX;

  uint32_t numberOfReservedPages_ = UINT32_MAX;

  uint64_t lastStoredCheckpoint = UINT64_MAX;
  uint64_t firstStoredCheckpoint = UINT64_MAX;

  map<uint64_t, CheckpointDesc> descMap;

  bool fetching = false;

  CheckpointDesc checkpointBeingFetched;
  // none if checkpointBeingFetched.checkpointNum == 0

  uint64_t firstRequiredBlock = UINT64_MAX;
  uint64_t lastRequiredBlock = UINT64_MAX;

  map<uint32_t, char*> pendingPages;

  struct ResPageKey {
    uint32_t pageId;
    uint64_t checkpoint;

    bool operator<(const ResPageKey& rhs) const {
      if (pageId != rhs.pageId)
        return pageId < rhs.pageId;
      else
        return checkpoint > rhs.checkpoint;
    }
  };

  struct ResPageVal {
    STDigest pageDigest;
    char* page;
  };

  map<ResPageKey, ResPageVal> pages;

  std::string getPagesForLog() {
    std::ostringstream oss;
    oss << "reserved pages: ";
    for (const auto& it : pages) oss << "[" << it.first.pageId << ":" << it.first.checkpoint << "]";
    return oss.str();
  }

  friend class DBDataStore;
  const uint32_t getSizeOfReservedPage() const { return sizeOfReservedPage_; }
  const map<uint64_t, CheckpointDesc>& getDescMap() const { return descMap; }
  const map<ResPageKey, ResPageVal>& getPagesMap() const { return pages; }
  const map<uint32_t, char*>& getPendingPagesMap() const { return pendingPages; }

  void setInitialized(bool init) { wasInit_ = init; }
  logging::Logger& logger() {
    static logging::Logger logger_ = logging::getLogger("concord.bft.st.inmem");
    return logger_;
  }
};

}  // namespace impl
}  // namespace bcst
}  // namespace bftEngine
