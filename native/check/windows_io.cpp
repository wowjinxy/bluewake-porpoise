#include "pc/cos_pc_config.h"
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <type_traits>
int main() {
    static_assert(sizeof(decltype(ftello((FILE*)nullptr))) == 8);
    _set_fmode(_O_BINARY);
    const unsigned char bytes[] = {0,10,13,10,26,255,65,10,0,13,26,90};
    const char* path = "windows-io-fixture.bin";
    int fd = open(path, O_CREAT | O_EXCL | O_RDWR, _S_IREAD | _S_IWRITE);
    if(fd < 0 || write(fd, bytes, sizeof(bytes)) != sizeof(bytes)) return 1;
    struct stat st{};
    if(fstat(fd, &st) || st.st_size != sizeof(bytes)) return 2;
    unsigned char readback[sizeof(bytes)]{};
    if(_lseeki64(fd,0,SEEK_SET) != 0 || read(fd,readback,sizeof(readback)) != sizeof(bytes) || memcmp(bytes,readback,sizeof(bytes))) return 3;
    if(_lseeki64(fd,7,SEEK_SET) != 7 || pread(fd,readback,3,2) != 3 || memcmp(readback,bytes+2,3) || _lseeki64(fd,0,SEEK_CUR) != 7) return 4;
    if(close(fd)) return 5;
    FILE* f=fopen(path,"rb");
    if(!f || fseeko(f,-2,SEEK_END) || ftello(f) != sizeof(bytes)-2) return 6;
    fclose(f);
    timespec a{}, b{}, real{};
    if(clock_gettime(CLOCK_MONOTONIC,&a) || clock_gettime(CLOCK_REALTIME,&real)) return 7;
    Sleep(20);
    if(clock_gettime(CLOCK_MONOTONIC,&b) || b.tv_sec<a.tv_sec || (b.tv_sec==a.tv_sec && b.tv_nsec<a.tv_nsec)) return 8;
    if(clock_gettime(-999,&b) != -1 || errno != EINVAL || clock_gettime(CLOCK_MONOTONIC,nullptr) != -1 || errno != EINVAL) return 9;
    if(unlink(path)) return 10;
    puts("PASS Windows binary I/O, matching stat layout, positioned reads, 64-bit stdio offsets, monotonic/realtime clocks and invalid-clock errors");
    return 0;
}
