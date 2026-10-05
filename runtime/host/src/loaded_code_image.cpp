// SPDX-License-Identifier: GPL-3.0-or-later
#include "loaded_code_image.h"
#include <algorithm>
#include <cstring>
#include <limits>
namespace bw_ic_code {
namespace {
bool span(std::size_t off, std::size_t count, std::size_t size) { return off <= size && count <= size-off; }
std::uint16_t u16(const std::uint8_t* p) { return std::uint16_t(p[0]) | std::uint16_t(p[1])<<8; }
std::uint32_t u32(const std::uint8_t* p) { return std::uint32_t(p[0]) | std::uint32_t(p[1])<<8 | std::uint32_t(p[2])<<16 | std::uint32_t(p[3])<<24; }
std::uint64_t u64(const std::uint8_t* p) { return std::uint64_t(u32(p)) | std::uint64_t(u32(p+4))<<32; }
void put64(std::uint8_t* p, std::uint64_t value) { for (unsigned i=0;i<8;++i) p[i]=std::uint8_t(value>>(8*i)); }
bool power2(std::uint32_t n) { return n && !(n&(n-1)); }
struct Section { std::uint32_t rva, extent, raw, raw_size, flags; };
bool inside(const Section& s, std::uint32_t rva, std::size_t size) { return rva >= s.rva && span(rva-s.rva,size,s.extent); }
const std::uint8_t* file_rva(const std::uint8_t* file, const Image& image,
    const std::vector<Section>& sections, std::uint32_t rva, std::size_t size) {
    if (span(rva,size,image.headers_size)) return file+rva;
    for (const auto& s:sections) if (rva>=s.rva && span(rva-s.rva,size,s.raw_size)) return file+s.raw+(rva-s.rva);
    return nullptr;
}
bool parse(const std::uint8_t* file,std::size_t size,std::uint64_t base,Image& image) {
    if (!file || size<64 || size>kMaxFile || !base || u16(file)!=0x5a4d) return false;
    const std::size_t nt=u32(file+0x3c);
    if (nt<64 || !span(nt,24,size) || u32(file+nt)!=0x4550 || u16(file+nt+4)!=0x8664) return false;
    const unsigned count=u16(file+nt+6), optional=u16(file+nt+20);
    if (!count || count>kMaxSections || optional<240 || !span(nt+24,optional,size)) return false;
    const auto* opt=file+nt+24;
    if (u16(opt)!=0x20b || u32(opt+108)<6 || u32(opt+108)>16) return false;
    image.preferred_base=u64(opt+24);image.image_size=u32(opt+56);image.headers_size=u32(opt+60);
    const auto align=u32(opt+32), file_align=u32(opt+36);
    if (!image.preferred_base || !image.image_size || image.image_size>kMaxFile || !image.headers_size ||
        image.headers_size>size || image.headers_size>image.image_size ||
        !power2(align) || align<4096 || align>1024*1024 || !power2(file_align) || file_align<512 || file_align>65536 ||
        align<file_align || image.image_size%align || base>std::numeric_limits<std::uint64_t>::max()-image.image_size) return false;
    const std::size_t table=nt+24+optional;
    if (!span(table,count*40,size) || !span(table,count*40,image.headers_size)) return false;
    std::vector<Section> sections;sections.reserve(count);
    for (unsigned i=0;i<count;++i) {
        const auto* p=file+table+i*40;const auto vs=u32(p+8), rva=u32(p+12), rs=u32(p+16), raw=u32(p+20), flags=u32(p+36);
        const std::uint64_t used=std::max(vs,rs);
        if (!used || !rva || rva%align || rva<image.headers_size || (rs && (raw%file_align || rs%file_align || raw<image.headers_size)) ||
            !span(raw,rs,size) || used>image.image_size || !span(rva,std::size_t(used),image.image_size)) return false;
        const std::uint64_t rounded=(used+align-1)&~std::uint64_t(align-1);
        if (!span(rva,std::size_t(rounded),image.image_size)) return false;
        Section s{rva,std::uint32_t(rounded),raw,rs,flags};
        for (const auto& old:sections) {
            if (std::uint64_t(rva)<std::uint64_t(old.rva)+old.extent && std::uint64_t(old.rva)<std::uint64_t(rva)+s.extent) return false;
            if (rs && old.raw_size && std::uint64_t(raw)<std::uint64_t(old.raw)+old.raw_size && std::uint64_t(old.raw)<std::uint64_t(raw)+rs) return false;
        }
        sections.push_back(s);
        if (flags&0x20000000u) {
            if (flags&(1u<<31) || !(flags&0x40000000u)) return false;
            ExecutableSpan code;code.rva=rva;code.bytes.assign(s.extent,0);
            if (rs) std::memcpy(code.bytes.data(),file+raw,rs);
            image.executable.push_back(std::move(code));
        }
    }
    if (image.executable.empty()) return false;
    const auto reloc_rva=u32(opt+152), reloc_size=u32(opt+156);
    if ((reloc_rva==0)!=(reloc_size==0)) return false;
    const std::uint64_t delta=base-image.preferred_base; // modulo64 is defined for downward ASLR too
    if (!reloc_size) return delta==0;
    const auto* reloc=file_rva(file,image,sections,reloc_rva,reloc_size);
    if (!reloc || !span(reloc_rva,reloc_size,image.image_size)) return false;
    std::size_t off=0;std::uint32_t entries=0;std::vector<std::uint32_t> changed;
    while (off<reloc_size) {
        if (!span(off,8,reloc_size)) return false;
        const auto page=u32(reloc+off), block=u32(reloc+off+4);
        if (page%4096 || page>=image.image_size || block<8 || block%2 || !span(off,block,reloc_size)) return false;
        for (std::size_t at=8;at<block;at+=2) {
            if (++entries>kMaxRelocations) return false;
            const auto value=u16(reloc+off+at);const unsigned type=value>>12;
            if (!type) continue;
            if (type!=10) return false;
            const std::uint64_t target=std::uint64_t(page)+(value&4095);
            if (target>std::numeric_limits<std::uint32_t>::max() || !span(std::size_t(target),8,image.image_size)) return false;
            const auto rva=std::uint32_t(target);
            bool allocated=span(rva,8,image.headers_size);
            for (const auto& s:sections) allocated|=inside(s,rva,8);
            if (!allocated) return false;
            // Repeated/overlapping relocation writes are malformed, even outside code.
            changed.push_back(rva);
            for (auto& code:image.executable) if (rva>=code.rva && span(rva-code.rva,8,code.bytes.size())) {
                auto* p=code.bytes.data()+(rva-code.rva);put64(p,u64(p)+delta);
            }
        }
        off+=block;
    }
    std::sort(changed.begin(),changed.end());
    for (std::size_t i=1;i<changed.size();++i) if (std::uint64_t(changed[i-1])+8>changed[i]) return false;
    return true;
}
} // namespace
bool build_expected(const std::uint8_t* file,std::size_t size,std::uint64_t base,Image& out) noexcept {
    out={};try { Image next;if(!parse(file,size,base,next)) return false;out=std::move(next);return true; } catch (...) { out={};return false; }
}
bool executable_contains(const Image& image,std::uint32_t rva,std::size_t size) noexcept {
    if (!size) return false;
    for (const auto& code:image.executable) if (rva>=code.rva && span(rva-code.rva,size,code.bytes.size())) return true;
    return false;
}
} // namespace bw_ic_code
