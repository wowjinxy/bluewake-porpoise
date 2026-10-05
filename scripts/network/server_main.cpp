// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_session.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
static std::atomic<bool> stopped{false};
static void stop(int){stopped=true;}
int main(int argc,char** argv){
    const char* bind="127.0.0.1";unsigned port=49383,seconds=0;
    for(int i=1;i<argc;++i){if(i+1>=argc)return 2;
        if(!std::strcmp(argv[i],"--bind"))bind=argv[++i];
        else if(!std::strcmp(argv[i],"--port"))port=unsigned(std::strtoul(argv[++i],nullptr,10));
        else if(!std::strcmp(argv[i],"--seconds"))seconds=unsigned(std::strtoul(argv[++i],nullptr,10));else return 2;}
    if(port>65535||seconds>86400)return 2;
    BwNetworkServer* server=bw_network_server_start(bind,uint16_t(port));if(!server){std::fprintf(stderr,"Cannot bind BlueWake dedicated server\n");return 1;}
    std::signal(SIGINT,stop);std::signal(SIGTERM,stop);
    std::printf("{\"ready\":true,\"namespace\":\"%s\",\"port\":%u}\n",BW_NETWORK_NAMESPACE,bw_network_server_port(server));std::fflush(stdout);
    const auto start=std::chrono::steady_clock::now();
    while(!stopped&&(!seconds||std::chrono::steady_clock::now()-start<std::chrono::seconds(seconds)))std::this_thread::sleep_for(std::chrono::milliseconds(25));
    bw_network_server_stop(server);return 0;
}
