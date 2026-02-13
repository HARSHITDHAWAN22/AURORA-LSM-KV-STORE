#include "KVStore.h"
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <chrono>
#include <thread>

#include "MemTable.h"
#include<math.h>
#include<algorithm>
#include "MemTableIterator.h"
#include "SSTableIterator.h"
#include "MergeIterator.h"
#include "RangeIterator.h"

#include <filesystem>



void KVStore::loadStats() {
    std::ifstream in("metadata/stats.dat", std::ios::binary);
    if (!in.is_open()) return;
    in.read(reinterpret_cast<char*>(&stats), sizeof(stats));
}

void KVStore::saveStats() const {
    std::ofstream out("metadata/stats.dat",
                      std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(&stats),
              sizeof(stats));
}


KVStore::KVStore(const std::string& configPath,
                 const std::string& strategy)
    : configManager(configPath),
      memTable(nullptr),
      wal("metadata/wal.log"),
      compaction(
          strategy == "tiering"
              ? Compaction::Strategy::TIERED
              : Compaction::Strategy::LEVEL,
          0
      ),
      manifest("metadata/manifest.txt"),
      sstableCounter(0),
      running(false)
{
    if(!configManager.load()){
        std::cerr << "Config load failed\n";
        std::exit(1);
    }
loadStats();

    //SAFE: heap allocation (no mutex copy)
    memTable = new MemTable(configManager.getMemTableMaxEntries());

    // WAL recovery
    wal.replay(*memTable);

    loadFromManifest();
    //sortSSTablesByAge();

    running = true;
    flushThread = std::thread(&KVStore::backgroundFlush, this);
}

KVStore::~KVStore(){
    running = false;
    saveStats();

    if(flushThread.joinable())
        flushThread.join();

    delete memTable;
}

void KVStore::loadFromManifest(){
    manifest.load();

    for(const auto& path : manifest.getSSTables()){
        sstables.emplace_back(
            path,
            configManager.getBloomFilterBitSize(),
            configManager.getBloomFilterHashCount()
        );
        ++sstableCounter;
    }
}
// void KVStore::sortSSTablesByAge() {
//     std::sort(
//         sstables.begin(),
//         sstables.end(),
//         [](const SSTable& a, const SSTable& b) {
//             return std::stoi(a.getFilePath().substr(a.getFilePath().find_last_of('_') + 1)) <
//        std::stoi(b.getFilePath().substr(b.getFilePath().find_last_of('_') + 1));

//         }
//     );
// }

void KVStore::put(const std::string& key,const std::string& value){
   stats.totalPuts++;


    wal.logPut(key,value);
    memTable->put(key,value);

    if(memTable->isFull()){
        flushMemTable();
    }
}

bool KVStore::get(const std::string& key,std::string& value){
    {stats.totalGets++;

    std::string memVal;
    if (memTable->get(key, memVal)) {
        if (memVal == TOMBSTONE)
            return false;

        value = memVal;
        return true;
    }
}


    for(auto it = sstables.rbegin(); it != sstables.rend(); ++it){
        GetResult res = it->get(key, value);

if (res == GetResult::FOUND) {
    return true;
}
if (res == GetResult::DELETED) {
    return false;
}
// NOT_FOUND → continue

    }
    return false;
}

void KVStore::deleteKey(const std::string& key){
    wal.logDelete(key);
    memTable->put(key, TOMBSTONE);


    if(memTable->isFull()){
        flushMemTable();
    }
}

void KVStore::flushMemTable(){
    std::lock_guard<std::mutex> guard(flushMutex);

    if(memTable->isEmpty())
        return;

    std::string filePath =
        configManager.getSSTableDirectory() +
        "/sstable_" + std::to_string(sstableCounter++) + ".dat";

    SSTable sstable(
        filePath,
        configManager.getBloomFilterBitSize(),
        configManager.getBloomFilterHashCount()
    );

    if (sstable.writeToDisk(memTable->getData())) {
        // estimate flush bytes
for (const auto& kv : memTable->getData()) {
    stats.totalBytesWritten += kv.first.size() + kv.second.size();
}


    // IMPORTANT FIX
    SSTable reloaded(
        filePath,
        configManager.getBloomFilterBitSize(),
        configManager.getBloomFilterHashCount()
    );

    sstables.push_back(reloaded);

    manifest.addSSTable(filePath);
    manifest.save();

    memTable->clear();
    wal.clear();
    runCompactionIfNeeded();
    stats.totalFlushes++;
}


}

void KVStore::flush(){
    flushMemTable();
  //  sortSSTablesByAge();
}

void KVStore::scan(const std::string& start,const std::string& end){
    // create iterators
    std::vector<Iterator*> inputs;

    // MemTable iterator (highest priority)
    MemTableIterator memIter(
        memTable->getData().cbegin(),
        memTable->getData().cend()
    );
    inputs.push_back(&memIter);

    // SSTable iterators (newest first = higher priority)
    std::vector<SSTableIterator> sstIters;
    for(auto it = sstables.rbegin(); it != sstables.rend(); ++it){
        sstIters.emplace_back(*it);
        inputs.push_back(&sstIters.back());
    }

    // merge iterator
    MergeIterator merge(inputs);

    // range filter 
    RangeIterator range(&merge, start, end);

    // output
    while(range.valid()){
       if(range.value() != TOMBSTONE){
    std::cout << range.key()
              << " -> "
              << range.value()
              << "\n";
}
 range.next(); 
    }
}


void KVStore::backgroundFlush(){
    while(running){
        std::this_thread::sleep_for(std::chrono::seconds(2));
        flushMemTable();
    }
}

void KVStore::runCompactionIfNeeded(){
    size_t before = sstables.size();

    compaction.run(sstables);

    size_t after = sstables.size();

    if (after < before) {
       stats.totalCompactions++;
        // approximate compaction write amplification
        for (const auto& table : sstables) {
            stats.totalCompactionBytes += std::filesystem::file_size(table.getFilePath());
        }
    }
}


void KVStore::printStats() const {
    std::cout << "==== AuroraKV Stats ====\n";
    std::cout << "Total PUTs        : " << stats.totalPuts << "\n";
    std::cout << "Total GETs        : " << stats.totalGets << "\n";
    std::cout << "Total Flushes     : " << stats.totalFlushes << "\n";
    std::cout << "Total Compactions : " << stats.totalCompactions << "\n";
    std::cout << "SSTable Count     : " << sstables.size() << "\n";

    std::cout << "Bytes Written     : " << stats.totalBytesWritten << "\n";
    std::cout << "Compaction Bytes  : " << stats.totalCompactionBytes << "\n";

    if (stats.totalBytesWritten > 0) {
        double wa =
            (double)(stats.totalBytesWritten + stats.totalCompactionBytes)
            / (double)stats.totalBytesWritten;
        std::cout << "Write Amplification : " << wa << "\n";
    }

    std::cout << "========================\n";
}
