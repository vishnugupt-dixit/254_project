#include "cache.h"
#include "dogfault.h"
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// DO NOT MODIFY THIS FILE. INVOKE AFTER EACH ACCESS FROM runTrace
void print_result(result r) {
  if (r.status == CACHE_EVICT)
    printf(" [status: miss eviction, victim_block: 0x%llx, insert_block: 0x%llx]",
           r.victim_block_addr, r.insert_block_addr);
  if (r.status == CACHE_HIT)
    printf(" [status: hit]");
  if (r.status == CACHE_MISS)
    printf(" [status: miss, insert_block: 0x%llx]", r.insert_block_addr);
}


result operateCache(const unsigned long long address, Cache *cache) {
  result r;
  r.status = CACHE_MISS;
  r.victim_block_addr = 0;
  r.insert_block_addr = 0;
  

  unsigned long long set_index = cache_set(address, cache);
  

  cache->sets[set_index].lru_clock++;
  

  if (probe_cache(address, cache)) {
    // cache hit
    hit_cacheline(address, cache);
    r.status = CACHE_HIT;
    cache->hit_count++;
  } else {

    if (insert_cacheline(address, cache)) {

      r.status = CACHE_MISS;
      r.insert_block_addr = address_to_block(address, cache);
      cache->miss_count++;
    } else {

      unsigned long long victim_addr = victim_cacheline(address, cache);
      replace_cacheline(victim_addr, address, cache);
      r.status = CACHE_EVICT;
      r.victim_block_addr = victim_addr;
      r.insert_block_addr = address_to_block(address, cache);
      cache->miss_count++;
      cache->eviction_count++;
    }
  }
  
  return r;
}

unsigned long long address_to_block(const unsigned long long address,
                                const Cache *cache) {
  unsigned long long block_mask = ~((1ULL << cache->blockBits) - 1);
  return address & block_mask; 
}


unsigned long long cache_tag(const unsigned long long address,
                             const Cache *cache) {

  int tag_shift = cache->setBits + cache->blockBits;
  return address >> tag_shift;
}


unsigned long long cache_set(const unsigned long long address,
                             const Cache *cache) {

  unsigned long long set_mask = (1ULL << cache->setBits) - 1;

  return (address >> cache->blockBits) & set_mask;
}


bool probe_cache(const unsigned long long address, const Cache *cache) {
  unsigned long long set_index = cache_set(address, cache);
  unsigned long long tag = cache_tag(address, cache);
  
  Set *set = &cache->sets[set_index];
  

  for (int i = 0; i < cache->linesPerSet; i++) {
    if (set->lines[i].valid && set->lines[i].tag == tag) {
      return true;
    }
  }
  
  return false;
}


void hit_cacheline(const unsigned long long address, Cache *cache) {
  unsigned long long set_index = cache_set(address, cache);
  unsigned long long tag = cache_tag(address, cache);
  
  Set *set = &cache->sets[set_index];
  for (int i = 0; i < cache->linesPerSet; i++) {
    if (set->lines[i].valid && set->lines[i].tag == tag) {
      // LRU clock making it recently used value
      set->lines[i].lru_clock = set->lru_clock;
      // update access counter for LFU checking for frequency of use
      set->lines[i].access_counter++;
      return;
    }
  }
}


bool insert_cacheline(const unsigned long long address, Cache *cache) {
  unsigned long long set_index = cache_set(address, cache);
  unsigned long long tag = cache_tag(address, cache);
  unsigned long long block_addr = address_to_block(address, cache);
  
  Set *set = &cache->sets[set_index];

  for (int i = 0; i < cache->linesPerSet; i++) {
    if (!set->lines[i].valid) {

      set->lines[i].valid = true;
      set->lines[i].tag = tag;
      set->lines[i].block_addr = block_addr;
      set->lines[i].lru_clock = set->lru_clock;
      set->lines[i].access_counter = 1;
      return true;
    }
  }
  
  return false; // no empty line found
}


unsigned long long victim_cacheline(const unsigned long long address,
                                const Cache *cache) {
  unsigned long long set_index = cache_set(address, cache);
  Set *set = &cache->sets[set_index];
  
  int victim_index = 0;
  
  if (cache->lfu == 0) {

    unsigned long long min_lru = set->lines[0].lru_clock;

    for (int i = 1; i < cache->linesPerSet; i++) {
      if (set->lines[i].lru_clock < min_lru) {
        min_lru = set->lines[i].lru_clock;
        victim_index = i;
      }
    }
  } else {

    int min_access = set->lines[0].access_counter;
    unsigned long long min_lru = set->lines[0].lru_clock;
    
    for (int i = 1; i < cache->linesPerSet; i++) {
      if (set->lines[i].access_counter < min_access ||
          (set->lines[i].access_counter == min_access && 
           set->lines[i].lru_clock < min_lru)) {
        min_access = set->lines[i].access_counter;
        min_lru = set->lines[i].lru_clock;
        victim_index = i;
      }
    }
  }
  
  return set->lines[victim_index].block_addr;
}

void replace_cacheline(const unsigned long long victim_block_addr,
               const unsigned long long insert_addr, Cache *cache) {
  unsigned long long set_index = cache_set(insert_addr, cache);
  unsigned long long tag = cache_tag(insert_addr, cache);
  unsigned long long block_addr = address_to_block(insert_addr, cache);
  
  Set *set = &cache->sets[set_index];

  for (int i = 0; i < cache->linesPerSet; i++) {
    if (set->lines[i].block_addr == victim_block_addr) {
      // Replace this line
      set->lines[i].valid = true;
      set->lines[i].tag = tag;
      set->lines[i].block_addr = block_addr;
      set->lines[i].lru_clock = set->lru_clock;
      set->lines[i].access_counter = 1;
      return;
    }
  }
}



void cacheSetUp(Cache *cache, char *name) {

  int num_sets = 1 << cache->setBits; // 2^setBits

  cache->sets = (Set*)malloc(num_sets * sizeof(Set));
  
  // initialize each set
  for (int i = 0; i < num_sets; i++) {
    cache->sets[i].lru_clock = 0;
    cache->sets[i].lines = (Line*)malloc(cache->linesPerSet * sizeof(Line));
    
    // initialize each line in the set
    for (int j = 0; j < cache->linesPerSet; j++) {
      cache->sets[i].lines[j].valid = false;
      cache->sets[i].lines[j].tag = 0;
      cache->sets[i].lines[j].block_addr = 0;
      cache->sets[i].lines[j].lru_clock = 0;
      cache->sets[i].lines[j].access_counter = 0;
    }
  }
  
  // set cache name
  cache->name = (char*)malloc(strlen(name) + 1);
  strcpy(cache->name, name);
}

// deallocate the memory space for the cache
void deallocate(Cache *cache) {
  int num_sets = 1 << cache->setBits;
  
  // free lines for each set
  for (int i = 0; i < num_sets; i++) {
    free(cache->sets[i].lines);
  }
  
  // free sets array
  free(cache->sets);
  
  // free cache name
  free(cache->name);
}

// print out summary stats for the cache
void printSummary(const Cache *cache) {
  printf("%s hits: %d, misses: %d, evictions: %d\n", cache->name, cache->hit_count,
         cache->miss_count, cache->eviction_count);
}
