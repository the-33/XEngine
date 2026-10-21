// Standalone SDL-free microbenchmark of v14.1 vs v15 contact indexing.
// Compile: g++ -std=c++20 -O2 tools/benchmark_contact_merge_cpu.cpp -o contact_merge_bench
// Models only index insertion/reservation and ordered contact-copy overhead;
// it does NOT measure the complete XEngine merge, callbacks, or frame time.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>
struct Data { uint64_t key; int a, b; };
struct Info { int a,b; };
static volatile uint64_t sink = 0;
static double measure(size_t n, bool newer){
 std::vector<Data> data; data.reserve(n);
 for(size_t i=0; i<n; ++i) data.push_back({(uint64_t(i+1)<<32)|uint64_t(i+13),int(i),int(i*3)});
 std::unordered_map<uint64_t,Info> index;
 std::unordered_set<uint64_t> oldset;
 std::vector<Data> output;
 output.reserve(128);
 auto start=std::chrono::steady_clock::now();
 if(newer){ index.reserve(n); output.reserve(n); }
 for(const auto& row :data){
   output.emplace_back(row);
   if(newer) index.insert_or_assign(row.key,Info{row.a,row.b});
   else { oldset.insert(row.key); index[row.key] = Info{row.a,row.b}; }
 }
 uint64_t checksum=0;
 for(const auto& x :output) checksum += x.key;
 sink=checksum;
 return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
int main(){
 for(size_t n : {10000u,100000u,450000u}){
  std::vector<double> a,b;
  for(int i=0;i<9;++i){ if(i%2){b.push_back(measure(n,true));a.push_back(measure(n,false));} else{a.push_back(measure(n,false));b.push_back(measure(n,true));} }
  std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());
  std::cout<<n<<" "<<a[4]<<" "<<b[4]<<" "<<(a[4]-b[4])/a[4]*100<<"%\n";
 }
}
