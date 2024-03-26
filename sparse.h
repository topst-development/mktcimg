#include "common.h"

#define SPARSE_HEADER_MAGIC	0xED26FF3A
#define CHUNK_TYPE_RAW		0xCAC1
#define CHUNK_TYPE_FILL		0xCAC2
#define CHUNK_TYPE_DONT_CARE	0xCAC3
#define CHUNK_TYPE_CRC		0xCAC4

#define PACKETSIZE		64*1024


typedef struct sparse_header {
	u32	magic;
	u16	major_version;
	u16	minor_version;
	u16	file_hdr_sz;
	u16	chunk_hdr_sz;
	u32	blk_sz;
	u32	total_blks;
	u32	total_chunks;
	u32	image_checksum;
} sparse_header_t;

typedef struct chunk_header {
	u16	chunk_type;
	u16	reserved1;
	u32	chunk_sz;
	u32	total_sz;
} chunk_header_t;

int check_sparse_image(FILE *);
int sparse_image_write(FILE *infd, FILE *outfd, u64 uLba);
int check_sparse_image_size(FILE *infd, u64 partition_size);
int get_sparse_chunk_count(FILE *fd);
