// SPDX-License-Identifier: MIT
// Authored dispatch bodies with actual CPU layout and maintained charge helpers.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dispatch_preparations_generated.h"
#include "cases.inc"
typedef void(*Run)(CPUState*);
static unsigned long long checks;
static void compare(Run* functions,uint32_t pc,int64_t count,int64_t budget,int64_t deadline,unsigned alias) {
    CPUState initial,wanted,actual;
    memset(&initial,0xa5,sizeof(initial));
    initial.pc=alias?(pc&~0x40000000u):pc;
    initial.downcount=count;initial.cycle_budget=budget;initial.cycle_deadline_budget=deadline;
    for(unsigned r=0;r<32;++r)initial.gpr[r]=r*747796405u+2891336453u;
    memcpy(&wanted,&initial,sizeof wanted);functions[0](&wanted);
    for(unsigned i=1;i<4;++i){memcpy(&actual,&initial,sizeof actual);functions[i](&actual);assert(memcmp(&actual,&wanted,sizeof actual)==0);++checks;}
}
int main(void) {
    _Static_assert(sizeof(CPUState)==3552,"Actual core CPU layout");
    Run functions[][4]={
      {test_entry_original,test_entry_dense,test_entry_ranges,test_entry_both},
      {test_cold_original,test_cold_dense,test_cold_ranges,test_cold_both},
      {test_return_original,test_return_dense,test_return_ranges,test_return_both}};
    const uint32_t bases[]={0x80004000u,0xc04200d4u,0x80008000u};
    const int64_t counts[]={-17,-8,-2,-1,0,3,9};
    const int64_t budgets[]={0,1,2,8,16};
    const int64_t deadlines[]={-1,0,1,4,19};
    for(unsigned f=0;f<3;++f)
      for(uint32_t offset=0;offset<1152;++offset)
        for(unsigned c=0;c<7;++c)for(unsigned b=0;b<5;++b)for(unsigned d=0;d<5;++d) {
          uint32_t pc=bases[f]+offset-64;
          compare(functions[f],pc,counts[c],budgets[b],deadlines[d],0);
          compare(functions[f],pc^0x40000000u,counts[c],budgets[b],deadlines[d],0);
          compare(functions[f],pc|0x40000000u,counts[c],budgets[b],deadlines[d],1);
        }
    const uint32_t outside[]={0,1,3,4,0x7fffffffu,0x80000000u,0xbfffffffu,0xc0000000u,0xfffffffdu,0xfffffffeu,0xffffffffu};
    for(unsigned f=0;f<3;++f)for(unsigned p=0;p<sizeof outside/sizeof outside[0];++p)
      for(unsigned c=0;c<7;++c)for(unsigned b=0;b<5;++b)for(unsigned d=0;d<5;++d)
        compare(functions[f],outside[p],counts[c],budgets[b],deadlines[d],0);
    printf("Dispatch preparations: %llu exact CPU/budget/deadline comparisons\n",checks);
    return 0;
}
