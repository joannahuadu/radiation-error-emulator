#ifndef __MEMORY_H
#define __MEMORY_H

#include "Config.h"
#include <vector>
#include <functional>
#include <cmath>
#include <cassert>
#include <tuple>
#include <random>
#include <algorithm>
#include <set>
using namespace std;
typedef vector<unsigned int> MapSrcVector;
typedef map<unsigned int, MapSrcVector > MapSchemeEntry;
typedef map<unsigned int, MapSchemeEntry> MapScheme;

namespace famulator
{

class MemoryBase{
public:
    MemoryBase() {}
    virtual ~MemoryBase() {}
    virtual void getAddressInfo(uintptr_t s_paddr, uintptr_t e_paddr) = 0;
    virtual void offset(vector<uintptr_t>& errors, int ro_ref, int co_ref, int error_bit_num, int seed) = 0;
};

// template <class T, template<typename> class Controller = Controller >
template <class T>
class Memory : public MemoryBase
{

protected:
  long max_paddr;
  MapScheme mapping_scheme;
  
public:
    // enum class Type {
    //     ChRaBaRoCo,
    //     RoBaRaCoCh,
    //     MAX,
    // } type = Type::RoBaRaCoCh;

    // vector<Controller<T>*> ctrls;
    T * spec;
    vector<int> addr_bits;
    // string mapping_file;
    // bool use_mapping_file;
    bool dump_mapping;
    vector<int> s_vec;
    vector<int> e_vec;
    int s_ro;
    int e_ro;
    int s_co;
    int e_co;
    int byte_idx;
    // int ofs_bits;
    vector<vector<int>> has_ch_ra_ba_xor;
    vector<vector<int>> has_ch_ra_ba;
    
    Memory(T* spec, const Config& configs)
        : spec(spec),
          addr_bits(int(T::Level::MAX))
    {
        // make sure 2^N channels/ranks
        int *sz = spec->org_entry.count;
        if (((sz[0] & (sz[0] - 1)) != 0) || ((sz[1] & (sz[1] - 1)) != 0)) {
            std::cerr << "Make sure 2^N channels/ranks" << std::endl;
            return;
        }
        // the least significant bits of the physical address are not used for addressing (serve as byte index)
        assert(spec->org_entry.dq * 8 == spec->channel_width);
        byte_idx = calc_log2(spec->org_entry.dq);
        assert((1<<byte_idx) ==spec->org_entry.dq);
        // bit_idx = calc_log2(spec->channel_width);

        // Parsing mapping file and initialize mapping table
        dump_mapping = true;
        init_mapping_with_file(configs["mapping"]);
        
        // If hi address bits will not be assigned to Rows
        // then the chips must not be LPDDRx 6Gb, 12Gb etc.
        // if (type != Type::RoBaRaCoCh && spec->standard_name.substr(0, 5) == "LPDDR")
        //     assert((sz[int(T::Level::Row)] & (sz[int(T::Level::Row)] - 1)) == 0);

        for (unsigned int lev = 0; lev < addr_bits.size(); lev++) {
            addr_bits[lev] = calc_log2(sz[lev]);
        }
    }

    ~Memory()
    {
        delete spec;
    }


    void getAddressInfo(uintptr_t s_paddr, uintptr_t e_paddr) {
        s_vec.resize(addr_bits.size());
        e_vec.resize(addr_bits.size());
        // cout << hex << s_paddr << " " << e_paddr << endl;
        s_paddr = clear_lower_bits(s_paddr, byte_idx);
        e_paddr = clear_lower_bits(e_paddr, byte_idx);
        apply_mapping(s_paddr, e_paddr);
    }


    void offset(vector<uintptr_t>& errors, int ro_ref, int co_ref, int error_bit_num, int seed){
        // cout <<"[offset]row and column range: "<< dec << s_ro << " " << e_ro <<" " << s_co << " " << e_co << endl;
        vector<ErrorIndex> error_index_map = selectErrorBits(ro_ref, co_ref, error_bit_num, seed);
        // cout<<"[error bit map](row, column) of error bits: "<<endl;
        // for (const auto& index : error_index_map) {
        //     std::cout << "(" << index.row << ", " << index.column << ") ";   
        // }
        // cout<<endl;
        
        int64_t offset_byte;
        int64_t offset_item;
        int64_t ofs = 0;
        mt19937 gen;
        gen.seed(seed);
        std::uniform_int_distribution<int64_t> bytes_rand(0, (1<<byte_idx)-1);
        offset_byte = bytes_rand(gen);
        // cout<<"offset_byte: "<<offset_byte<<endl;
        
        int64_t single_bit;
        vector<int> ref_xor_base(int(T::Level::MAX));
        vector<int> xor_base(int(T::Level::MAX));
        //channel/bank/rank offset
        for(int lvl = 0; lvl < int(T::Level::MAX)-2 ; lvl++){
            // cout<<spec->level_str[lvl]<<": ";
            if (has_ch_ra_ba_xor[lvl].size() == 0){
                //TODO: bank、channel and rank in has_ch_ra_ba are fixed. (ref_xor_base) no need to randomly set.
                for(const auto& elem : has_ch_ra_ba[lvl]){
                    std::uniform_int_distribution<int64_t> bit_rand(0,1);
                    // cout<<" [elements] "<<elem<<" ";
                    single_bit = bit_rand(gen);
                    // cout<<"random single_bit: "<<single_bit<<" ";
                    ofs += (single_bit<<elem);
                    // cout<<"ofs: "<<ofs<<endl;
                }
            }else{
                //TODO: bank、channel and rank in has_ch_ra_ba are fixed. (ref_xor_base) no need to randomly set.
                // (xor_base) is fixed. only one. 
                for (int i = 0; i<has_ch_ra_ba[lvl].size(); i++){
                    std::uniform_int_distribution<int64_t> bit_rand(0,1);
                    // cout<<" [ref_xor_base] "<<has_ch_ra_ba[lvl][i]<<" ";
                    single_bit = bit_rand(gen);
                    // cout<<"random single_bit: "<<single_bit<<" ";
                    ref_xor_base[lvl] += (single_bit<<i);
                    // cout<<"ref_xor_base: "<<ref_xor_base[lvl]<<" ";
                    xor_base[lvl] += (1<<i);
                    // cout<<"xor_base: "<<xor_base[lvl]<<endl;
                }
            }
            // cout<<endl;
        }
        vector<int>ref_xor_result(int(T::Level::MAX));
        vector<int>ref_xor_index = calculate_xor(co_ref, ro_ref);
        // cout<<"ref_xor_result: ";
        for(int lvl=0; lvl<int(T::Level::MAX)-2; lvl++){
            ref_xor_result[lvl] = ref_xor_index[lvl] xor ref_xor_base[lvl];
            // cout<<ref_xor_result[lvl]<<" ";
        }
        // cout<<endl;
        
        //channel/bank/rank offset (for xor, each index has unique xor offset)
        for(const auto& index : error_index_map){
            int64_t xor_offset=0;
            bool flag = true;
            int64_t co_offset_item = index.column;
            int64_t ro_offset_item = index.row;
            vector<int> index_xor_index;
            vector<int64_t> index_xor_result(int(T::Level::MAX));
            index_xor_index = calculate_xor(co_offset_item, ro_offset_item);
            for (int lvl = 0; lvl<int(T::Level::MAX)-2; lvl++){
                // cout<<spec->level_str[lvl]<<": ";
                index_xor_result[lvl] = index_xor_index[lvl] xor ref_xor_result[lvl];
                // cout<<"index_xor_result: "<<index_xor_result[lvl]<<" ";
                if ((index_xor_result[lvl] > xor_base[lvl])){
                    //TODO index_xor_result[lvl] != xor_base[lvl]
                    flag = false;
                    break;
                }else{
                    for (int i = 0; i<has_ch_ra_ba[lvl].size(); i++){
                        xor_offset += calculate_level_ofs(index_xor_result[lvl],has_ch_ra_ba[lvl][i]);
                        // cout << "xor_offset: "<<xor_offset<<" ";
                    }
                }
            }
            // cout<<"flag: "<<flag<<endl; 
            if (index.row != -1 && index.column != -1) {
                offset_item = (ro_offset_item << mapping_scheme[int(T::Level::Row)][0][0]) +
                            (co_offset_item << mapping_scheme[int(T::Level::Column)][0][0]) +
                            ofs + xor_offset;
            } else if (index.column != -1) {
                // std::cout << "e_ro: " << e_ro << std::endl;
                offset_item = (e_ro << mapping_scheme[int(T::Level::Row)][0][0]) + (co_offset_item << mapping_scheme[int(T::Level::Column)][0][0]) + ofs + xor_offset;
            } else {
                throw std::invalid_argument("Invalid row index and column index");
            }

            offset_item = (offset_item<<byte_idx)+offset_byte; 
            // if (offset_item >= page_size*8) {
                // throw std::runtime_error("Page Overflow! Invalid row index and column index!");
            // }
            if(flag){
                errors.push_back(offset_item);
            }
        }
    }

    void init_mapping_with_file(string filename){
        ifstream file(filename);
        // possible line types are:
        // 0. Empty line
        // 1. Direct bit assignment   : component N   = x
        // 2. Direct range assignment : component N:M = x:y
        // 3. XOR bit assignment      : component N   = x y z ...
        // 4. Comment line            : # comment here
        string line;
        char delim[] = " \t";
        while (getline(file, line)) {
            short capture_flags = 0;
            int level = -1;
            int target_bit = -1, target_bit2 = -1;
            int source_bit = -1, source_bit2 = -1;
            // cout << "Processing: " << line << endl;
            bool is_range = false;
            while (true) { // process next word
                size_t start = line.find_first_not_of(delim);
                if (start == string::npos) // no more words
                    break;
                size_t end = line.find_first_of(delim, start);
                string word = line.substr(start, end - start);
                
                if (word.at(0) == '#')// starting a comment
                    break;
                if (word == "Burst_length"){
                    line = line.substr(end);
                    start = line.find_first_not_of(delim);
                    if (start == string::npos) // no more words
                        break;
                    end = line.find_first_of(delim, start);
                    word = line.substr(start, end - start);
                    spec->prefetch_size = stoi(word);
                    break;
                }
                size_t col_index;
                int source_min, target_min, target_max;
                switch (capture_flags){
                    case 0: // capturing the component name
                        // fetch component level from channel spec
                        for (int i = 0; i < int(T::Level::MAX); i++)
                            if (word.find(T::level_str[i]) != string::npos) {
                                level = i;
                                capture_flags ++;
                            }
                        break;

                    case 1: // capturing target bit(s)
                        col_index = word.find(":");
                        if ( col_index != string::npos ){
                            target_bit2 = stoi(word.substr(col_index+1));
                            word = word.substr(0,col_index);
                            is_range = true;
                        }
                        target_bit = stoi(word);
                        capture_flags ++;
                        break;

                    case 2: //this should be the delimiter
                        assert(word.find("=") != string::npos);
                        capture_flags ++;
                        break;

                    case 3:
                        if (is_range){
                            col_index = word.find(":");
                            source_bit  = stoi(word.substr(0,col_index));
                            source_bit2 = stoi(word.substr(col_index+1));
                            assert(source_bit2 - source_bit == target_bit2 - target_bit);
                            source_min = min(source_bit, source_bit2);
                            target_min = min(target_bit, target_bit2);
                            target_max = max(target_bit, target_bit2);
                            while (target_min <= target_max){
                                mapping_scheme[level][target_min].push_back(source_min);
                                // cout << target_min << " <- " << source_min << endl;
                                source_min ++;
                                target_min ++;
                            }
                        }
                        else {
                            source_bit = stoi(word);
                            mapping_scheme[level][target_bit].push_back(source_bit);
                        }
                }
                if (end == string::npos) { // this is the last word
                    break;
                }
                line = line.substr(end);
            }
        }
        // if (dump_mapping)
        //     dump_mapping_scheme();
    }
    
    void dump_mapping_scheme(){
        cout << "Mapping Scheme: " << endl;
        for (MapScheme::iterator mapit = mapping_scheme.begin(); mapit != mapping_scheme.end(); mapit++)
        {
            int level = mapit->first;
            for (MapSchemeEntry::iterator entit = mapit->second.begin(); entit != mapit->second.end(); entit++){
                cout << T::level_str[level] << "[" << entit->first << "] := ";
                cout << "PhysicalAddress[" << *(entit->second.begin()) << "]";
                // entit->second.erase(entit->second.begin());
                for (MapSrcVector::iterator it = next(entit->second.begin()); it != entit->second.end(); it ++)
                    cout << " xor PhysicalAddress[" << *it << "]";
                cout << endl;
            }
        }
    }

    void apply_mapping(uintptr_t s_paddr, uintptr_t e_paddr){
        has_ch_ra_ba_xor.resize(int(T::Level::MAX));
        has_ch_ra_ba.resize(int(T::Level::MAX));
        int co_range_max = mapping_scheme[int(T::Level::Column)].size() - 1;
        int ro_range_max = mapping_scheme[int(T::Level::Row)].size() - 1; 
        // row and column range
        // cout << hex << s_paddr << " " << e_paddr << endl;
        s_ro = get_bit_at_range(s_paddr, mapping_scheme[int(T::Level::Row)][0][0], mapping_scheme[int(T::Level::Row)][ro_range_max][0]);
        e_ro = get_bit_at_range(e_paddr, mapping_scheme[int(T::Level::Row)][0][0], mapping_scheme[int(T::Level::Row)][ro_range_max][0]);
        s_co = get_bit_at_range(s_paddr, mapping_scheme[int(T::Level::Column)][0][0], mapping_scheme[int(T::Level::Column)][co_range_max][0]);
        e_co = get_bit_at_range(e_paddr, mapping_scheme[int(T::Level::Column)][0][0], mapping_scheme[int(T::Level::Column)][co_range_max][0]);
        // cout <<"row and column range: "<< dec << s_ro << " " << e_ro <<" " << s_co << " " << e_co << endl;

        // xor index for channel and bank
        for (int lvl = 0; lvl < int(T::Level::MAX); lvl++){
            // cout<<spec->level_str[lvl]<<endl;
            for(MapSchemeEntry::iterator entit = mapping_scheme[lvl].begin(); entit!=mapping_scheme[lvl].end(); entit++){
                for (MapSrcVector::iterator it = entit->second.begin(); it != entit->second.end(); it ++){
                    if ((1<<static_cast<int>(*it)) <= e_paddr){
                        if((((*it) <= mapping_scheme[int(T::Level::Column)][co_range_max][0]) && \
                            ((*it) >= mapping_scheme[int(T::Level::Column)][0][0])) ||\
                            (((*it) <= mapping_scheme[int(T::Level::Row)][ro_range_max][0]) && \
                            ((*it) >= mapping_scheme[int(T::Level::Row)][0][0]))){
                                e_vec[lvl]+=1;
                                if(lvl < int(T::Level::Row)){ 
                                    has_ch_ra_ba_xor[lvl].push_back(*it);
                                    // cout << "[xor]" << *it << " ";
                                }
                        }else{
                            e_vec[lvl]+=1;
                            if(lvl < int(T::Level::Row)){
                                has_ch_ra_ba[lvl].push_back(*it);
                                // cout << "[nxor]" << *it << " ";
                            }
                        }
                    }
                    if((1<<static_cast<int>(*it)) <= s_paddr){
                        if((((*it) <= mapping_scheme[int(T::Level::Column)][co_range_max][0]) && \
                            ((*it) >= mapping_scheme[int(T::Level::Column)][0][0])) ||\
                            (((*it) <= mapping_scheme[int(T::Level::Row)][ro_range_max][0]) && \
                            ((*it) >= mapping_scheme[int(T::Level::Row)][0][0]))){
                                s_vec[lvl]+=1;
                        }else{
                                s_vec[lvl]+=1;
                        }
                    }
                }
            }
            // cout << endl;
        }
    }

private:
    struct ErrorIndex {
        int row;
        int column;
        bool operator==(const ErrorIndex& other) const {
            return (row == other.row) && (column == other.column);
        }
    };

    bool contains(const std::vector<ErrorIndex>& vec, const ErrorIndex& index) {
        return std::find(vec.begin(), vec.end(), index) != vec.end();
    }
    bool isedge(int& row, int& column){
        if(row==-1){
            if(column <= s_co) column = s_co;
            if(column >= e_co) column = e_co;
            return (column <= s_co || column >= e_co);
        }
        else{
            if(row <= s_ro) row = s_ro;
            if(row >= e_ro) row = e_ro;
            if(column <= s_co) column = s_co;
            if(column >= e_co) column = e_co;
            return (row <= s_ro || row >= e_ro || column <= s_co || column >= e_co);
        }
    }

    std::vector<ErrorIndex> selectErrorBits(int ro_ref, int co_ref, int error_bit_num, int seed) {
        std::vector<ErrorIndex> error_index_map;
        std::mt19937 gen;
        gen.seed(seed);
        std::uniform_int_distribution<int> dist(0, 1);
        std::uniform_real_distribution<double> ran(0, 1);
        if (ro_ref!=-1 && co_ref!=-1){
            int i=0;
            while (i < error_bit_num) {
                ErrorIndex index;
                index.row = ro_ref;
                index.column = co_ref;
                if((!contains(error_index_map, index)) && (!isedge(ro_ref, co_ref))){
                    error_index_map.push_back(index);
                    i++;
                }
                double rand_num = ran(gen); //multiple events tend to occur along the wordline
                if ((rand_num >= 0.2) || ro_ref == e_ro){
                    if(ro_ref == e_ro)
                        ro_ref -= 1;
                    int dx = dist(gen) == 0 ? -1 : 1;  // left or right
                    co_ref += dx;
                }else{
                    int dy = dist(gen) == 0 ? -1 : 1;  // up or down
                    ro_ref += dy;
                }
            }
            
        }else if(ro_ref==-1 && co_ref!=-1){
            int i=0;
            while (i < error_bit_num) {
                ErrorIndex index;
                index.row = ro_ref;
                index.column = co_ref;
                if(!contains(error_index_map, index) && !isedge(ro_ref, co_ref)){
                    error_index_map.push_back(index);
                    i++;
                }
                int dx = dist(gen) == 0 ? -1 : 1;  // left or right
                co_ref += dx;
            }
        }

        return error_index_map;
    }

    int calc_log2(int64_t val){
        int n = 0;
        while ((val >>= 1))
            n ++;
        return n;
    }
    int has_level_bits(int& p_bits , int bits)
    {   
        if (p_bits>bits){
            p_bits = p_bits - bits;
            return bits;
        }else if(p_bits>0){
            bits = p_bits;
            p_bits = p_bits - bits;
            return bits;
        }else{
            return 0;
        }
    }
    // int slice_lower_bits(uint64_t& addr, int bits)
    // {
    //     int lbits = addr & ((1<<bits) - 1);
    //     addr >>= bits;
    //     return lbits;
    // }
    // int slice_lower_bits(uintptr_t addr, int bits)
    // {  
    //     int lbits = addr & (1<<bits);
    //     lbits >>= bits;
    //     return lbits;
    // }
    bool get_bit_at(uintptr_t addr, int bit)
    {
        // cout << "get_bit_at " << bit << " ";
        return (((addr >> bit) & 1) == 1);
    }
    uintptr_t clear_lower_bits(uintptr_t addr, int bits)
    {
        return addr>>bits;
    }
    int get_bit_at_range(uintptr_t addr, int low, int high)
    {
        uintptr_t addr_  = clear_lower_bits(addr, low);
        return  addr_ & ((1<<(high-low+1))-1);
    }
    void set_range(uintptr_t& addr, int range)
    {
        addr &= ((1<<(range+1))-1);
    }
    long lrand(void) {
        if(sizeof(int) < sizeof(long)) {
            return static_cast<long>(rand()) << (sizeof(int) * 8) | rand();
        }

        return rand();
    }
    // int calculate_xor_offset(int& index, int bits){
    //     int lbits = index << bits;
        
    //     int lbits = index & ((1<<bits)-1);
    //     index >>= bits;
    //     index <<= (bits+1);
    //     index += lbits;
    //     return index;
    // }

    int64_t calculate_level_ofs(int64_t& level, int elem){
        int64_t offset;
        offset = (level << elem) & ((1<<(elem+1))-1);
        level >>= 1;
        return offset;
    }
    vector<int> calculate_xor(int co, int ro){
        int co_range_max = mapping_scheme[int(T::Level::Column)].size() - 1;
        int ro_range_max = mapping_scheme[int(T::Level::Row)].size() - 1; 
        vector<int>xor_index(int(T::Level::MAX));
        for(int lvl = 0; lvl < int(T::Level::MAX)-2 ; lvl++){
            // cout<<spec->level_str[lvl]<<": ";
            int count = 0;
            // cout<<" [xor_index] ";
            for (auto elem : has_ch_ra_ba_xor[lvl]){
                if(elem < mapping_scheme[int(T::Level::Column)][co_range_max][0] && elem>mapping_scheme[int(T::Level::Column)][0][0]){
                    // cout<<"co" << " ";
                    xor_index[lvl] += get_bit_at(co, elem-mapping_scheme[int(T::Level::Column)][0][0]) << count;
                    // cout<<xor_index[lvl]<<" ";
                }
                else if(elem < mapping_scheme[int(T::Level::Row)][ro_range_max][0] && elem>mapping_scheme[int(T::Level::Row)][0][0]){
                    // cout<<"ro" << " ";
                    xor_index[lvl] += get_bit_at(ro, elem-mapping_scheme[int(T::Level::Row)][0][0]) << count;
                    // cout<<xor_index[lvl]<<" ";
                }
                count ++;  
            }
            // cout<<endl;
        }
        return xor_index;
    }
};

} /*namespace famulator*/

#endif /*__MEMORY_H*/
