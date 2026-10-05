// SPDX-License-Identifier: GPL-3.0-or-later
// Source-only staged synthetic PE fixture. No game bytes or loader calls.
#include "loaded_code_image.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks=0;
#define CHECK(v) do { ++checks; if(!(v)) { std::fprintf(stderr,"line%d\n",__LINE__); std::abort(); } } while(0)
void w16(std::vector<unsigned char>& b,std::size_t o,unsigned v){ b[o]=v;b[o+1]=v>>8; }
void w32(std::vector<unsigned char>& b,std::size_t o,unsigned v){ for(unsigned i=0;i<4;++i)b[o+i]=v>>(8*i); }
void w64(std::vector<unsigned char>& b,std::size_t o,std::uint64_t v){for(unsigned i=0;i<8;++i)b[o+i]=v>>(8*i);}
std::vector<unsigned char> pe(){
    std::vector<unsigned char> b(1536,0);w16(b,0,0x5a4d);w32(b,0x3c,128);w32(b,128,0x4550);
    w16(b,132,0x8664);w16(b,134,2);w16(b,148,240);
    const unsigned o=152;w16(b,o,0x20b);w64(b,o+24,0x180000000ull);w32(b,o+32,4096);w32(b,o+36,512);
    w32(b,o+56,12288);w32(b,o+60,512);w32(b,o+108,16);w32(b,o+152,8192);w32(b,o+156,12);
    const unsigned s=392;w32(b,s+8,16);w32(b,s+12,4096);w32(b,s+16,512);w32(b,s+20,512);w32(b,s+36,0x60000020);
    w32(b,s+40+8,12);w32(b,s+40+12,8192);w32(b,s+40+16,512);w32(b,s+40+20,1024);w32(b,s+40+36,0x40000040);
    w64(b,520,0x180002345ull);w32(b,1024,4096);w32(b,1028,12);w16(b,1032,0xa008);w16(b,1034,0);
    return b;
}
// One original DIR64 plus legal ABS padding. The larger directory stays in
// its own non-executable section; no game bytes or mapped-image operations.
std::vector<unsigned char> large_relocation_pe(std::uint32_t entries){
    auto b=pe();
    const std::uint32_t directory_size=8u+2u*entries;
    const std::uint32_t raw_size=(directory_size+511u)&~511u;
    const std::uint32_t extent=(raw_size+4095u)&~4095u;
    b.resize(1024u+raw_size,0);
    w32(b,152+56,8192u+extent);
    w32(b,152+156,directory_size);
    w32(b,392+40+8,directory_size);
    w32(b,392+40+16,raw_size);
    w32(b,1028,directory_size);
    return b;
}
std::uint64_t r64(const std::vector<unsigned char>& b,std::size_t off){std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(b[off+i])<<(8*i);return v;}
}
int main(){
    const std::uint64_t native=0x180000000ull;auto b=pe();bw_ic_code::Image image;
    CHECK(bw_ic_code::build_expected(b.data(),b.size(),native,image));
    CHECK(image.image_size==12288&&image.executable.size()==1&&image.executable[0].bytes.size()==4096);
    CHECK(r64(image.executable[0].bytes,8)==native+0x2345);
    CHECK(image.executable[0].bytes[512]==0&&image.executable[0].bytes[4095]==0);
    CHECK(bw_ic_code::executable_contains(image,4096,4096));
    CHECK(!bw_ic_code::executable_contains(image,4095,1));
    CHECK(!bw_ic_code::executable_contains(image,8191,2));
    CHECK(!bw_ic_code::executable_contains(image,8192,1));
    CHECK(bw_ic_code::build_expected(b.data(),b.size(),native+0x10000,image));
    CHECK(r64(image.executable[0].bytes,8)==native+0x12345);
    CHECK(bw_ic_code::build_expected(b.data(),b.size(),native-0x10000,image));
    CHECK(r64(image.executable[0].bytes,8)==native-0x10000+0x2345);
    for(std::size_t size=0;size<512;++size) {
        CHECK(!bw_ic_code::build_expected(b.data(),size,native,image));
        CHECK(image.executable.empty()&&image.image_size==0);
    }
    const auto reject=[&](std::vector<unsigned char> broken){CHECK(!bw_ic_code::build_expected(broken.data(),broken.size(),native+4096,image));CHECK(image.executable.empty()&&image.image_size==0);};
    auto x=b;w32(x,0x3c,0xfffffff0);reject(x);
    x=b;w16(x,132,0x14c);reject(x);
    x=b;w16(x,134,97);reject(x);
    x=b;w16(x,148,8);reject(x);
    x=b;w16(x,152,0x10b);reject(x);
    x=b;w32(x,152+108,0xffffffff);reject(x);
    x=b;w32(x,152+56,4096);reject(x);
    x=b;w32(x,152+32,3000);reject(x);
    x=b;w32(x,392+12,4097);reject(x);
    x=b;w32(x,392+36,0xe0000020);reject(x); // writable executable
    x=b;w32(x,392+40+12,4096);reject(x); // overlapping virtual sections
    x=b;w32(x,392+40+20,512);reject(x); // overlapping raw sections
    x=b;w32(x,152+152,0);reject(x); // one-sided relocation directory
    x=b;w32(x,1028,7);reject(x);
    x=b;w16(x,1032,0x3008);reject(x); // unsupported HIGHLOW
    x=b;w16(x,1034,0xa00c);reject(x); // overlapping DIR64
    x=b;w32(x,1024,0xfffff000);reject(x);
    x=b;w32(x,1024,0);w16(x,1032,0xa100); // legal header relocation outside executable bytes
    CHECK(bw_ic_code::build_expected(x.data(),x.size(),native+4096,image));
    x=b;w32(x,152+152,0);w32(x,152+156,0);
    CHECK(bw_ic_code::build_expected(x.data(),x.size(),native,image));
    CHECK(!bw_ic_code::build_expected(x.data(),x.size(),native+4096,image));
    CHECK(!bw_ic_code::build_expected(b.data(),b.size(),0,image));
    CHECK(!bw_ic_code::build_expected(b.data(),b.size(),~std::uint64_t(0)-100,image));
    // Same metadata/layout but a native executable byte differs: the expected
    // image must differ; an actual mapped-image comparison must reject it.
    auto a=pe(),c=a;c[513]=0x42;bw_ic_code::Image first,second;
    CHECK(bw_ic_code::build_expected(a.data(),a.size(),native,first));
    CHECK(bw_ic_code::build_expected(c.data(),c.size(),native,second));
    CHECK(first.executable[0].bytes!=second.executable[0].bytes);
    // The approved real module exceeds the former 1,048,576-entry bound.
    // Synthetic boundary cases verify finite accounting includes ABS padding
    // and still applies the original DIR64 for native/upward/downward bases.
    {
        auto large=large_relocation_pe(1048577u);
        CHECK(bw_ic_code::build_expected(large.data(),large.size(),native,image));
        CHECK(r64(image.executable[0].bytes,8)==native+0x2345);
        CHECK(bw_ic_code::build_expected(large.data(),large.size(),native+0x10000,image));
        CHECK(r64(image.executable[0].bytes,8)==native+0x12345);
        CHECK(bw_ic_code::build_expected(large.data(),large.size(),native-0x10000,image));
        CHECK(r64(image.executable[0].bytes,8)==native-0x10000+0x2345);
    }
    {
        auto large=large_relocation_pe(4194304u);
        CHECK(bw_ic_code::build_expected(large.data(),large.size(),native,image));
        CHECK(r64(image.executable[0].bytes,8)==native+0x2345);
        CHECK(bw_ic_code::build_expected(large.data(),large.size(),native+0x10000,image));
        CHECK(r64(image.executable[0].bytes,8)==native+0x12345);
        CHECK(bw_ic_code::build_expected(large.data(),large.size(),native-0x10000,image));
        CHECK(r64(image.executable[0].bytes,8)==native-0x10000+0x2345);
    }
    {
        auto large=large_relocation_pe(4194305u);
        // The previous accepted image is populated; failure must clear it.
        CHECK(!image.executable.empty());
        CHECK(!bw_ic_code::build_expected(large.data(),large.size(),native+0x10000,image));
        CHECK(image.executable.empty()&&image.image_size==0&&image.headers_size==0&&image.preferred_base==0);
    }
    std::printf("SYNTHETIC_PE_IMAGE checks=%u; no loaded-module/native proof\n",checks);
}
