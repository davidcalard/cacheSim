#ifndef HW2_CACHESIM_H
#define HW2_CACHESIM_H

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <math.h>
#include <array>
#include <vector>
#include <iomanip>

#define ADDRESS_SIZE 32
#define STRAIGHTEN_BITS 2
#define EFF_ADDRESS_SIZE ADDRESS_SIZE -STRAIGHTEN_BITS
#define DATA_SIZE 32
#define UNDEFINED -1

#define MISS false
#define HIT true

#define MEM_CYC 0
#define B_SIZE 1
#define L1_SIZE 2
#define L2_SIZE 3
#define L1_ASSOC 4
#define L2_ASSOC 5
#define L1_CYC 6
#define L2_CYC 7
#define WR_ALLOC 8

using namespace std;

/*------------------------------------------------------------------------------
 *              cache class, subclasses and methods declaration
 -----------------------------------------------------------------------------*/

//-----------------------------------way--------------------------------------//

class way {
    unsigned *entry;
    const unsigned entries_num;
    const unsigned tag_size;
public:
    way(unsigned entries_num, unsigned block_size);
    ~way();

    void inline swap(unsigned address, unsigned set);
    bool check(unsigned address, unsigned set);

};

//----------------------------------level-------------------------------------//

class level {
    vector<vector<unsigned>> LRU_table;
    vector<way> ways_vec;
    const unsigned b_size;//log2
    const unsigned size;//log2
    const unsigned assoc;//log2
    const unsigned wr_alloc;
    unsigned hits_num;
    unsigned misses_num;
public:
    level(unsigned b_s, unsigned s, unsigned a, unsigned w);

    int findWay(unsigned address, unsigned set);
    void updateLRU(unsigned way_index, unsigned set);
    bool accessLevel(unsigned address,bool is_wr);

    bool read(unsigned address);
    bool write(unsigned address);

    void incHit();
    void incMiss();

    unsigned inline getStat(bool mode);
};
//-------------------------------cache----------------------------------------//


class cache{
    level l1;
    level l2;
    unsigned l1_cyc;
    unsigned l2_cyc;
    unsigned mem_cyc;
    unsigned requests;
    unsigned mem_requests;

public:
    explicit cache(unsigned init_param[9]);

    void writeToCache(unsigned long int address);
    void readFromCache(unsigned long int address);
    void printStats();

};

#endif //HW2_CACHESIM_H