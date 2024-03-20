#pragma once

typedef unsigned char  		u8;
typedef unsigned short int 	u16;
typedef unsigned int 		u32;
typedef unsigned long long 	u64;

extern u64 sector_size;
extern u64 sector_shift;
#define BYTES_TO_SECTOR(x)	((x + (sector_size - 1)) >> sector_shift)

#define DBG

#ifdef DBG
#define DEBUG(fmt, args...) printf(fmt, ##args)
#else
#define DEBUG(gmt, args...) do {} while (0)
#endif

#define FAIL_MSG(fmt, args...) printf("\033[31m"fmt"\033[0m", ##args)
#define SUCCESS_MSG(fmt, args...) printf("\033[32m"fmt"\033[0m", ##args)

#ifdef DBG
void DumpHex(const void* data, unsigned int size);
#else
#define DumpHex(gmt, args...) do {} while (0)
#endif
