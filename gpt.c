
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <uuid/uuid.h>
#include "gpt.h"

void fill_crc32(struct guid_partition_tbl *ptbl)
{
	struct uefi_header *hdr = &ptbl->guid_header;
	u32 crc;

	crc = 0; hdr->crc32=0;
	crc = calculate_crc32((unsigned char*)&ptbl->guid_entry, 
			hdr->efi_entries_count*ENTRY_SIZE);
	hdr->efi_entries_crc32 = crc;
	DEBUG("crc32 of header : %x \n" , crc);

	crc = 0; hdr->crc32 = 0;
	crc = calculate_crc32((unsigned char*)&ptbl->guid_header, 
			hdr->header_size);
	hdr->crc32 = crc;

	DEBUG("crc32 of partition array : %x \n" , crc);
}

void prepare_guid_header(struct guid_partition_tbl *ptbl, unsigned int lba)
{
	struct uefi_header *hdr = &ptbl->guid_header;

	memcpy(hdr->magic, GUID_MAGIC, 8);
	hdr->version		= GUID_VERSION;
	hdr->header_size	= sizeof(struct uefi_header);
	hdr->header_lba		= 1;
	hdr->backup_lba		= lba - 1;
	hdr->first_lba		= 34;
	hdr->last_lba		= lba - 34;
	memcpy(hdr->guid, volume_guid, 16);
	hdr->efi_entries_lba	= 2;
	hdr->efi_entries_count	= MAX_PARTITIONS;
	hdr->efi_entries_size	= ENTRY_SIZE;
}

void prepare_backup_guid_header(struct uefi_header *hdr)
{
	u64 primary_lba = hdr->header_lba;
	hdr->header_lba = hdr->backup_lba;
	hdr->backup_lba = primary_lba;
	hdr->efi_entries_lba = hdr->last_lba + 1;
}

int guid_add_partition(struct guid_partition_tbl *ptbl, u64 first_lba,
		u64 last_lba, u8 *name)
{
	struct uefi_header *hdr = &ptbl->guid_header;
	struct gpt_partition_entry *entry = ptbl->guid_entry;
	uuid_t binuuid;
	char str[37];
	u32 idx;

	if(first_lba < GUID_RESERVED){
		FAIL_MSG("partition %s Overwrapped GUID RESERVED AREA \n", name);
		return -1;
	}

	if(last_lba > hdr->last_lba){
		FAIL_MSG("Partition %s Over Size to Storage Size %llu , %llu\n", name, hdr->last_lba, last_lba);
		return -1;
	}

	for (idx = 0; idx < ENTRY_SIZE; idx++, entry++){
		if(entry->last_lba) continue;

		//uuid_generate_time_safe(binuuid);
		uuid_generate_random(binuuid);
		uuid_unparse(binuuid, str);
		memcpy(entry->type_guid, guid_partition_type[1], 16);
		//memcpy(entry->type_guid, &binuuid, 16);
		memcpy(entry->unique_partition_guid, &binuuid, 16);
		entry->first_lba = first_lba;
		entry->last_lba = last_lba;

		DEBUG("uuid : %s , part-name : %s \n", str, name);
		for(idx = 0; (idx < MAX_GPT_NAME_SIZE / 2 ) && *name ; idx++)
			entry->name[idx*2] = *name++;

		return 0;
	}

	FAIL_MSG("Unrecognized partition %s \n", name);
	return -1;
}

unsigned int sizeof_guid_partition_tbl(void)
{
	const unsigned int guid_partition_tbl_sector = 34;
	return sector_size * guid_partition_tbl_sector;
}