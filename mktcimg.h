
#include "common.h"

#define TAG_AREA_IMAGE_HEADER			"[HEADER]"
#define TAG_AREA_IMAGE_TYPE_KERNEL_IMAGE	"KERNEL_IMAGE"
#define TAG_AREA_IMAGE_TYPE_RAW_IMAGE		"RAW_IMAGE"
#define TAG_AREA_IMAGE_TYPE_KEYBOX_IMAGE	"KEYBOX_IMAGE"
#define TAG_AREA_IMAGE_TYPE_UID_IMAGE		"UID_IMAGE"

#define TAG_AREA_IMAGE_TYPE_DISK_IMAGE		"FILESYSTEM_IMAGE"
#define TAG_DISK_IMAGE_VERSION			"TCC FAT IMG V0.1"
#define TAG_DISK_IMAGE_SIZE			"DISKSIZE"
#define TAG_DISK_IMAGE_PARTITION_CNT		"PART_CNT"

#define PRIMARY_PARTITION_MAX   0x04
#define PARTITION_MAX           0x1A

#define PART_TYPE_RAW           0x00
#define PART_TYPE_LINUX_NATIVE  0x83
#define PART_TYPE_EXTDOSLBA     0x0F

#define RESERVED_SECTOR         0x20 /* MBR + Reserved */

#define SIZE_OF_MBR             0x4000
#define SIZE_OF_EBR             0x4000



typedef struct {
	u8  	tagHeader[8];
	u32  	ulHeaderSize;
	u32  	ulCRC32;

	u8 	tagImageType[16];

	u8 	tagVersion[16];

	u8 	areaName[16];

	u8 	tagDiskSize[8];
	u64	llDiskSize;

	u8 	tagPartitionCount[8];
	u32  	ulPartitionCount;
	u32 	ulDummy2;
}tagDiskImageHeader;

typedef struct {
	u64 ullTargetAddress;
	u64 ullLength;
}tagDiskImageBunchHeaderType;


typedef struct {
	u64 ullPartitionInfoStartOffset;
}tagGDiskInfo;

struct partition_list {
	u8     name[256];
	u64    byte_size;
	u64    size;
	u8     path[4096];
    u8     parttype;
};

tagGDiskInfo DiskInfo;
int bSparseFill;
unsigned long long get_file_offset(FILE *fd);
int Write_BunchHeader(FILE *fd , tagDiskImageBunchHeaderType *BunchHeader);

