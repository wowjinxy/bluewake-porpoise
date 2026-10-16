#include "pc/pc_remote_body_asset.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } ++checks; } while (0)
static unsigned checks;
using pc_remote_asset::Bytes;
using Buffer = std::vector<uint8_t>;
static void word(Buffer& b, size_t o, uint32_t v) { b[o] = uint8_t(v >> 24); b[o+1] = uint8_t(v >> 16); b[o+2] = uint8_t(v >> 8); b[o+3] = uint8_t(v); }
static Buffer archive() {
    Buffer b(0x100);
    std::memcpy(b.data(), "RARC", 4); word(b, 4, uint32_t(b.size())); word(b, 8, 0x20);
    word(b, 12, 0x60); word(b, 16, 0x80);
    word(b, 0x28, 1); word(b, 0x2c, 0x20); word(b, 0x30, 7); word(b, 0x34, 0x40);
    word(b, 0x44, 0x11000000); word(b, 0x48, 0); word(b, 0x4c, 16);
    std::memcpy(b.data() + 0x60, "cl.bdl", 7);
    std::memcpy(b.data() + 0x80, "J3D2bdl4", 8);
    return b;
}
static Buffer model() {
    Buffer b(0x20 + 5 * 12);
    std::memcpy(b.data(), "J3D2bdl4", 8); word(b, 8, uint32_t(b.size())); word(b, 12, 5);
    const uint32_t tags[] = {0x4a4e5431, 0x4d415433, 0x53485031, 0x54455831, 0x4d444c33};
    for (unsigned i = 0; i < 5; ++i) { size_t o = 0x20 + i * 12; word(b, o, tags[i]); word(b, o + 4, 12); b[o+9] = i ? 1 : 42; }
    return b;
}
int main() {
    CHECK(!pc_remote_asset::range(std::numeric_limits<size_t>::max(), 2, 8));
    CHECK(!pc_remote_asset::range(7, std::numeric_limits<size_t>::max(), 8));
    CHECK(pc_remote_asset::range(8, 0, 8));
    uint8_t output[32]{}; size_t written = 99;
    Buffer yaz(16); std::memcpy(yaz.data(), "Yaz0", 4); word(yaz, 4, 6);
    yaz.insert(yaz.end(), {0xe0, 'a', 'b', 'c', 0x10, 0x02});
    CHECK(pc_remote_asset::yaz0({yaz.data(), yaz.size()}, output, sizeof(output), written));
    CHECK(written == 6 && !std::memcmp(output, "abcabc", 6));
    for (size_t n = 0; n < yaz.size(); ++n) CHECK(!pc_remote_asset::yaz0({yaz.data(), n}, output, sizeof(output), written));
    CHECK(!pc_remote_asset::yaz0({yaz.data(), yaz.size()}, output, 5, written));
    auto bad = yaz; bad[16] = 0; CHECK(!pc_remote_asset::yaz0({bad.data(), bad.size()}, output, sizeof(output), written));
    bad = yaz; bad.back() = 31; CHECK(!pc_remote_asset::yaz0({bad.data(), bad.size()}, output, sizeof(output), written));
    auto arc = archive(); Bytes result{};
    CHECK(pc_remote_asset::cl_resource({arc.data(), arc.size()}, 0, result));
    CHECK(result.data == arc.data() + 0x80 && result.size == 16);
    CHECK(!pc_remote_asset::cl_resource({arc.data(), arc.size()}, 1, result));
    for (size_t n = 0; n < arc.size(); ++n) CHECK(!pc_remote_asset::cl_resource({arc.data(), n}, 0, result));
    auto damaged = arc; word(damaged, 0x2c, 0xfffffff0); CHECK(!pc_remote_asset::cl_resource({damaged.data(), damaged.size()}, 0, result));
    damaged = arc; word(damaged, 0x48, 0x7f); CHECK(!pc_remote_asset::cl_resource({damaged.data(), damaged.size()}, 0, result));
    damaged = arc; damaged[0x60] = '.'; CHECK(!pc_remote_asset::cl_resource({damaged.data(), damaged.size()}, 0, result));
    damaged = arc; word(damaged, 0x44, 0x02000000); CHECK(!pc_remote_asset::cl_resource({damaged.data(), damaged.size()}, 0, result));
    auto bdl = model(); CHECK(pc_remote_asset::bdl({bdl.data(), bdl.size()}));
    for (size_t n = 0; n < bdl.size(); ++n) CHECK(!pc_remote_asset::bdl({bdl.data(), n}));
    damaged = bdl; damaged[0x29] = 41; CHECK(!pc_remote_asset::bdl({damaged.data(), damaged.size()}));
    damaged = bdl; word(damaged, 0x24, 0xfffffff0); CHECK(!pc_remote_asset::bdl({damaged.data(), damaged.size()}));
    damaged = bdl; word(damaged, 12, 33); CHECK(!pc_remote_asset::bdl({damaged.data(), damaged.size()}));
    damaged = bdl; damaged[0x35] = 65; CHECK(!pc_remote_asset::bdl({damaged.data(), damaged.size()}));
    std::printf("PASS remote-body asset bounds %u checks (no native/render proof)\n", checks);
}
