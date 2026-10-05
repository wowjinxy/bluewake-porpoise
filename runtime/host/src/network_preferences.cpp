// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_session.h"
#include "network_wire.h"
#include "atomic_file.h"
#include "network_digest.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <set>
#include <string>

namespace {
void error(char* out,unsigned size,const char* text){if(out&&size)std::snprintf(out,size,"%s",text);}
bool terminated(const char* s,size_t size){return std::memchr(s,0,size)!=nullptr;}
bool hex(const char* s,size_t length){if(std::strlen(s)!=length)return false;for(size_t i=0;i<length;++i)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;return true;}
bool printable(const char* s,bool empty=false){if(!empty&&!*s)return false;for(;*s;++s)if(uint8_t(*s)<32||uint8_t(*s)>126)return false;return true;}
bool slug(const char* s){if(!*s)return false;for(;*s;++s)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='-'||*s=='_'))return false;return true;}
bool address(const char* s){if(!std::strcmp(s,"localhost"))return true;in6_addr v6{};in_addr v4{};
    return inet_pton(AF_INET,s,&v4)==1||inet_pton(AF_INET6,s,&v6)==1;}
std::string room_namespace(const BwNetworkConfig& c){
    /* Hash every compatibility/endpoint/room field with unambiguous lengths.
     * This keeps normal Windows paths short without placing a server address
     * into the filesystem. Full manifest equality is still required on wire. */
    std::string identity;const auto add=[&](const std::string& s){identity+=std::to_string(s.size())+":"+s;};
    add(BW_NETWORK_NAMESPACE);add(std::to_string(c.compatibility.progression_schema));
    add(c.compatibility.game_id);add(c.compatibility.build_id);add(c.compatibility.module_digest);
    add(c.compatibility.options_digest);add(c.server);add(std::to_string(c.port));add(c.room);
    return std::string("Network/")+bw_net::digest(identity)+"/"+c.player_id;
}
void defaults(BwNetworkPreferences& p){p={};std::strcpy(p.config.server,"127.0.0.1");p.config.port=49383;
    std::strcpy(p.config.room,"bluewake");std::strcpy(p.config.player_name,"Link");
    std::strcpy(p.config.compatibility.game_id,"GZLE01");p.config.compatibility.progression_schema=BW_PROGRESSION_SCHEMA;
    for(char* field:{p.config.compatibility.build_id,p.config.compatibility.module_digest,p.config.compatibility.options_digest}){std::memset(field,'0',64);field[64]=0;}
    std::random_device random;const char* alphabet="0123456789abcdef";
    for(unsigned i=0;i<32;++i)p.config.player_id[i]=alphabet[random()&15u];p.config.player_id[32]=0;
}
std::filesystem::path preferences(const char* directory){return std::filesystem::u8path(directory)/"network.ini";}
}
extern "C" bool bw_network_config_valid(const BwNetworkConfig* c,char* out,unsigned size){
    if(!c){error(out,size,"Missing network configuration");return false;}
    if(!terminated(c->server,sizeof c->server)||!terminated(c->room,sizeof c->room)||!terminated(c->player_id,sizeof c->player_id)||
       !terminated(c->player_name,sizeof c->player_name)||!terminated(c->room_password,sizeof c->room_password)||
       !terminated(c->compatibility.game_id,sizeof c->compatibility.game_id)||!terminated(c->compatibility.build_id,65)||
       !terminated(c->compatibility.module_digest,65)||!terminated(c->compatibility.options_digest,65)){
        error(out,size,"Unterminated network configuration field");return false;}
    if(c->compatibility.progression_schema!=BW_PROGRESSION_SCHEMA||std::strcmp(c->compatibility.game_id,"GZLE01")||
       !hex(c->compatibility.build_id,64)||!hex(c->compatibility.module_digest,64)||!hex(c->compatibility.options_digest,64)){
        error(out,size,"Invalid game/build/module/options compatibility manifest");return false;}
    if(!c->port||!address(c->server)||!slug(c->room)||!hex(c->player_id,32)||
       !printable(c->player_name)||!printable(c->room_password,true)){
        error(out,size,"Use numeric server IP/localhost, port, ASCII room/name and32-hex player ID");return false;}
    error(out,size,"");return true;
}
extern "C" bool bw_network_same_room(const BwNetworkConfig* a,const BwNetworkConfig* b){
    return bw_network_config_valid(a,nullptr,0)&&bw_network_config_valid(b,nullptr,0)&&
        a->port==b->port&&std::strcmp(a->server,b->server)==0&&std::strcmp(a->room,b->room)==0&&
        std::strcmp(a->player_id,b->player_id)==0&&std::strcmp(a->compatibility.game_id,b->compatibility.game_id)==0&&
        std::strcmp(a->compatibility.build_id,b->compatibility.build_id)==0&&std::strcmp(a->compatibility.module_digest,b->compatibility.module_digest)==0&&
        std::strcmp(a->compatibility.options_digest,b->compatibility.options_digest)==0&&a->compatibility.progression_schema==b->compatibility.progression_schema;
}
extern "C" bool bw_network_room_card_path(const char* dir,const BwNetworkConfig* c,char* path,unsigned capacity,char* out,unsigned size){
    if(!dir||!*dir||!path||!bw_network_config_valid(c,out,size))return false;
    try{const auto base=std::filesystem::absolute(std::filesystem::u8path(dir)).lexically_normal();
        const auto folder=(base/std::filesystem::u8path(room_namespace(*c))).lexically_normal();
        const auto resolved_base=std::filesystem::weakly_canonical(base);
        const auto resolved_folder=std::filesystem::weakly_canonical(folder);
        const auto relative=resolved_folder.lexically_relative(resolved_base);
        if(relative.empty()||*relative.begin()==".."){error(out,size,"Room directory escaped data directory");return false;}
        const std::string route=(folder/"GZLE01.card").u8string();
        if(route.size()>=capacity){error(out,size,"Room CARD path buffer too small");return false;}
        std::error_code ec;std::filesystem::create_directories(folder,ec);
        if(ec){error(out,size,"Cannot create isolated room CARD directory");return false;}
        const auto final_relative=std::filesystem::canonical(folder).lexically_relative(resolved_base);
        if(final_relative.empty()||*final_relative.begin()==".."){error(out,size,"Room directory escaped data directory");return false;}
        const auto card=folder/"GZLE01.card";
        if(std::filesystem::exists(card)){
            const auto card_relative=std::filesystem::canonical(card).lexically_relative(resolved_base);
            if(card_relative.empty()||*card_relative.begin()==".."||!std::filesystem::is_regular_file(card)||
               (std::filesystem::exists(base/"GZLE01.card")&&std::filesystem::equivalent(card,base/"GZLE01.card"))){
                error(out,size,"Room CARD must be an isolated file inside data directory");return false;}}
        std::memcpy(path,route.c_str(),route.size()+1);error(out,size,"");return true;
    }catch(...){error(out,size,"Cannot resolve isolated room CARD directory");return false;}
}
extern "C" bool bw_network_preferences_save(const char* dir,const BwNetworkPreferences* p,char* out,unsigned size){
    if(!dir||!*dir||!p||!bw_network_config_valid(&p->config,out,size))return false;
    try{const std::string path=preferences(dir).u8string();char* pending=bw_atomic_path(path.c_str());
        FILE* file=pending?bw_atomic_open(pending,"wb"):nullptr;
        if(!file){free(pending);error(out,size,"Cannot open pending network preferences");return false;}
        const auto& c=p->config;
        bool ok=std::fprintf(file,"version=1\nroom_mode=%d\nserver=%s\nport=%u\nroom=%s\nplayer_id=%s\nplayer_name=%s\nroom_password=%s\ncreate_room=%d\ngame_id=%s\nbuild_id=%s\nmodule_digest=%s\noptions_digest=%s\nprogression_schema=%u\n",
            p->room_mode,c.server,c.port,c.room,c.player_id,c.player_name,c.room_password,c.create_room,c.compatibility.game_id,
            c.compatibility.build_id,c.compatibility.module_digest,c.compatibility.options_digest,c.compatibility.progression_schema)>0;
        ok=bw_atomic_finish(file,pending,path.c_str(),ok);free(pending);
        error(out,size,ok?"":"Cannot atomically persist network preferences");return ok;
    }catch(...){error(out,size,"Cannot persist network preferences");return false;}
}
extern "C" bool bw_network_preferences_load(const char* dir,BwNetworkPreferences* p,char* out,unsigned size){
    if(!dir||!*dir||!p){error(out,size,"Missing preferences data directory");return false;}
    try{defaults(*p);const std::string path=preferences(dir).u8string();FILE* file=bw_atomic_open(path.c_str(),"rb");
        if(!file){if(errno==ENOENT){error(out,size,"");return true;}error(out,size,"Cannot read network preferences");return false;}
        char line[1024];std::set<std::string> seen;bool ok=true;unsigned bytes=0;
        while(ok&&std::fgets(line,sizeof line,file)){
            const size_t n=std::strlen(line);bytes+=unsigned(n);if(bytes>16384||(!n||line[n-1]!='\n')){ok=false;break;}
            std::string s(line,n-1);if(!s.empty()&&s.back()=='\r')s.pop_back();
            const size_t split=s.find('=');if(split==std::string::npos){ok=false;break;}
            const std::string key=s.substr(0,split),value=s.substr(split+1);if(!seen.insert(key).second){ok=false;break;}
            auto text=[&](char* field,size_t cap){if(value.size()>=cap){ok=false;return;}std::memcpy(field,value.c_str(),value.size()+1);};
            auto number=[&](unsigned maximum,unsigned* result){if(value.empty()||value.find_first_not_of("0123456789")!=std::string::npos){ok=false;return;}
                unsigned long long v=0;for(char digit:value){v=v*10+unsigned(digit-'0');if(v>maximum){ok=false;return;}}*result=unsigned(v);};
            auto& c=p->config;unsigned v=0;
            if(key=="version"){number(1,&v);if(v!=1)ok=false;}
            else if(key=="room_mode"){number(1,&v);p->room_mode=v!=0;}
            else if(key=="create_room"){number(1,&v);c.create_room=v!=0;}
            else if(key=="port"){number(65535,&v);c.port=uint16_t(v);}
            else if(key=="progression_schema"){number(BW_PROGRESSION_SCHEMA,&v);c.compatibility.progression_schema=v;}
            else if(key=="server")text(c.server,sizeof c.server);else if(key=="room")text(c.room,sizeof c.room);
            else if(key=="player_id")text(c.player_id,sizeof c.player_id);else if(key=="player_name")text(c.player_name,sizeof c.player_name);
            else if(key=="room_password")text(c.room_password,sizeof c.room_password);else if(key=="game_id")text(c.compatibility.game_id,sizeof c.compatibility.game_id);
            else if(key=="build_id")text(c.compatibility.build_id,65);else if(key=="module_digest")text(c.compatibility.module_digest,65);
            else if(key=="options_digest")text(c.compatibility.options_digest,65);else ok=false;
        }
        if(std::ferror(file))ok=false;std::fclose(file);
        if(!ok||seen.size()!=14||!seen.count("version")||!bw_network_config_valid(&p->config,nullptr,0)){
            error(out,size,"Invalid network.ini; room routing must fail closed, never use personal CARD");return false;}
        error(out,size,"");return true;
    }catch(...){error(out,size,"Cannot read network preferences");return false;}
}
