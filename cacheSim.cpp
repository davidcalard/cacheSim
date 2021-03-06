/* 046267 Computer Architecture - Winter 20/21 - HW #2 */

#include "cacheSim.h"


/*------------------------------------------------------------------------------
 *                      static helper functions
------------------------------------------------------------------------------*/

static inline unsigned extract(unsigned address,unsigned size,unsigned moshe){
    return (address>>moshe)%((unsigned)pow(2,size));
}

/*------------------------------------------------------------------------------
 *              cache class, subclasses and methods implementation
 -----------------------------------------------------------------------------*/

//-----------------------------------Way--------------------------------------//

way::way(const unsigned set_size, const unsigned block_size): entries_num(pow(2,set_size)),
                                                              tag_size(EFF_ADDRESS_SIZE - set_size - block_size){
    for(unsigned i = 0; i < entries_num; i++){
        entry.insert(pair<unsigned ,unsigned >(i,0));
        is_defined.insert(pair<unsigned,bool>(i, false));
        is_dirty.insert(pair<unsigned,bool>(i, false));
    }
}

void way::swap(unsigned address, unsigned set, tuple<unsigned,bool,bool>& removed, bool is_write){
    get<ADDRESS>(removed) = entry.find(set)->second;
    get<DEFINED>(removed) = is_defined.find(set)->second;
    get<DIRTY>(removed) = is_dirty.find(set)->second;
    entry.find(set)->second = address;
    is_defined.find(set)->second = true;
    if(get<DEFINED>(removed))
        is_dirty.find(set)->second = false;
    if(is_write)
        is_dirty.find(set)->second = true;

}

bool way::check(unsigned address, unsigned set){
    unsigned our_tag = extract(address,tag_size,ADDRESS_SIZE-tag_size);
    unsigned curr_tag = extract(entry.find(set)->second,tag_size,ADDRESS_SIZE-tag_size);
    return our_tag == curr_tag && is_defined.find(set)->second;
}

void inline way::undefineEntry(unsigned int set) {
    is_defined.find(set)->second = false;
}

//----------------------------------level-------------------------------------//

level::level(unsigned b_s, unsigned s, unsigned a, unsigned w)
        : b_size(b_s), size(s), assoc(a), wr_alloc(w),
          hits_num(0), misses_num(0) {
    //entries calc
    unsigned entries_num = pow(2,(size - b_size - assoc));
    //ways init
    for (int i = 0; i < pow(2,assoc); i++)
        ways_vec.emplace_back(way((size - b_size - assoc), b_size));
    //LRU table init
    for (int i = 0; i < entries_num; i++) {
        LRU_table.emplace_back(vector<unsigned>());
        for (int j = 0; j < pow(2,assoc); j++) LRU_table[i].emplace_back(j);
    }
}

int level::findWay(unsigned address, unsigned set) {
    int way_index = UNDEFINED;
    int counter = 0;
    for (auto &iterator : ways_vec) {
        way_index = iterator.check(address,set) ? counter : way_index;
        counter++;
    }
    return way_index;
}

void level::updateLRU(unsigned way_index, unsigned set, Mode mode) {
    int index = 0;
    for (auto iterator : LRU_table.at(set)) {
        if (iterator == way_index) break;
        index++;
    }
    LRU_table.at(set).erase(LRU_table.at(set).begin() + index);
    const auto iterator = mode == BACK ? LRU_table.at(set).end() : LRU_table.at(set).begin();
    LRU_table.at(set).emplace(iterator,way_index);
}

bool level::accessLevel(unsigned address,bool is_write, tuple<unsigned,bool,bool>& removed ){
    unsigned set = extract(address, (size - b_size - assoc), STRAIGHTEN_BITS + b_size);
    int way_index = findWay(address, set);
    get<DEFINED>(removed) = false;
    get<DIRTY>(removed) = false;
    if (way_index == UNDEFINED) {
        if (is_write && !wr_alloc) return false;//NWA
        way_index = LRU_table.at(set).begin().operator*();
        ways_vec.at(way_index).swap(address, set, removed, is_write);
        updateLRU(way_index, set, BACK);
        return false;
    } else {
        updateLRU(way_index, set, BACK);
        return true;
    }
}

void level::removeFromLevel(unsigned int address){
    unsigned set = extract(address, (size - b_size - assoc), STRAIGHTEN_BITS + b_size);
    int way_index = findWay(address, set);
    if(way_index!=UNDEFINED) {
        ways_vec.at(way_index).undefineEntry(set);
        updateLRU(way_index, set, FRONT);
    }
}

bool level::read(unsigned address, tuple<unsigned,bool,bool>& removed){
    return accessLevel(address, false, removed);
}

bool level::write(unsigned address, tuple<unsigned,bool,bool>& removed){
    return accessLevel(address, true, removed);
}

void inline level::incHit(){ hits_num++;}

void inline level::incMiss(){ misses_num++;}

unsigned inline level::getStat(bool mode){
    return mode ? hits_num : misses_num;
}

bool level::checkHit(unsigned int address) {
    unsigned set = extract(address, (size - b_size - assoc), STRAIGHTEN_BITS + b_size);
    int way_index = findWay(address, set);
    return way_index != UNDEFINED;
}


//----------------------------------cache-------------------------------------//

cache::cache(unsigned init_param[9]):
        l1(level(init_param[B_SIZE],init_param[L1_SIZE],init_param[L1_ASSOC],init_param[WR_ALLOC])),
        l2(level(init_param[B_SIZE],init_param[L2_SIZE],init_param[L2_ASSOC],init_param[WR_ALLOC])),
        l1_cyc(init_param[L1_CYC]), l2_cyc(init_param[L2_CYC]), mem_cyc(init_param[MEM_CYC]),
        requests(0), mem_requests(0){};

void cache::writeToCache(unsigned long int address){
    requests++;
    tuple<unsigned, bool, bool> removed;//0-address 1-defined 2-dirty
    if(l1.checkHit(address)){//found in l1
        l1.write(address, removed);//replaced in l1
        l1.incHit();
        //l2.write(address, removed);//we use inclusion so found==true for sure and is replaced without inc l2.hit, we
        // call here so l2.LRU is updated
        return;
    }
    //not found in l1
    if(l2.wr_alloc && get<DEFINED>(removed) && get<DIRTY>(removed))
        l2.write(get<ADDRESS>(removed),removed);//to update LRU (since there is inclusion here he is for sure already
        // in l2)
    l1.incMiss();
    if(l2.write(address, removed)){//found and replaced in l2
        l2.incHit();
        l1.write(address,removed);
        if(l2.wr_alloc && get<DEFINED>(removed) && get<DIRTY>(removed))
            l2.write(get<ADDRESS>(removed),removed);//to update LRU (since there is inclusion here he is for sure
            // already
        // in l2)
        return;
    }
    //not found in cache
    l2.incMiss();
    if(l2.wr_alloc && get<DEFINED>(removed)) {
        l1.removeFromLevel(get<ADDRESS>(removed));
    }
    l1.write(address,removed);
    if(l2.wr_alloc && get<DEFINED>(removed) && get<DIRTY>(removed))
        l2.write(get<ADDRESS>(removed),removed);//to update LRU (since there is inclusion here he is for sure already
    // in l2)
    mem_requests++;
}

void cache::readFromCache(unsigned long int address)
{
    requests++;
    tuple<unsigned, bool, bool> removed;//0-address 1-defined 2-dirty
    if (l1.checkHit(address)) {//found in l1
        l1.read(address,removed);
        l1.incHit();
        //if(removed.second){
          //  l2.read(removed.first,removed);//to update LRU (since there is inclusion here he is for sure already in
          //  l2)
        //}
        return;
    }
    //miss in l1
    l1.incMiss();
    if (l2.read(address,removed)) {//found in l2
        l2.incHit();
        l1.read(address,removed);
        if(get<DEFINED>(removed)&&get<DIRTY>(removed))
            l2.read(get<ADDRESS>(removed),removed);//to update LRU (since there is inclusion here he is for sure already in l2)
        return;
    }
    //miss in l2
    l2.incMiss();
    if(get<DEFINED>(removed)) {
        l1.removeFromLevel(get<ADDRESS>(removed));
    }
    l1.read(address,removed);
    if(get<DEFINED>(removed)&&get<DIRTY>(removed))
        l2.read(get<ADDRESS>(removed),removed);//to update LRU (since there is inclusion here he is for sure already in l2)
    mem_requests++;
}

void cache::printStats(){
    double access_l1 = l1.getStat(MISS)+l1.getStat(HIT);
    double access_l2 = l2.getStat(MISS)+l2.getStat(HIT);
    cout<<setprecision(3)<<fixed;
    cout<<"L1miss="<<((double)l1.getStat(MISS))/access_l1;
    cout<<" L2miss="<<((double)l2.getStat(MISS))/access_l2;
    cout<<" AccTimeAvg="<<(mem_requests*mem_cyc+access_l2*l2_cyc+access_l1*l1_cyc)/(double)requests;
    cout<<endl;
}

/*------------------------------------------------------------------------------
 *                      main cache sim
------------------------------------------------------------------------------*/

int main(int argc, char **argv) {

    if (argc < 19) {
        cerr << "Not enough arguments" << endl;
        return 0;
    }

    // Get input arguments

    // File
    // Assuming it is the first argument
    char* fileString = argv[1];
    ifstream file(fileString); //input file stream
    string line;
    if (!file || !file.good()) {
        // File doesn't exist or some other error
        cerr << "File not found" << endl;
        return 0;
    }

    unsigned mem_cyc = 0, b_size = 0, l1_size = 0, l2_size = 0, l1_assoc = 0,
            l2_assoc = 0, l1_cyc = 0, l2_cyc = 0, wr_alloc = 0;

    for (int i = 2; i < 19; i += 2) {
        string s(argv[i]);
        if (s == "--mem-cyc") {
            mem_cyc = atoi(argv[i + 1]);
        } else if (s == "--bsize") {
            b_size = atoi(argv[i + 1]);
        } else if (s == "--l1-size") {
            l1_size = atoi(argv[i + 1]);
        } else if (s == "--l2-size") {
            l2_size = atoi(argv[i + 1]);
        } else if (s == "--l1-cyc") {
            l1_cyc = atoi(argv[i + 1]);
        } else if (s == "--l2-cyc") {
            l2_cyc = atoi(argv[i + 1]);
        } else if (s == "--l1-assoc") {
            l1_assoc = atoi(argv[i + 1]);
        } else if (s == "--l2-assoc") {
            l2_assoc = atoi(argv[i + 1]);
        } else if (s == "--wr-alloc") {
            wr_alloc = atoi(argv[i + 1]);
        } else {
            cerr << "Error in arguments" << endl;
            return 0;
        }
    }
    unsigned init_params[9] = {mem_cyc,b_size,l1_size,l2_size,l1_assoc,l2_assoc,l1_cyc,l2_cyc,wr_alloc};
    cache our_cache(init_params);
    while (getline(file, line)) {

        stringstream ss(line);
        string address;
        char operation = 0; // read (R) or write (W)
        if (!(ss >> operation >> address)) {
            // Operation appears in an Invalid format
            cout << "Command Format error" << endl;
            return 0;
        }

        // DEBUG - remove this line
        cout << "operation: " << operation;

        string cutAddress = address.substr(2); // Removing the "0x" part of the address

        // DEBUG - remove this line
        cout << ", address (hex)" << cutAddress;

        unsigned long int num = 0;
        num = strtoul(cutAddress.c_str(), NULL, 16);

        // DEBUG - remove this line
        cout << " (dec) " << num << endl;

        operation == 'r' ? our_cache.readFromCache(num) : our_cache.writeToCache(num);

    }

    our_cache.printStats();

    return 0;
}
