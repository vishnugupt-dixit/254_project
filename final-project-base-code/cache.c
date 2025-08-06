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
  unsigned long long target_set = cache_set(address, cache);
  Set *current_set = &cache->sets[target_set];
  current_set->lru_clock++;

  if (probe_cache(address, cache)) {
    hit_cacheline(address, cache);
    r.status = CACHE_HIT;
    cache->hit_count++;
    return r;
  }

  if (insert_cacheline(address, cache)) {
    r.status = CACHE_MISS;
    r.insert_block_addr = address_to_block(address, cache);
    cache->miss_count++;
    return r;
  }

  
  unsigned long long victim_block = victim_cacheline(address, cache);
  r.victim_block_addr = victim_block;
  r.insert_block_addr = address_to_block(address, cache);
  replace_cacheline(victim_block, address, cache);
  r.status = CACHE_EVICT;
  cache->miss_count++;
  cache->eviction_count++;

  return r;
}

unsigned long long address_to_block(const unsigned long long address,
                                  const Cache *cache) {
  return address & ~((1ULL << cache->blockBits) - 1);
}

unsigned long long cache_tag(const unsigned long long address,
                           const Cache *cache) {
  return address >> (cache->setBits + cache->blockBits);
}

unsigned long long cache_set(const unsigned long long address,
                           const Cache *cache) {
  return (address >> cache->blockBits) & ((1ULL << cache->setBits) - 1);
}

bool probe_cache(const unsigned long long address, const Cache *cache) {
  unsigned long long tag = cache_tag(address, cache);
  Set *set = &cache->sets[cache_set(address, cache)];
  int i = 0;

  while (i < cache->linesPerSet) {
    if (set->lines[i].valid && set->lines[i].tag == tag) {
      return true;
    }
    i++;
  }
  return false;
}

void hit_cacheline(const unsigned long long address, Cache *cache) {
  unsigned long long tag = cache_tag(address, cache);
  Set *set = &cache->sets[cache_set(address, cache)];
  int i = 0;

  while (i < cache->linesPerSet) {
    Line *line = &set->lines[i];
    if (line->valid && line->tag == tag) {
      if (cache->lfu == 0) {
        line->lru_clock = set->lru_clock;
      } else {
        line->access_counter++;
      }
      break;
    }
    i++;
  }
}

bool insert_cacheline(const unsigned long long address, Cache *cache) {
  unsigned long long set_idx = cache_set(address, cache);
  Set *set = &cache->sets[set_idx];
  unsigned long long tag = cache_tag(address, cache);
  unsigned long long block = address_to_block(address, cache);
  int i = 0;

  while (i < cache->linesPerSet) {
    Line *line = &set->lines[i];
    if (!line->valid) {
      line->valid = true;
      line->tag = tag;
      line->block_addr = block;
      if (cache->lfu == 0) {
        line->lru_clock = set->lru_clock;
      } else {
        line->access_counter = 1;
      }
      return true;
    }
    i++;
  }
  return false;
}

unsigned long long victim_cacheline(const unsigned long long address,
                                  const Cache *cache) {
  unsigned long long set_idx = cache_set(address, cache);
  Set *set = &cache->sets[set_idx];
  int victim_index = 0;

  if (cache->lfu == 0) {
    unsigned long long min_clock = set->lines[0].lru_clock;
    int i = 1;
    while (i < cache->linesPerSet) {
      if (set->lines[i].lru_clock < min_clock) {
        min_clock = set->lines[i].lru_clock;
        victim_index = i;
      }
      i++;
    }
  } else {
    int min_access = set->lines[0].access_counter;
    unsigned long long min_lru = set->lines[0].lru_clock;
    victim_index = 0;
    int i = 1;
    
    while (i < cache->linesPerSet) {
      Line *line = &set->lines[i];
      if (line->access_counter < min_access ||
         (line->access_counter == min_access && line->lru_clock < min_lru)) {
        min_access = line->access_counter;
        min_lru = line->lru_clock;
        victim_index = i;
      }
      i++;
    }
  }
  return set->lines[victim_index].block_addr;
}

void replace_cacheline(const unsigned long long victim_block_addr,
                     const unsigned long long insert_addr, Cache *cache) {
  unsigned long long set_idx = cache_set(insert_addr, cache);
  Set *set = &cache->sets[set_idx];
  unsigned long long tag = cache_tag(insert_addr, cache);
  unsigned long long block = address_to_block(insert_addr, cache);
  int i = 0;

  while (i < cache->linesPerSet) {
    Line *line = &set->lines[i];
    if (line->valid && line->block_addr == victim_block_addr) {
      line->valid = true;
      line->tag = tag;
      line->block_addr = block;
      if (cache->lfu == 0) {
        line->lru_clock = set->lru_clock;
      } else {
        line->access_counter = 1;
        line->lru_clock = set->lru_clock;
      }
      break;
    }
    i++;
  }
}

void cacheSetUp(Cache *cache, char *name) {
  int num_sets = 1 << cache->setBits;
  cache->sets = (Set *)malloc(num_sets * sizeof(Set));
  int i = 0;

  while (i < num_sets) {
    cache->sets[i].lines = (Line *)malloc(cache->linesPerSet * sizeof(Line));
    cache->sets[i].lru_clock = 0;
    int j = 0;

    while (j < cache->linesPerSet) {
      Line *line = &cache->sets[i].lines[j];
      line->valid = false;
      line->tag = 0;
      line->block_addr = 0;
      line->lru_clock = 0;
      line->access_counter = 0;
      j++;
    }
    i++;
  }

  cache->hit_count = 0;
  cache->miss_count = 0;
  cache->eviction_count = 0;
  cache->name = name;
}

void deallocate(Cache *cache) {
  int num_sets = 1 << cache->setBits;
  int i = 0;
  
  while (i < num_sets) {
    free(cache->sets[i].lines);
    i++;
  }
  free(cache->sets);
}

void printSummary(const Cache *cache) {
  printf("%s hits: %d, misses: %d, evictions: %d\n", cache->name, cache->hit_count,
         cache->miss_count, cache->eviction_count);
}

int processCacheOperation(unsigned long address, Cache *cache) {
    result r;
    r = operateCache(address, cache);
    
    if (r.status == CACHE_HIT) {
        return CACHE_HIT_LATENCY;
    } else if (r.status == CACHE_MISS) {
        return CACHE_MISS_LATENCY;
    } else {
        return CACHE_OTHER_LATENCY;
    }
}