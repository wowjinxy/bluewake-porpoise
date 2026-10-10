// Private offline ETL export. No session control, token, or privilege APIs.
// Build/run belongs to root. C++17; explicit libraries: tdh.lib advapi32.lib.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <tdh.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr uint64_t kInputCap = 256ull << 20;
constexpr uint64_t kOutputCap = 64ull << 20; // all three files together
constexpr uint64_t kSummaryReserve = 4ull << 20;
constexpr uint64_t kEventCap = 2000000;
constexpr size_t kSchemaCap = 4096, kSchemaBytesCap = 1 << 20;
constexpr uint64_t kSchemaCacheBytesCap = 16ull << 20;
constexpr size_t kPropertyCap = 1024, kValueCap = 65536;
constexpr size_t kExpandedPropertyCap = 8192, kExtendedCap = 64;
constexpr double kSoftSeconds = 108.0; // external owned watchdog: 120 seconds
const wchar_t kPrivateRoot[] = L"D:\\Projects\\BlueWake-pmu-feasibility-20261010\\";

void need(bool b, const char* message) { if (!b) throw std::runtime_error(message); }
bool range_ok(size_t total, size_t offset, size_t count) {
    return offset <= total && count <= total - offset;
}
template<class T> std::string num(T n) { return std::to_string(n); }
std::string boolean(bool b) { return b ? "true" : "false"; }
std::string hex(const void* data, size_t size) {
    need(size == 0 || data != nullptr, "null nonempty byte range");
    need(size <= (kOutputCap / 2), "hex input exceeds output bound");
    static const char digits[] = "0123456789abcdef";
    const auto* p = static_cast<const unsigned char*>(data);
    std::string s(size * 2, '0');
    for (size_t i = 0; i < size; ++i) {
        s[2*i] = digits[p[i] >> 4]; s[2*i+1] = digits[p[i] & 15];
    }
    return s;
}
std::string quote(const std::string& s) {
    static const char digits[] = "0123456789abcdef";
    std::string r = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { r += '\\'; r += static_cast<char>(c); }
        else if (c < 0x20 || c >= 0x80) {
            r += "\\u00"; r += digits[c >> 4]; r += digits[c & 15];
        } else r += static_cast<char>(c);
    }
    return r + '"';
}
// Emit UTF-16 code units as JSON escapes. No lossy locale conversion or APIs.
std::string wquote(const std::wstring& s) {
    static_assert(sizeof(wchar_t) == 2, "Windows UTF-16 ABI required");
    static const char digits[] = "0123456789abcdef";
    std::string r = "\"";
    for (wchar_t wc : s) {
        const unsigned v = static_cast<unsigned short>(wc);
        if (v >= 0x20 && v < 0x7f && v != '"' && v != '\\') r += char(v);
        else {
            r += "\\u";
            for (int shift = 12; shift >= 0; shift -= 4) r += digits[(v >> shift) & 15];
        }
    }
    return r + '"';
}
std::string guid(const GUID& g) {
    char s[39]{};
    std::snprintf(s, sizeof(s), "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        static_cast<unsigned long>(g.Data1), unsigned(g.Data2), unsigned(g.Data3),
        unsigned(g.Data4[0]), unsigned(g.Data4[1]), unsigned(g.Data4[2]), unsigned(g.Data4[3]),
        unsigned(g.Data4[4]), unsigned(g.Data4[5]), unsigned(g.Data4[6]), unsigned(g.Data4[7]));
    return s;
}
std::string descriptor(const EVENT_DESCRIPTOR& d) {
    return "{\"id\":" + num(d.Id) + ",\"version\":" + num(d.Version) +
        ",\"channel\":" + num(d.Channel) + ",\"level\":" + num(d.Level) +
        ",\"opcode\":" + num(d.Opcode) + ",\"task\":" + num(d.Task) +
        ",\"keyword\":" + num(d.Keyword) + "}";
}
bool schema_string(const std::vector<unsigned char>& b, ULONG offset, std::wstring& out) {
    out.clear();
    if (!offset || (offset % sizeof(wchar_t)) || !range_ok(b.size(), offset, 2)) return false;
    for (size_t i = offset; range_ok(b.size(), i, 2); i += 2) {
        wchar_t c{}; std::memcpy(&c, b.data() + i, 2);
        if (!c) return true;
        out.push_back(c);
    }
    out.clear(); return false;
}

struct Schema {
    ULONG status = ERROR_SUCCESS, size_probe_status = ERROR_SUCCESS;
    ULONG requested_bytes = 0;
    std::vector<unsigned char> bytes;
    TRACE_EVENT_INFO header{};
    std::vector<EVENT_PROPERTY_INFO> properties;
    std::vector<std::wstring> names;
    std::vector<std::string> issues;
    bool valid = false;
    void validate() {
        const size_t base = offsetof(TRACE_EVENT_INFO, EventPropertyInfoArray);
        if (!range_ok(bytes.size(), 0, base)) { issues.push_back("short_TRACE_EVENT_INFO"); return; }
        std::memcpy(&header, bytes.data(), base);
        if (header.PropertyCount > kPropertyCap || header.TopLevelPropertyCount > header.PropertyCount ||
            !range_ok(bytes.size(), base, size_t(header.PropertyCount) * sizeof(EVENT_PROPERTY_INFO))) {
            issues.push_back("property_array_bounds_or_count"); return;
        }
        properties.resize(header.PropertyCount); names.resize(header.PropertyCount);
        if (!properties.empty()) std::memcpy(properties.data(), bytes.data() + base,
            properties.size() * sizeof(EVENT_PROPERTY_INFO));
        const ULONG offsets[] = {header.ProviderNameOffset, header.LevelNameOffset,
            header.ChannelNameOffset, header.KeywordsNameOffset, header.TaskNameOffset,
            header.OpcodeNameOffset, header.EventMessageOffset, header.ProviderMessageOffset,
            header.EventNameOffset, header.EventAttributesOffset};
        for (ULONG o : offsets) { std::wstring s; if (o && !schema_string(bytes, o, s)) issues.push_back("metadata_string_bounds"); }
        if (header.BinaryXMLSize && !range_ok(bytes.size(), header.BinaryXMLOffset, header.BinaryXMLSize))
            issues.push_back("binary_xml_bounds");
        for (size_t i = 0; i < properties.size(); ++i) {
            const auto& p = properties[i];
            if (!schema_string(bytes, p.NameOffset, names[i]) || names[i].empty()) issues.push_back("property_name_bounds");
            if (p.Flags & PropertyStruct) {
                if (!range_ok(properties.size(), p.structType.StructStartIndex, p.structType.NumOfStructMembers))
                    issues.push_back("struct_member_bounds");
            } else if (p.Flags & PropertyHasCustomSchema) {
                const size_t o = p.customSchemaType.CustomSchemaOffset;
                uint16_t length = 0;
                if (!range_ok(bytes.size(), o, 4)) issues.push_back("custom_schema_prefix_bounds");
                else { std::memcpy(&length, bytes.data()+o+2, 2);
                    if (!range_ok(bytes.size(), o+4, length)) issues.push_back("custom_schema_body_bounds"); }
            } else if (p.nonStructType.MapNameOffset) {
                std::wstring s; if (!schema_string(bytes, p.nonStructType.MapNameOffset, s)) issues.push_back("map_name_bounds");
            }
            if ((p.Flags & PropertyParamCount) && p.countPropertyIndex >= properties.size()) issues.push_back("count_index_bounds");
            if ((p.Flags & PropertyParamLength) && p.lengthPropertyIndex >= properties.size()) issues.push_back("length_index_bounds");
        }
        valid = issues.empty();
    }
};
std::string issue_list(const std::vector<std::string>& a) {
    std::string s = "["; bool comma = false;
    for (const auto& v : a) { if (comma) s += ','; comma = true; s += quote(v); }
    return s + ']';
}
size_t primitive_width(USHORT type, unsigned pointer_size) {
    switch (type) {
    case TDH_INTYPE_INT8: case TDH_INTYPE_UINT8: case TDH_INTYPE_ANSICHAR: return 1;
    case TDH_INTYPE_INT16: case TDH_INTYPE_UINT16: case TDH_INTYPE_UNICODECHAR: return 2;
    case TDH_INTYPE_INT32: case TDH_INTYPE_UINT32: case TDH_INTYPE_FLOAT:
    case TDH_INTYPE_BOOLEAN: case TDH_INTYPE_HEXINT32: return 4;
    case TDH_INTYPE_INT64: case TDH_INTYPE_UINT64: case TDH_INTYPE_DOUBLE:
    case TDH_INTYPE_FILETIME: case TDH_INTYPE_HEXINT64: return 8;
    case TDH_INTYPE_GUID: case TDH_INTYPE_SYSTEMTIME: return 16;
    case TDH_INTYPE_POINTER: case TDH_INTYPE_SIZET: return pointer_size;
    default: return 0;
    }
}
uint64_t unsigned_le(const std::vector<unsigned char>& bytes) {
    need(bytes.size() == 1 || bytes.size() == 2 || bytes.size() == 4 || bytes.size() == 8,
        "integer raw size must be 1/2/4/8");
    uint64_t n = 0;
    for (size_t i = 0; i < bytes.size(); ++i) n |= uint64_t(bytes[i]) << (8*i);
    return n;
}
std::string primitive(const std::vector<unsigned char>& bytes, USHORT type, unsigned pointer_size) {
    const size_t expected = primitive_width(type, pointer_size);
    const bool valid = expected && bytes.size() == expected;
    std::string s = "{\"in_type\":" + num(type) + ",\"expected_fixed_bytes\":" + num(expected) +
        ",\"fixed_size_known\":" + boolean(expected != 0) + ",\"fixed_size_valid\":" + boolean(valid) +
        ",\"raw_hex\":" + quote(hex(bytes.data(), bytes.size()));
    if (valid && expected <= 8) s += ",\"unsigned_little_endian_bits\":" + quote(num(unsigned_le(bytes)));
    // Floating point bits stay raw: NaN payloads and infinities never become invalid JSON numbers.
    return s + '}';
}
struct Budget {
    uint64_t used = 0, cap = kOutputCap, reserve = kSummaryReserve;
    void charge(size_t n, bool summary = false) {
        const uint64_t limit = summary ? cap : cap - reserve;
        need(used <= limit && uint64_t(n) <= limit - used, "aggregate_output_budget_exceeded");
        used += uint64_t(n);
    }
};
void cache_charge(uint64_t& used, size_t n) {
    need(used <= kSchemaCacheBytesCap && uint64_t(n) <= kSchemaCacheBytesCap - used,
        "aggregate_schema_cache_byte_cap_exceeded");
    used += uint64_t(n);
}
struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    Handle() = default;
    explicit Handle(HANDLE p) : h(p) {}
    Handle(const Handle&) = delete; Handle& operator=(const Handle&) = delete;
    ~Handle() { if (h != INVALID_HANDLE_VALUE && h != nullptr) CloseHandle(h); }
};
struct Output {
    Handle file;
    Budget* budget = nullptr;
    void create(const std::wstring& path, Budget& b) {
        file.h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        need(file.h != INVALID_HANDLE_VALUE, "exclusive_output_creation_failed"); budget = &b;
    }
    void line(std::string s, bool summary = false) {
        s += '\n'; budget->charge(s.size(), summary);
        DWORD done = 0;
        need(s.size() <= std::numeric_limits<DWORD>::max(), "output_line_size_overflow");
        need(WriteFile(file.h, s.data(), static_cast<DWORD>(s.size()), &done, nullptr) && done == s.size(), "output_write_failed");
    }
};
std::wstring absolute_path(const std::wstring& input) {
    need(input.size() >= 3 && input[1] == L':' && (input[2] == L'\\' || input[2] == L'/') &&
        ((input[0] >= L'A' && input[0] <= L'Z') || (input[0] >= L'a' && input[0] <= L'z')),
        "path_must_be_absolute_drive_path_not_UNC_or_device");
    need(input.find(L':', 2) == std::wstring::npos, "alternate_data_stream_rejected");
    std::vector<wchar_t> b(32768);
    DWORD n = GetFullPathNameW(input.c_str(), static_cast<DWORD>(b.size()), b.data(), nullptr);
    need(n && n < b.size(), "full_path_failed"); return std::wstring(b.data(), n);
}
void no_reparse_components(const std::wstring& path) {
    for (size_t i = 3; i <= path.size(); ++i) {
        if (i != path.size() && path[i] != L'\\') continue;
        const DWORD a = GetFileAttributesW(path.substr(0, i).c_str());
        need(a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_REPARSE_POINT), "missing_or_reparse_path_component");
    }
}
std::string file_info(const BY_HANDLE_FILE_INFORMATION& i) {
    return "{\"size\":" + num((uint64_t(i.nFileSizeHigh)<<32) | i.nFileSizeLow) +
        ",\"volume_serial\":" + num(i.dwVolumeSerialNumber) + ",\"file_index\":" +
        quote(num((uint64_t(i.nFileIndexHigh)<<32) | i.nFileIndexLow)) + ",\"last_write_filetime\":" +
        quote(num((uint64_t(i.ftLastWriteTime.dwHighDateTime)<<32) | i.ftLastWriteTime.dwLowDateTime)) + "}";
}
std::string header_json(const TRACE_LOGFILE_HEADER& h) {
    return "{\"BufferSize_raw\":" + num(h.BufferSize) + ",\"Version\":" + num(h.Version) +
        ",\"ProviderVersion\":" + num(h.ProviderVersion) + ",\"NumberOfProcessors\":" + num(h.NumberOfProcessors) +
        ",\"EndTime\":" + quote(num(h.EndTime.QuadPart)) + ",\"TimerResolution\":" + num(h.TimerResolution) +
        ",\"MaximumFileSize\":" + num(h.MaximumFileSize) + ",\"LogFileMode\":" + num(h.LogFileMode) +
        ",\"BuffersWritten\":" + num(h.BuffersWritten) + ",\"StartBuffers\":" + num(h.StartBuffers) +
        ",\"PointerSize_bytes\":" + num(h.PointerSize) + ",\"EventsLost\":" + num(h.EventsLost) +
        ",\"CpuSpeedInMHz\":" + num(h.CpuSpeedInMHz) + ",\"BootTime\":" + quote(num(h.BootTime.QuadPart)) +
        ",\"PerfFreq\":" + quote(num(h.PerfFreq.QuadPart)) + ",\"StartTime\":" + quote(num(h.StartTime.QuadPart)) +
        ",\"ReservedFlags_clock_type\":" + num(h.ReservedFlags) + ",\"BuffersLost\":" + num(h.BuffersLost) +
        ",\"TimeZone_raw_hex\":" + quote(hex(&h.TimeZone, sizeof(h.TimeZone))) + "}";
}

struct RawProperty { ULONG size_status = ERROR_SUCCESS, get_status = ERROR_SUCCESS, size = 0; std::vector<unsigned char> bytes; };
struct Decoder {
    Budget budget;
    Output events, schemas, summary;
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    bool failed = false;
    std::string error;
    uint64_t seen = 0, written = 0, schema_unknown = 0, property_unknown = 0, buffers = 0;
    uint64_t expanded = 0;
    uint64_t schema_cache_key_bytes = 0;
    unsigned pointer_size = 0;
    ULONG process_status = ERROR_SUCCESS, close_status = ERROR_SUCCESS;
    ULONG max_header_events_lost = 0, max_header_buffers_lost = 0;
    TRACE_LOGFILE_HEADER initial_header{}, last_header{};
    std::map<std::string, size_t> schema_ids;
    std::map<std::string, uint64_t> event_inventory, tdh_status_inventory, loss_candidates;
    std::vector<std::string> loss_sequences;
    void check_time() const {
        need(std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count() < kSoftSeconds,
            "soft_time_budget_exceeded");
    }
    void fail(const char* m) noexcept { failed = true; try { if (error.empty()) error = m; } catch (...) {} }
    RawProperty get(EVENT_RECORD* e, std::vector<PROPERTY_DATA_DESCRIPTOR>& path) {
        check_time(); RawProperty r;
        r.size_status = TdhGetPropertySize(e, 0, nullptr, static_cast<ULONG>(path.size()), path.data(), &r.size);
        if (r.size_status != ERROR_SUCCESS) { r.get_status = ERROR_NOT_FOUND; return r; }
        need(r.size <= kValueCap, "property_size_cap_exceeded");
        r.bytes.resize(r.size);
        unsigned char dummy = 0;
        r.get_status = TdhGetProperty(e, 0, nullptr, static_cast<ULONG>(path.size()), path.data(),
            r.size, r.bytes.empty() ? &dummy : r.bytes.data());
        if (r.get_status != ERROR_SUCCESS) r.bytes.clear();
        return r;
    }
    bool count(EVENT_RECORD* e, const Schema& s, size_t index,
        const std::vector<PROPERTY_DATA_DESCRIPTOR>& parent, uint64_t& n) {
        const auto& p = s.properties[index]; n = p.count;
        if (!(p.Flags & PropertyParamCount)) return true;
        const size_t ref = p.countPropertyIndex;
        if (ref >= s.properties.size() || (s.properties[ref].Flags & (PropertyStruct | PropertyParamCount))) return false;
        auto path = parent;
        if (ref < s.header.TopLevelPropertyCount) path.clear();
        else if (path.empty()) return false; // cannot invent a containing struct for a child count
        const USHORT t = s.properties[ref].nonStructType.InType;
        if (t != TDH_INTYPE_UINT8 && t != TDH_INTYPE_UINT16 && t != TDH_INTYPE_UINT32 && t != TDH_INTYPE_UINT64) return false;
        PROPERTY_DATA_DESCRIPTOR d{}; d.PropertyName = reinterpret_cast<ULONG_PTR>(s.names[ref].c_str());
        d.ArrayIndex = 0; path.push_back(d);
        RawProperty r = get(e, path);
        if (r.size_status || r.get_status || r.bytes.size() != primitive_width(t, pointer_size)) return false;
        n = unsigned_le(r.bytes); return true;
    }
    void property(EVENT_RECORD* e, const Schema& s, size_t index,
        std::vector<PROPERTY_DATA_DESCRIPTOR> parent, const std::string& prefix,
        size_t depth, std::string& output, bool& comma) {
        check_time();
        need(depth < 8 && index < s.properties.size(), "property_recursion_bound");
        const auto& p = s.properties[index];
        uint64_t n = 0;
        if (!count(e, s, index, parent, n)) {
            if (comma) output += ','; comma = true; ++property_unknown;
            output += "{\"property_index\":" + num(index) + ",\"name\":" + wquote(s.names[index]) +
                ",\"state\":\"UNKNOWN_dynamic_count\"}"; return;
        }
        need(n <= kExpandedPropertyCap, "property_array_count_cap_exceeded");
        if (!n) {
            if (comma) output += ','; comma = true;
            output += "{\"property_index\":" + num(index) + ",\"name\":" + wquote(s.names[index]) + ",\"array_count\":0}";
        }
        for (uint64_t a = 0; a < n; ++a) {
            need(++expanded <= kExpandedPropertyCap, "expanded_property_cap_exceeded");
            auto path = parent; PROPERTY_DATA_DESCRIPTOR d{};
            d.PropertyName = reinterpret_cast<ULONG_PTR>(s.names[index].c_str()); d.ArrayIndex = static_cast<ULONG>(a); path.push_back(d);
            const std::string id = prefix + num(index) + '[' + num(a) + ']';
            if (p.Flags & PropertyStruct) {
                const size_t first = p.structType.StructStartIndex, count_members = p.structType.NumOfStructMembers;
                need(range_ok(s.properties.size(), first, count_members), "struct_range_changed");
                for (size_t j = first; j < first + count_members; ++j) property(e, s, j, path, id + '/', depth + 1, output, comma);
                continue;
            }
            RawProperty raw = get(e, path);
            if (comma) output += ','; comma = true;
            const USHORT type = p.nonStructType.InType;
            const size_t expected = primitive_width(type, pointer_size);
            const bool size_bad = expected && raw.bytes.size() != expected;
            if (raw.size_status || raw.get_status || size_bad) ++property_unknown;
            output += "{\"path_indices\":" + quote(id) + ",\"property_index\":" + num(index) +
                ",\"name\":" + wquote(s.names[index]) + ",\"flags\":" + num(static_cast<ULONG>(p.Flags)) +
                ",\"array_index\":" + num(a) + ",\"array_count\":" + num(n) +
                ",\"size_status\":" + num(raw.size_status) + ",\"get_status\":" + num(raw.get_status) +
                ",\"tdh_reported_bytes\":" + num(raw.size) + ",\"state\":" +
                quote((raw.size_status || raw.get_status) ? "UNKNOWN_TDH_property_error" :
                    size_bad ? "UNKNOWN_primitive_size_mismatch" : expected ? "TDH_raw_fixed_value" : "TDH_raw_variable_or_uninterpreted_type") +
                ",\"value\":" + primitive(raw.bytes, type, pointer_size) + "}";
            need(output.size() <= kOutputCap - kSummaryReserve, "event_property_output_cap");
        }
    }
    Schema schema(EVENT_RECORD* e) {
        check_time(); Schema s; ULONG bytes = 0;
        s.size_probe_status = TdhGetEventInformation(e, 0, nullptr, nullptr, &bytes);
        s.requested_bytes = bytes;
        if (s.size_probe_status != ERROR_INSUFFICIENT_BUFFER || !bytes) { s.status = s.size_probe_status; return s; }
        need(bytes <= kSchemaBytesCap, "schema_byte_cap_exceeded");
        s.bytes.resize(bytes); ULONG actual = bytes;
        s.status = TdhGetEventInformation(e, 0, nullptr, reinterpret_cast<TRACE_EVENT_INFO*>(s.bytes.data()), &actual);
        if (s.status != ERROR_SUCCESS) { s.bytes.clear(); return s; }
        need(actual <= bytes, "TDH_schema_size_exceeds_allocated_buffer"); s.bytes.resize(actual); s.validate(); return s;
    }
    std::string schema_record(const Schema& s, size_t id, const std::string& identity) {
        std::string r = "{\"schema_id\":" + num(id) + ",\"event_identity\":" + quote(identity) +
            ",\"size_probe_status\":" + num(s.size_probe_status) + ",\"status\":" + num(s.status) +
            ",\"requested_bytes\":" + num(s.requested_bytes) + ",\"raw_schema_hex\":" + quote(hex(s.bytes.data(), s.bytes.size())) +
            ",\"validated\":" + boolean(s.valid) + ",\"issues\":" + issue_list(s.issues);
        if (s.valid) {
            r += ",\"provider_guid\":" + quote(guid(s.header.ProviderGuid)) + ",\"event_guid\":" + quote(guid(s.header.EventGuid)) +
                ",\"descriptor\":" + descriptor(s.header.EventDescriptor) + ",\"decoding_source\":" + num(s.header.DecodingSource) +
                ",\"flags\":" + num(s.header.Flags) + ",\"property_count\":" + num(s.header.PropertyCount) +
                ",\"top_level_property_count\":" + num(s.header.TopLevelPropertyCount) + ",\"metadata_strings\":[";
            const ULONG offsets[] = {s.header.ProviderNameOffset,s.header.LevelNameOffset,s.header.ChannelNameOffset,
                s.header.KeywordsNameOffset,s.header.TaskNameOffset,s.header.OpcodeNameOffset,s.header.EventMessageOffset,
                s.header.ProviderMessageOffset,s.header.EventNameOffset,s.header.EventAttributesOffset};
            const char* labels[] = {"provider","level","channel","keywords","task","opcode","event_message","provider_message","event_name_or_activity","attributes_or_related_activity"};
            for (size_t j=0; j<10; ++j) { std::wstring w; if (j) r += ','; schema_string(s.bytes,offsets[j],w);
                r += "{\"field\":"+quote(labels[j])+",\"offset\":"+num(offsets[j])+",\"text\":"+(offsets[j]?wquote(w):"null")+"}"; }
            r += "],\"properties\":[";
            for (size_t i=0; i<s.properties.size(); ++i) {
                const auto& p=s.properties[i]; if(i) r+=',';
                r += "{\"index\":"+num(i)+",\"name_offset\":"+num(p.NameOffset)+",\"name\":"+wquote(s.names[i])+
                    ",\"flags\":"+num(static_cast<ULONG>(p.Flags))+",\"count_or_index\":"+num(p.count)+
                    ",\"length_or_index\":"+num(p.length);
                if(p.Flags&PropertyStruct)r+=",\"struct_start_index\":"+num(p.structType.StructStartIndex)+
                    ",\"struct_member_count\":"+num(p.structType.NumOfStructMembers);
                else r+=",\"in_type\":"+num(p.nonStructType.InType)+",\"out_type\":"+num(p.nonStructType.OutType);
                r+=",\"raw_info_hex\":"+quote(hex(&p,sizeof(p)))+"}";
            }
            r += ']';
        }
        return r+'}';
    }
    void event(EVENT_RECORD* e) {
        check_time(); need(++seen <= kEventCap, "event_count_cap_exceeded"); expanded = 0;
        const auto& h=e->EventHeader;
        const std::string identity=guid(h.ProviderId)+"/"+descriptor(h.EventDescriptor)+"/flags="+num(h.Flags);
        need(event_inventory.size()<kSchemaCap || event_inventory.count(identity), "event_identity_cap_exceeded"); ++event_inventory[identity];
        Schema s=schema(e); ++tdh_status_inventory[num(s.status)];
        std::string key=identity+"/tdh="+num(s.status)+"/"+hex(s.bytes.data(),s.bytes.size());
        // Full returned schema distinguishes TraceLogging events sharing a descriptor.
        auto it=schema_ids.find(key); size_t id=0;
        if(it==schema_ids.end()) { need(schema_ids.size()<kSchemaCap,"schema_count_cap_exceeded"); id=schema_ids.size()+1;
            cache_charge(schema_cache_key_bytes,key.size());
            schemas.line(schema_record(s,id,identity)); schema_ids.emplace(std::move(key),id); }
        else id=it->second;
        if(!s.valid) ++schema_unknown;
        unsigned event_pointer=0;
        const bool bit32=(h.Flags&EVENT_HEADER_FLAG_32_BIT_HEADER)!=0, bit64=(h.Flags&EVENT_HEADER_FLAG_64_BIT_HEADER)!=0;
        if(bit32!=bit64) event_pointer=bit32?4:8;
        else if(!bit32 && (pointer_size==4 || pointer_size==8)) event_pointer=pointer_size;
        const unsigned file_pointer=pointer_size; pointer_size=event_pointer;
        std::string values="["; bool comma=false;
        if(s.valid) for(size_t i=0;i<s.header.TopLevelPropertyCount;++i) property(e,s,i,{},"",0,values,comma);
        values+=']'; pointer_size=file_pointer;
        need(e->ExtendedDataCount<=kExtendedCap,"extended_item_count_cap_exceeded");
        need(!e->ExtendedDataCount || e->ExtendedData,"null_extended_array");
        std::string ext="[";
        for(size_t i=0;i<e->ExtendedDataCount;++i) { const auto& x=e->ExtendedData[i]; if(i)ext+=',';
            need(!x.DataSize || x.DataPtr,"null_extended_data_pointer");
            ext+="{\"type\":"+num(x.ExtType)+",\"linkage\":"+num(x.Linkage)+",\"reserved1\":"+num(x.Reserved1)+
                ",\"reserved2\":"+num(x.Reserved2)+",\"bytes\":"+num(x.DataSize)+",\"raw_hex\":"+
                quote(hex(reinterpret_cast<const void*>(static_cast<ULONG_PTR>(x.DataPtr)),x.DataSize))+"}"; }
        ext+=']';
        // Loss-event names are evidence candidates, not a hard-coded PMU payload interpretation.
        bool loss=false;
        if(s.valid) { std::wstring task,op,name; schema_string(s.bytes,s.header.TaskNameOffset,task);
            schema_string(s.bytes,s.header.OpcodeNameOffset,op); schema_string(s.bytes,s.header.EventNameOffset,name);
            std::wstring label=task+L" "+op+L" "+name;
            for(auto& c:label) if(c>=L'A'&&c<=L'Z')c+=L'a'-L'A';
            loss=label.find(L"lost")!=std::wstring::npos || label.find(L"loss")!=std::wstring::npos; }
        if(loss) { ++loss_candidates[identity]; need(loss_sequences.size()<kSchemaCap,"loss_inventory_cap_exceeded"); loss_sequences.push_back(num(seen)); }
        std::string r="{\"sequence\":"+num(seen)+",\"schema_id\":"+num(id)+",\"tdh_status\":"+num(s.status)+
            ",\"schema_state\":"+quote(s.valid?"TDH_validated_schema":"UNKNOWN_schema")+
            ",\"provider_guid\":"+quote(guid(h.ProviderId))+",\"descriptor\":"+descriptor(h.EventDescriptor)+
            ",\"header_size\":"+num(h.Size)+",\"header_type\":"+num(h.HeaderType)+",\"header_flags\":"+num(h.Flags)+
            ",\"event_property\":"+num(h.EventProperty)+",\"header_pid\":"+num(h.ProcessId)+",\"header_tid\":"+num(h.ThreadId)+
            ",\"raw_timestamp\":"+quote(num(h.TimeStamp.QuadPart))+",\"activity_id\":"+quote(guid(h.ActivityId))+
            ",\"processor_time_union_raw\":"+quote(num(h.ProcessorTime))+
            ",\"buffer_context_raw_hex\":"+quote(hex(&e->BufferContext,sizeof(e->BufferContext)))+
            ",\"processor_index_or_number\":"+num(GetEventProcessorIndex(e))+",\"logger_id\":"+num(e->BufferContext.LoggerId)+
            ",\"pointer_size_for_types\":"+num(event_pointer)+",\"user_data_bytes\":"+num(e->UserDataLength)+
            ",\"user_data_hex\":"+quote(hex(e->UserData,e->UserDataLength))+
            ",\"extended_data\":"+ext+",\"properties\":"+values+",\"loss_name_candidate\":"+boolean(loss)+"}";
        events.line(std::move(r)); ++written;
    }
};
void WINAPI event_callback(EVENT_RECORD* e) noexcept {
    auto* d=static_cast<Decoder*>(e->UserContext); if(!d || d->failed)return;
    try { d->event(e); } catch(const std::exception& x) { d->fail(x.what()); } catch(...) { d->fail("unknown_callback_exception"); }
}
ULONG WINAPI buffer_callback(EVENT_TRACE_LOGFILEW* logfile) noexcept {
    auto* d=static_cast<Decoder*>(logfile->Context); if(!d)return FALSE;
    try { ++d->buffers; d->last_header=logfile->LogfileHeader;
        d->max_header_events_lost=std::max(d->max_header_events_lost,logfile->LogfileHeader.EventsLost);
        d->max_header_buffers_lost=std::max(d->max_header_buffers_lost,logfile->LogfileHeader.BuffersLost);
        d->check_time();
    } catch(const std::exception& x) { d->fail(x.what()); } catch(...) { d->fail("unknown_buffer_exception"); }
    return d->failed?FALSE:TRUE;
}
std::string inventory(const std::map<std::string,uint64_t>& m) {
    std::string s="[";bool comma=false;for(const auto& p:m){if(comma)s+=',';comma=true;s+="{\"key\":"+quote(p.first)+",\"count\":"+num(p.second)+"}";}return s+']';
}

int self_test() {
    // Pure memory/CRT only: no path APIs, file IO, ETL, OpenTrace, TDH, or sessions.
    unsigned checks=0;
    auto test=[&](bool b){need(b,"self_test_assertion_failed");++checks;};
    test(range_ok(16,16,0));test(!range_ok(16,17,0));test(!range_ok(16,15,2));
    test(!range_ok(16,std::numeric_limits<size_t>::max(),8));
    test(quote(std::string("\"\\\n\0",4))=="\"\\\"\\\\\\u000a\\u0000\"");
    test(wquote(std::wstring(1,L'\x03a9'))=="\"\\u03a9\"");
    std::vector<unsigned char> str={0,0,'A',0,0,0};std::wstring w;
    test(schema_string(str,2,w)&&w==L"A");test(!schema_string(str,1,w));test(!schema_string(str,5,w));
    str.pop_back();test(!schema_string(str,2,w));
    test(primitive_width(TDH_INTYPE_UINT64,8)==8);test(primitive_width(TDH_INTYPE_POINTER,4)==4);
    test(primitive_width(TDH_INTYPE_POINTER,0)==0);test(primitive_width(TDH_INTYPE_BINARY,8)==0);
    test(unsigned_le({0x78,0x56,0x34,0x12})==0x12345678);
    test(primitive({1,2},TDH_INTYPE_UINT32,8).find("\"fixed_size_valid\":false")!=std::string::npos);
    test(primitive({0,0,0xc0,0x7f},TDH_INTYPE_FLOAT,8).find("\"raw_hex\":\"0000c07f\"")!=std::string::npos);
    bool caught=false;try{unsigned_le({1,2,3});}catch(...){caught=true;}test(caught);
    Budget b;b.cap=16;b.reserve=4;b.charge(12);caught=false;try{b.charge(1);}catch(...){caught=true;}test(caught&&b.used==12);
    b.charge(4,true);caught=false;try{b.charge(1,true);}catch(...){caught=true;}test(caught&&b.used==16);
    Schema bad;bad.bytes.resize(4);bad.validate();test(!bad.valid&&!bad.issues.empty());
    const size_t base=offsetof(TRACE_EVENT_INFO,EventPropertyInfoArray);
    Schema good;good.bytes.resize(base+sizeof(EVENT_PROPERTY_INFO)+4,0);
    TRACE_EVENT_INFO h{};h.PropertyCount=1;h.TopLevelPropertyCount=1;std::memcpy(good.bytes.data(),&h,base);
    EVENT_PROPERTY_INFO p{};p.NameOffset=static_cast<ULONG>(base+sizeof(p));p.count=1;p.nonStructType.InType=TDH_INTYPE_UINT32;
    std::memcpy(good.bytes.data()+base,&p,sizeof(p));good.bytes[p.NameOffset]='x';good.validate();test(good.valid&&good.names[0]==L"x");
    Schema off;off.bytes=good.bytes;p.NameOffset=static_cast<ULONG>(off.bytes.size()+2);std::memcpy(off.bytes.data()+base,&p,sizeof(p));off.validate();test(!off.valid);
    Schema ref;ref.bytes=good.bytes;p.NameOffset=static_cast<ULONG>(base+sizeof(p));p.Flags=PropertyParamCount;p.countPropertyIndex=1;
    std::memcpy(ref.bytes.data()+base,&p,sizeof(p));ref.validate();test(!ref.valid);
    Schema over;over.bytes=good.bytes;h.PropertyCount=0xffffffffu;std::memcpy(over.bytes.data(),&h,base);over.validate();test(!over.valid);
    Schema members;members.bytes=good.bytes;p.Flags=PropertyStruct;p.structType.StructStartIndex=1;p.structType.NumOfStructMembers=1;
    std::memcpy(members.bytes.data()+base,&p,sizeof(p));members.validate();test(!members.valid);
    Schema custom;custom.bytes=good.bytes;p.Flags=PropertyHasCustomSchema;p.customSchemaType.CustomSchemaOffset=static_cast<ULONG>(custom.bytes.size()-2);
    std::memcpy(custom.bytes.data()+base,&p,sizeof(p));custom.validate();test(!custom.valid);
    uint64_t used=kSchemaCacheBytesCap-1;cache_charge(used,1);caught=false;try{cache_charge(used,1);}catch(...){caught=true;}test(caught&&used==kSchemaCacheBytesCap);
    std::printf("ETL_DECODER_SELF_TEST_PASS checks=%u no_etl=1 no_trace_api=1\n",checks);return 0;
}
int schema_fixture() {
    // Deliberate synthetic reference bytes ONLY. Production event() never uses these offsets.
    // Microsoft/perfview TraceEvent/Parsers/KernelTraceEventParser.cs PMCCounterProfTraceData
    // and SampledProfileIntervalTraceData; independently pinned in source-schema-assessment-b1.json.
    // Installed TDH may not have PMCSample47 schema. UNKNOWN is an observation, not fixture failure.
    const GUID perfinfo={0xce1dbfb4,0x137e,0x4da6,{0x87,0xb0,0x3f,0x59,0xaa,0x10,0x2c,0xbc}};
    auto put=[](std::vector<unsigned char>& b,size_t offset,uint64_t n,size_t width){
        need(range_ok(b.size(),offset,width)&&width<=8,"fixture_store_bounds");
        for(size_t i=0;i<width;++i)b[offset+i]=static_cast<unsigned char>(n>>(8*i));
    };
    for(unsigned which=0;which<2;++which) {
        std::vector<unsigned char> payload(which?12:16,0);
        if(!which){put(payload,0,0x1122334455667788ull,8);put(payload,8,0x23456789u,4);put(payload,12,9,2);}
        else {put(payload,0,9,4);put(payload,4,65536,4);put(payload,8,4096,4);}
        EVENT_RECORD e{};e.EventHeader.Size=sizeof(EVENT_HEADER);e.EventHeader.ProviderId=perfinfo;
        e.EventHeader.Flags=EVENT_HEADER_FLAG_CLASSIC_HEADER|EVENT_HEADER_FLAG_64_BIT_HEADER;
        e.EventHeader.EventDescriptor.Version=2;e.EventHeader.EventDescriptor.Opcode=which?73:47;
        e.UserData=payload.data();e.UserDataLength=static_cast<USHORT>(payload.size());
        Decoder d;d.pointer_size=8;Schema s=d.schema(&e);
        std::string values="[";bool comma=false;
        if(s.valid)for(size_t i=0;i<s.header.TopLevelPropertyCount;++i)d.property(&e,s,i,{},"",0,values,comma);
        values+=']';
        const std::string line="{\"mode\":\"synthetic_schema_fixture\",\"case\":"+
            quote(which?"PerfInfo_interval_v2_64":"PerfInfo_PMC_v2_64")+
            ",\"real_decode_qualified\":false,\"ETL_or_hardware_proof\":false,\"OpenTrace_called\":false,\"state\":"+
            quote(s.valid?"OBSERVED_TDH_schema":"UNKNOWN_TDH_schema")+
            ",\"provider_guid\":"+quote(guid(perfinfo))+",\"descriptor\":"+descriptor(e.EventHeader.EventDescriptor)+
            ",\"header_flags\":"+num(e.EventHeader.Flags)+",\"deliberate_reference_payload_hex\":"+quote(hex(payload.data(),payload.size()))+
            ",\"schema\":"+d.schema_record(s,which+1,"synthetic_reference_only")+",\"properties\":"+values+"}";
        need(line.size()<=kSchemaBytesCap*3,"schema_fixture_output_cap");std::printf("%s\n",line.c_str());
    }
    std::printf("ETL_DECODER_SCHEMA_FIXTURE_OBSERVED cases=2 real_decode_qualified=0 no_etl=1\n");return 0;
}
int decode(const std::wstring& etl_arg,const std::wstring& out_arg) {
    const std::wstring etl=absolute_path(etl_arg),out=absolute_path(out_arg);
    const size_t root_len=(sizeof(kPrivateRoot)/sizeof(wchar_t))-1;
    need(out.size()>=root_len && _wcsnicmp(out.c_str(),kPrivateRoot,root_len)==0,"output_must_be_existing_private_D_capsule_subdirectory");
    no_reparse_components(etl);no_reparse_components(out);
    need((GetFileAttributesW(out.c_str())&FILE_ATTRIBUTE_DIRECTORY)!=0,"output_not_directory");
    Handle input(CreateFileW(etl.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    need(input.h!=INVALID_HANDLE_VALUE,"input_read_open_failed");
    BY_HANDLE_FILE_INFORMATION before{},after{};need(GetFileInformationByHandle(input.h,&before)!=FALSE,"input_metadata_failed");
    const uint64_t size=(uint64_t(before.nFileSizeHigh)<<32)|before.nFileSizeLow;
    need(size>0&&size<=kInputCap&&!(before.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY),"input_size_or_type_rejected");
    Decoder d;
    d.events.create(out+L"\\events.jsonl",d.budget);d.schemas.create(out+L"\\schemas.jsonl",d.budget);d.summary.create(out+L"\\summary.json",d.budget);
    EVENT_TRACE_LOGFILEW logfile{};logfile.LogFileName=const_cast<wchar_t*>(etl.c_str());logfile.LoggerName=nullptr;
    logfile.ProcessTraceMode=PROCESS_TRACE_MODE_EVENT_RECORD|PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    logfile.EventRecordCallback=event_callback;logfile.BufferCallback=buffer_callback;logfile.Context=&d;
    TRACEHANDLE trace=OpenTraceW(&logfile);
    const bool opened=trace!=INVALID_PROCESSTRACE_HANDLE;
    const ULONG open_error=opened?ERROR_SUCCESS:GetLastError();
    if(!opened)d.fail(("OpenTraceW_error_"+num(open_error)).c_str());
    else {
        d.initial_header=logfile.LogfileHeader;d.last_header=d.initial_header;d.pointer_size=d.initial_header.PointerSize;
        d.max_header_events_lost=d.initial_header.EventsLost;d.max_header_buffers_lost=d.initial_header.BuffersLost;
        d.process_status=ProcessTrace(&trace,1,nullptr,nullptr);
        d.close_status=CloseTrace(trace);
        if(d.process_status!=ERROR_SUCCESS)d.fail(("ProcessTrace_error_"+num(d.process_status)).c_str());
        if(d.close_status!=ERROR_SUCCESS)d.fail(("CloseTrace_error_"+num(d.close_status)).c_str());
    }
    bool preserved=GetFileInformationByHandle(input.h,&after)!=FALSE && file_info(before)==file_info(after);
    if(!preserved)d.fail("input_identity_or_metadata_changed");
    const bool complete=!d.failed&&d.written==d.seen;
    std::string lossseq="[";for(size_t i=0;i<d.loss_sequences.size();++i){if(i)lossseq+=',';lossseq+=d.loss_sequences[i];}lossseq+=']';
    std::string s="{\"status\":"+quote(complete?"PASS_BOUNDED_OFFLINE_ETL_EXPORT_NOT_PMU_VALIDATED":"FAIL_BOUNDED_OFFLINE_ETL_EXPORT_PRESERVED")+
        ",\"error\":"+(d.error.empty()?"null":quote(d.error))+",\"complete_export\":"+boolean(complete)+
        ",\"pmu_validity_established\":false,\"PID_filter_applied\":false,\"all_events_exported_without_PID_filter\":"+boolean(complete)+",\"etl\":"+wquote(etl)+
        ",\"input_before\":"+file_info(before)+",\"input_after\":"+file_info(after)+",\"input_metadata_preserved\":"+boolean(preserved)+
        ",\"events_seen\":"+num(d.seen)+",\"events_written\":"+num(d.written)+",\"schema_count\":"+num(d.schema_ids.size())+
        ",\"events_with_unknown_schema\":"+num(d.schema_unknown)+",\"unknown_property_values\":"+num(d.property_unknown)+
        ",\"buffer_callbacks\":"+num(d.buffers)+",\"OpenTrace_attempted\":true,\"OpenTrace_status\":"+num(open_error)+
        ",\"ProcessTrace_attempted\":"+boolean(opened)+",\"CloseTrace_attempted\":"+boolean(opened)+
        ",\"ProcessTrace_status\":"+(opened?num(d.process_status):"null")+",\"CloseTrace_status\":"+(opened?num(d.close_status):"null")+
        ",\"raw_timestamp_mode\":true,\"initial_logfile_header\":"+(opened?header_json(d.initial_header):"null")+
        ",\"last_buffer_logfile_header\":"+(d.buffers?header_json(d.last_header):"null")+
        ",\"max_header_EventsLost\":"+(opened?num(d.max_header_events_lost):"null")+
        ",\"max_header_BuffersLost\":"+(opened?num(d.max_header_buffers_lost):"null")+
        ",\"EVENT_TRACE_LOGFILE_EventsLost_used\":false,\"event_inventory\":"+inventory(d.event_inventory)+
        ",\"tdh_status_inventory\":"+inventory(d.tdh_status_inventory)+",\"loss_named_event_inventory\":"+inventory(d.loss_candidates)+
        ",\"loss_named_event_sequences\":"+lossseq+
        ",\"loss_inventory_note\":\"Name-based candidates only; UNKNOWN schema loss events may remain in the full raw event inventory. Zero counters do not establish PMU delivery or loss-free capture.\""+
        ",\"elapsed_seconds\":"+num(std::chrono::duration<double>(std::chrono::steady_clock::now()-d.begin).count())+
        ",\"schema_cache_key_bytes\":"+num(d.schema_cache_key_bytes)+
        ",\"limits\":{\"input_bytes\":268435456,\"output_total_bytes\":67108864,\"summary_reserved_bytes\":4194304,\"schema_cache_key_bytes\":16777216,\"events\":2000000,\"schemas\":4096,\"soft_seconds\":108}"+
        ",\"output_bytes_before_summary\":"+num(d.budget.used)+"}";
    d.summary.line(std::move(s),true);
    std::printf("ETL_DECODER_%s events=%llu schemas=%zu pmu_validated=0\n",complete?"EXPORT_COMPLETE":"EXPORT_FAILED",
        static_cast<unsigned long long>(d.written),d.schema_ids.size());return complete?0:1;
}
} // namespace
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc==2&&std::wcscmp(argv[1],L"--self-test")==0)return self_test();
        if(argc==2&&std::wcscmp(argv[1],L"--schema-fixture")==0)return schema_fixture();
        need(argc==5&&std::wcscmp(argv[1],L"--etl")==0&&std::wcscmp(argv[3],L"--out")==0,
            "usage: etl_decode1.exe --self-test | --schema-fixture | --etl ABSOLUTE.etl --out EXISTING_PRIVATE_D_DIRECTORY");
        return decode(argv[2],argv[4]);
    } catch(const std::exception& x){std::fprintf(stderr,"ETL_DECODER_FAILED: %s\n",x.what());return 1;}
    catch(...){std::fprintf(stderr,"ETL_DECODER_FAILED: unknown_exception\n");return 1;}
}
