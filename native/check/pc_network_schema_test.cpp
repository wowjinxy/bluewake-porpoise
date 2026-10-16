// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted value/framing cases from BlueWake tests/network_session_test.cpp.
#include "pc/network/network_session.h"
#include "network_wire.h"
#include "network_digest.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
int main() {
    CHECK(pc_net::digest("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(pc_net::digest("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    PcNetworkFacts facts{};
    CHECK(!pc_network_fact_valid({0,1}) && !pc_network_fact_valid({15,1}));
    CHECK(!pc_network_fact_valid({1,0}) && !pc_network_fact_valid({1,2}));
    CHECK(pc_network_fact_valid({1,1}) && pc_network_fact_merge(&facts,{1,1}));
    CHECK(facts.values[0] == 0 && facts.values[1] == 1 && !pc_network_fact_merge(&facts,{1,1}));
    CHECK(!pc_network_fact_merge(&facts,{0,1}) && !pc_network_fact_merge(nullptr,{1,1}));
    pc_network_fact_clear(&facts); CHECK(facts.values[1] == 0);
    CHECK(PC_NETWORK_PROTOCOL==3&&PC_NETWORK_SCHEMA==2&&PC_NETWORK_FACTS==128);
    for(uint16_t key=0;key<128;++key){
        CHECK(!pc_network_fact_valid({key,0}));
        if(pc_network_fact_valid({key,1})){CHECK(pc_network_fact_merge(&facts,{key,1}));CHECK(!pc_network_fact_merge(&facts,{key,1}));}
    }
    for(const uint16_t key:{uint16_t(9),uint16_t(13)}){
        CHECK(pc_network_fact_merge(&facts,{key,2}));CHECK(!pc_network_fact_merge(&facts,{key,1}));
    }
    CHECK(!pc_network_fact_valid({9,3})&&pc_network_fact_valid({13,3})&&!pc_network_fact_valid({13,4}));
    CHECK(pc_network_fact_valid({49,16})&&pc_network_fact_valid({49,32})&&!pc_network_fact_valid({49,17}));
    const uint64_t source=UINT64_C(3)<<56|1;
    for(uint16_t key:{uint16_t(48),uint16_t(50),uint16_t(51)}){
        CHECK(pc_permanent_server_authoritative(key));
        CHECK(pc_network_reward_valid({source,key,PC_PERMANENT_STEP,1}));
        CHECK(!pc_network_reward_valid({source,key,PC_PERMANENT_MAX,key==48?1u:60u}));
        CHECK(!pc_network_reward_valid({source,key,PC_PERMANENT_STEP,2}));
        uint32_t value=pc_permanent_operation_result(key,0,PC_PERMANENT_STEP,1);
        CHECK(value==(key==48?1u:60u));value=pc_permanent_operation_result(key,value,PC_PERMANENT_STEP,1);
        CHECK(value==(key==48?2u:99u));CHECK(pc_permanent_operation_result(key,value,PC_PERMANENT_STEP,1)==value);
    }
    CHECK(pc_network_reward_valid({UINT64_C(2)<<56|1,1,PC_PERMANENT_MAX,1}));
    CHECK(!pc_network_reward_valid({0,1,PC_PERMANENT_MAX,1}));
    CHECK(!pc_network_reward_valid({UINT64_C(7)<<56|1,1,PC_PERMANENT_MAX,1}));
    CHECK(!pc_network_reward_valid({UINT64_C(2)<<56,1,PC_PERMANENT_MAX,1}));
    CHECK(!pc_network_reward_valid({source,49,PC_PERMANENT_STEP,1}));
    PcNetworkConfig c{};
    pc_network_native_compatibility(&c.compatibility);
    std::strcpy(c.server,"localhost"); c.port=49383; std::strcpy(c.room,"test_1");
    std::memset(c.player_id,'a',32); std::strcpy(c.player_name,"Link");
    CHECK(pc_network_config_valid(&c,nullptr,0));
    char normalized[256]{};
    CHECK(pc_network_normalize_endpoint("localhost",normalized,sizeof normalized) && !std::strcmp(normalized,"127.0.0.1"));
    CHECK(pc_network_normalize_endpoint("0:0:0:0:0:0:0:1",normalized,sizeof normalized) && !std::strcmp(normalized,"::1"));
    CHECK(!pc_network_normalize_endpoint("example.com",normalized,sizeof normalized));
    CHECK(!pc_network_normalize_endpoint("127.0.0.1",normalized,4));
    auto changed=c; changed.compatibility.disc_revision=1; CHECK(!pc_network_config_valid(&changed,nullptr,0));
    changed=c; std::memset(changed.compatibility.build_id,'1',64); changed.compatibility.build_id[64]=0;
    CHECK(!pc_network_config_valid(&changed,nullptr,0)); // legacy translated build identity
    changed=c; changed.compatibility.rules_digest[0]^=1; CHECK(!pc_network_config_valid(&changed,nullptr,0));
    changed=c; changed.compatibility.progression_schema=1; CHECK(!pc_network_config_valid(&changed,nullptr,0));
    changed=c; std::strcpy(changed.room,"../personal"); CHECK(!pc_network_config_valid(&changed,nullptr,0));
    changed=c; std::memset(changed.server,'x',sizeof changed.server); CHECK(!pc_network_config_valid(&changed,nullptr,0));
    using namespace pc_net;
    Bytes body; delta(body,{1,1}); const auto bytes=frame(Delta,9,body); Pipe pipe; Frame parsed{};
    for(size_t i=0;i<bytes.size();++i) { pipe.input.push_back(bytes[i]); CHECK(pipe.next(parsed)==(i+1==bytes.size()?1:0)); }
    CHECK(parsed.type==Delta && parsed.sequence==9 && parsed.body==body && pipe.input.empty());
    Bytes awardBytes;const PcNetworkReward claim{source,50,PC_PERMANENT_STEP,1};reward(awardBytes,claim);
    Reader awardReader{awardBytes};CHECK(same_reward(reward(awardReader),claim)&&awardReader.done()&&awardBytes.size()==16);
    pipe.input=bytes; pipe.input[4]=0xFF; CHECK(pipe.next(parsed)==-1);
    pipe.input=bytes; pipe.input[5]=2; CHECK(pipe.next(parsed)==-1); // prior native protocol
    pipe.input=bytes; pipe.input[8]=1; CHECK(pipe.next(parsed)==-1);
    pipe.input=bytes; std::memcpy(pipe.input.data(),"BWPN",4); CHECK(pipe.next(parsed)==-1);
    pipe.input=bytes; pipe.input[7]=255; CHECK(pipe.next(parsed)==-1);
    Pipe bounded;
    for(unsigned i=0;i<256;++i) CHECK(bounded.queue(Ping,i,{}));
    CHECK(!bounded.queue(Ping,257,{}));
    CHECK(!Pipe{}.queue(Delta,1,Bytes(max_body+1,0)));
    Bytes identityBytes; identity(identityBytes,c.compatibility); Reader reader{identityBytes}; PcNetworkCompatibility decoded{};
    CHECK(identity(reader,decoded) && reader.done() && same(decoded,c.compatibility));
    identityBytes.back()=0; Reader invalidReader{identityBytes}; CHECK(!identity(invalidReader,decoded));
    std::printf("native network schema/framing: PASS checks=%u\n",checks);
    return 0;
}
