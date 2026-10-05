// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_session.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
static std::atomic<bool> stopped{false};
static void stop(int){stopped=true;}
static bool number(const char* text,unsigned maximum,unsigned& out){
    if(!text||!*text)return false;unsigned value=0;
    for(;*text;++text){if(*text<'0'||*text>'9'||value>(maximum-unsigned(*text-'0'))/10)return false;value=value*10+unsigned(*text-'0');}
    out=value;return true;
}
static int run(int argc,char** argv){
    const char* bind="127.0.0.1";const char* directory="bluewake-network-state";unsigned port=49383,seconds=0;bool ephemeral=false;
    for(int i=1;i<argc;++i){
        if(!std::strcmp(argv[i],"--ephemeral")){ephemeral=true;continue;}
        if(i+1>=argc)return 2;
        if(!std::strcmp(argv[i],"--bind"))bind=argv[++i];
        else if(!std::strcmp(argv[i],"--state-dir"))directory=argv[++i];
        else if(!std::strcmp(argv[i],"--port")){if(!number(argv[++i],65535,port))return 2;}
        else if(!std::strcmp(argv[i],"--seconds")){if(!number(argv[++i],86400,seconds))return 2;}else return 2;}
    char error[160]{};
    BwNetworkServer* server=ephemeral?bw_network_server_start(bind,uint16_t(port)):
        bw_network_server_start_persistent(bind,uint16_t(port),directory,error,sizeof error);
    if(!server){std::fprintf(stderr,"%s\n",error[0]?error:"Cannot bind BlueWake dedicated server");return 1;}
    std::signal(SIGINT,stop);std::signal(SIGTERM,stop);
    std::printf("{\"ready\":true,\"namespace\":\"%s\",\"port\":%u,\"durable\":%s}\n",BW_NETWORK_NAMESPACE,bw_network_server_port(server),ephemeral?"false":"true");std::fflush(stdout);
    const auto start=std::chrono::steady_clock::now();
    while(!stopped&&(!seconds||std::chrono::steady_clock::now()-start<std::chrono::seconds(seconds)))std::this_thread::sleep_for(std::chrono::milliseconds(25));
    bw_network_server_stop(server);return 0;
}
#ifdef _WIN32
int wmain(int argc,wchar_t** wide){
    std::vector<std::string> strings;std::vector<char*> argv;
    for(int i=0;i<argc;++i){const int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,nullptr,0,nullptr,nullptr);
        if(count<=0)return 2;std::string text(size_t(count),'\0');
        if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,&text[0],count,nullptr,nullptr)!=count)return 2;
        text.pop_back();strings.push_back(std::move(text));}
    for(auto& text:strings)argv.push_back(&text[0]);return run(argc,argv.data());
}
#else
int main(int argc,char** argv){return run(argc,argv);}
#endif
