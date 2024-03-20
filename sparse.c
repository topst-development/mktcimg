#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "sparse.h"
#include "mktcimg.h"

#define FILL_DATA_SIZE			4


int check_sparse_image(FILE *fd)
{
	sparse_header_t	sparse_header;

	fread(&sparse_header, 1, sizeof(sparse_header_t), fd);

	if(sparse_header.magic == SPARSE_HEADER_MAGIC) return 0;
	else return -1;
}

int check_sparse_image_size(FILE *infd, u64 partition_sector_size)
{
	sparse_header_t sparse_header;
	u64 origin_image_size = 0ULL;
	u64 partition_size = partition_sector_size * sector_size;

	fseek(infd, 0, SEEK_SET);
	fread(&sparse_header , 1 , sizeof(sparse_header_t), infd);

	origin_image_size = sparse_header.blk_sz * sparse_header.total_blks;

	if(partition_size < origin_image_size) {
		FAIL_MSG("Size of sparse image(%llu bytes) is larger than size of partition(%llu bytes)\n", origin_image_size, partition_size);
		return -1;
	} else  {
		return 0;
	}
}

int sparse_image_write(FILE *infd, FILE *outfd, u64 uLba)
{
	u32 chunk;
	u32 chunk_data_sz;
	sparse_header_t sparse_header;
	chunk_header_t chunk_header;
	u64 total_blocks = 0;
	u64 total_cnt = 0;
	u8 *chunk_buf;

	tagDiskImageBunchHeaderType BunchHeader;

	fseek(infd, 0, SEEK_SET);
	fread(&sparse_header , 1 , sizeof(sparse_header_t), infd);
	total_cnt += sparse_header.file_hdr_sz;

	if(sparse_header.file_hdr_sz > sizeof(sparse_header_t))
	{
		/*
		 * Skip the remaining bytes in a header that is longer than 
		 * we expected.
		*/

		total_cnt += (sparse_header.file_hdr_sz - sizeof(sparse_header_t));
	}

	DEBUG ( "=== Sparse Image Header ===\n");
	DEBUG ( "magic: 0x%x\n", sparse_header.magic);
	DEBUG ( "major_version: 0x%x\n", sparse_header.major_version);
	DEBUG ( "minor_version: 0x%x\n", sparse_header.minor_version);
	DEBUG ( "file_hdr_sz: %d\n", sparse_header.file_hdr_sz);
	DEBUG ( "chunk_hdr_sz: %d\n", sparse_header.chunk_hdr_sz);
	DEBUG ( "blk_sz: %d\n", sparse_header.blk_sz);
	DEBUG ( "total_blks: %d\n", sparse_header.total_blks);
	DEBUG ( "total_chunks: %d\n", sparse_header.total_chunks);

	for(chunk = 0; chunk < sparse_header.total_chunks; chunk++){
		
		fread(&chunk_header, 1, sizeof(chunk_header_t), infd);
		total_cnt += sizeof(chunk_header_t);

		DEBUG ( "=== Chunk Header ===\n");
		DEBUG ( "chunk_type: 0x%x\n", chunk_header.chunk_type);
		DEBUG ( "chunk_data_sz: 0x%x\n", chunk_header.chunk_sz);
		DEBUG ( "total_size: 0x%x\n", chunk_header.total_sz);

		if(sparse_header.chunk_hdr_sz > sizeof(chunk_header_t)){
			/*
			 * Skip the remaining bytes in a header that is longer than 
			 * we expected.
			 */
			total_cnt += (sparse_header.chunk_hdr_sz - sizeof(chunk_header_t));
		}

		chunk_data_sz = sparse_header.blk_sz * chunk_header.chunk_sz;

		switch(chunk_header.chunk_type){

			case CHUNK_TYPE_RAW:
				chunk_buf = malloc(PACKETSIZE);
				memset(chunk_buf, 0x0, PACKETSIZE);

				BunchHeader.ullTargetAddress = (uLba * sector_size) + (total_blocks * sparse_header.blk_sz);
				BunchHeader.ullLength = (u64)chunk_data_sz;
				Write_BunchHeader(outfd, &BunchHeader);

				if(chunk_header.total_sz != (sparse_header.chunk_hdr_sz + chunk_data_sz)){
					FAIL_MSG("Bugs Chunks Size for chunk type Raw\n");
					return -1;
				}

				while(chunk_data_sz > 0){

					u32 packetsize = (chunk_data_sz > PACKETSIZE) ? PACKETSIZE : (u32)chunk_data_sz;

					chunk_data_sz -= packetsize;

					fread(chunk_buf, 1 , packetsize, infd);

					total_cnt += packetsize;

					fwrite(chunk_buf, 1, packetsize, outfd);
				}
				total_blocks += chunk_header.chunk_sz;

				DiskInfo.ullPartitionInfoStartOffset += get_file_offset(outfd);

				free(chunk_buf);
				break;
			case CHUNK_TYPE_FILL:
			if(bSparseFill == 0)
			{
				u8 *fill_data;
				u32 offset;
				chunk_buf = malloc(PACKETSIZE);
				memset(chunk_buf, 0x0, PACKETSIZE);

				BunchHeader.ullTargetAddress = (uLba * sector_size) + (total_blocks * sparse_header.blk_sz)    ;
				BunchHeader.ullLength = (u64)chunk_data_sz;
				Write_BunchHeader(outfd, &BunchHeader);

				if((unsigned int)chunk_header.total_sz != (unsigned int)(sparse_header.chunk_hdr_sz + 4)){
					FAIL_MSG("Bugs Chunks Size for chunk type Raw\n");
					return -1;
				}

				fread(chunk_buf, 1 , 4, infd);

				while(chunk_data_sz > 0) {
					u32 packetsize = (chunk_data_sz > PACKETSIZE) ? PACKETSIZE : (u32)chunk_data_sz;
					fill_data = malloc(packetsize);
					chunk_data_sz -= packetsize;

					for(offset = 0; offset < packetsize; offset += FILL_DATA_SIZE) {
						memcpy(fill_data + offset, chunk_buf, FILL_DATA_SIZE);
					}

					total_cnt += packetsize;
					fwrite(fill_data, 1, packetsize, outfd);

					free(fill_data);
				}

				total_blocks += chunk_header.chunk_sz;
				DiskInfo.ullPartitionInfoStartOffset += get_file_offset(outfd);
				free(chunk_buf);

				break;
			}
			else
			{
				//for 4byte sync
				chunk_buf = malloc(PACKETSIZE);
				memset(chunk_buf, 0x0, PACKETSIZE);
				fread(chunk_buf, 1 , 4, infd);

				total_blocks += chunk_header.chunk_sz;

				break;
			}
			case CHUNK_TYPE_DONT_CARE:
				total_blocks += chunk_header.chunk_sz;
				break;
			case CHUNK_TYPE_CRC:
				if(chunk_header.total_sz != (u32)(sparse_header.chunk_hdr_sz +4)){
					FAIL_MSG("Bugs Chunks size for chunk type crc \n");
					return -1;
				}
				total_blocks += chunk_header.chunk_sz;
				fseek(infd, SEEK_CUR, chunk_data_sz);
				break;

			default:
				FAIL_MSG("unkown chunk type : %d\n", chunk_header.chunk_type);
				return -1;
		}
	}

	return 0;
}
