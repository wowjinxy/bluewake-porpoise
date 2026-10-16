#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Value-only, bounded readers for a fresh native disc archive. These never open
// a JKR volume and never inspect/copy the already-mutated local J3D backing.
namespace pc_remote_asset {
struct Bytes { const uint8_t* data; size_t size; };
inline bool range(size_t offset, size_t count, size_t size) {
    return offset <= size && count <= size - offset;
}
inline uint32_t be32(const uint8_t* p) {
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
inline uint16_t be16(const uint8_t* p) { return uint16_t(p[0]) << 8 | p[1]; }

// Only the actual Yaz0 format is supported initially; unknown/Yay0 fails closed.
// Unlike the general game decoder, every read, back-reference and write is bound.
inline bool yaz0(Bytes src, uint8_t* dst, size_t capacity, size_t& written) {
    written = 0;
    if (!src.data || !dst || src.size < 16 || memcmp(src.data, "Yaz0", 4)) return false;
    const size_t target = be32(src.data + 4);
    if (!target || target > capacity) return false;
    size_t in = 16, out = 0;
    uint8_t bits = 0, code = 0;
    while (out < target) {
        if (!bits) {
            if (in >= src.size) return false;
            code = src.data[in++]; bits = 8;
        }
        if (code & 0x80) {
            if (in >= src.size) return false;
            dst[out++] = src.data[in++];
        } else {
            if (!range(in, 2, src.size)) return false;
            const uint8_t a = src.data[in++], b = src.data[in++];
            const size_t distance = ((size_t(a & 15) << 8) | b) + 1;
            size_t count = a >> 4;
            if (!count) {
                if (in >= src.size) return false;
                count = size_t(src.data[in++]) + 0x12;
            } else count += 2;
            if (distance > out || count > target - out) return false;
            for (size_t i = 0; i < count; ++i) { dst[out] = dst[out - distance]; ++out; }
        }
        code <<= 1; --bits;
    }
    written = out;
    return true;
}

// RARC fields/entry width mirror JKRArchive.h. Select the generated INDEX,
// not a runtime pointer/file ID, and require the authored CL file name.
inline bool cl_resource(Bytes arc, uint32_t index, Bytes& result) {
    result = {};
    if (!arc.data || arc.size < 0x40 || memcmp(arc.data, "RARC", 4)) return false;
    const size_t total = be32(arc.data + 4), info = be32(arc.data + 8);
    if (total > arc.size || total < 0x40 || !range(info, 0x20, total)) return false;
    const size_t data_rel = be32(arc.data + 0x0c), data_size = be32(arc.data + 0x10);
    if (!range(info, data_rel, total)) return false;
    const size_t data = info + data_rel;
    if (!range(data, data_size, total)) return false;
    const size_t count = be32(arc.data + info + 8), files_rel = be32(arc.data + info + 12);
    const size_t strings_size = be32(arc.data + info + 16), strings_rel = be32(arc.data + info + 20);
    if (!count || count > 65535 || index >= count || !range(info, files_rel, total) ||
        !range(info + files_rel, count * 20, total) || !range(info, strings_rel, total) ||
        !range(info + strings_rel, strings_size, total)) return false;
    const uint8_t* entry = arc.data + info + files_rel + index * 20;
    const uint32_t name_flags = be32(entry + 4);
    const uint8_t flags = uint8_t(name_flags >> 24);
    const size_t name_offset = name_flags & 0xffffff;
    if (!(flags & 1) || (flags & 2) || !range(name_offset, 7, strings_size)) return false;
    const char* name = reinterpret_cast<const char*>(arc.data + info + strings_rel + name_offset);
    const char expected[] = "cl.bdl";
    for (size_t i = 0; i < sizeof(expected); ++i) {
        const char c = name[i] >= 'A' && name[i] <= 'Z' ? name[i] + ('a' - 'A') : name[i];
        if (c != expected[i]) return false;
    }
    const size_t offset = be32(entry + 8), size = be32(entry + 12);
    if (!size || !range(offset, size, data_size)) return false;
    result = {arc.data + data + offset, size};
    return true;
}

inline bool bdl(Bytes b) {
    if (!b.data || b.size < 0x28 || memcmp(b.data, "J3D2", 4) ||
        (memcmp(b.data + 4, "bdl3", 4) && memcmp(b.data + 4, "bdl4", 4))) return false;
    const size_t total = be32(b.data + 8), count = be32(b.data + 12);
    if (total > b.size || total < 0x28 || !count || count > 32) return false;
    size_t offset = 0x20;
    uint32_t required = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!range(offset, 8, total)) return false;
        const size_t size = be32(b.data + offset + 4);
        if (size < 8 || !range(offset, size, total)) return false;
        const uint32_t tag = be32(b.data + offset);
        if (tag == 0x4a4e5431) { // JNT1
            if (size < 0x0c || be16(b.data + offset + 8) != 42) return false;
            required |= 1;
        }
        if (tag == 0x4d415433) {
            if (size < 0x0c || !be16(b.data + offset + 8) || be16(b.data + offset + 8) > 64) return false;
            required |= 2;
        }
        if (tag == 0x53485031) {
            if (size < 0x0c || !be16(b.data + offset + 8) || be16(b.data + offset + 8) > 64) return false;
            required |= 4;
        }
        if (tag == 0x54455831) {
            if (size < 0x0c || !be16(b.data + offset + 8) || be16(b.data + offset + 8) > 64) return false;
            required |= 8;
        }
        if (tag == 0x4d444c33) required |= 16;
        offset += size;
    }
    return required == 31 && offset <= total;
}
} // namespace pc_remote_asset
