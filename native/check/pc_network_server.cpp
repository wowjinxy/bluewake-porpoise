// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit standalone owned LAN host; no game, CARD, default startup or credentials.
#include "pc/network/network_session.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
int main(int argc,char** argv){
    const char* bind="127.0.0.1";const char* store=nullptr;unsigned port=49383,seconds=300;
    for(int i=1;i<argc;i+=2){
        if(i+1>=argc){std::fputs("Expected option value\n",stderr);return 2;}
        if(!std::strcmp(argv[i],"--bind"))bind=argv[i+1];
        else if(!std::strcmp(argv[i],"--store"))store=argv[i+1];
        else if(!std::strcmp(argv[i],"--port")||!std::strcmp(argv[i],"--seconds")){
            const char* text=argv[i+1];if(!*text||std::strspn(text,"0123456789")!=std::strlen(text)){std::fputs("Invalid number\n",stderr);return 2;}
            char* end=nullptr;const unsigned long value=std::strtoul(text,&end,10);
            if(*end||value>65535){std::fputs("Number outside bounds\n",stderr);return 2;}
            if(!std::strcmp(argv[i],"--port"))port=unsigned(value);else seconds=unsigned(value);
        }else {std::fputs("Unknown host option\n",stderr);return 2;}
    }
    if(!store||!*store||seconds<1||seconds>600){std::fputs("Use --store DIR [--bind IPv4] [--port 0..65535] [--seconds 1..600]\n",stderr);return 2;}
    char error[128]{};auto server=pc_network_server_start_persistent(bind,uint16_t(port),store,error,sizeof error);
    if(!server){std::fprintf(stderr,"Native host failed: %s\n",error);return 1;}
    std::printf("{\"native_host_ready\":true,\"protocol\":%u,\"port\":%u}\n",PC_NETWORK_PROTOCOL,pc_network_server_port(server));std::fflush(stdout);
    std::this_thread::sleep_for(std::chrono::seconds(seconds));pc_network_server_stop(server);return 0;
}
