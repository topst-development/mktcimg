#include "common.h"

#define PRIMARY_PARTITION_MAX   0x04
#define PARTITION_MAX           0x1A

#define PART_TYPE_RAW           0x00
#define PART_TYPE_LINUX_NATIVE  0x83
#define PART_TYPE_EXTDOSLBA     0x0F

#define RESERVED_SECTOR         0x20 /* MBR + Reserved */

#define SIZE_OF_MBR             0x4000
#define SIZE_OF_EBR             0x4000

typedef struct {
    u64     nSector;
    u32     Parttype;
}partition_prop;


typedef struct {
    u32     nPrimaryPart;
    partition_prop      PrimaryPart[PRIMARY_PARTITION_MAX];
    u32     nExtendedPart;
    partition_prop      ExtendedPart[PARTITION_MAX];
    u32     disktype;
    u32     diskinfosize;
}partition_info;

struct chs {
    u8  head;
    u8  sector;
    u8  cylinder;
} __attribute__((packed));

struct partition_record{
    u8  status;
    struct chs  start;
    u8  type;
    struct chs  end;
    u32 start_lba;
    u32 len_lba;
} __attribute__((packed));

struct master_boot_record {
    u8      code[440];
    u32     disk_sig;
    u16     pad;
    struct partition_record ptable[PRIMARY_PARTITION_MAX];
    u16 mbr_sig;
}__attribute__((packed));

typedef struct partition_record part_record;
typedef struct master_boot_record mbr_record;
    

struct partition_list *parse_mbr_ptn(FILE *fp, u32 nline);

int prepare_mbr_partition(partition_info *pinfo, u32 pNum, struct partition_list *plist, u64 disksize);
int make_mbr(partition_info *pinfo, char *mbr);
int make_ebr(partition_info *pinfo, u32 pindex, char *ebr);


