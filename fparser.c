#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "gpt.h"
#include "mbr.h"


u32 parse_file_line(FILE *fp)
{
	u32 line = 0;
	char text[4096], *p;
	fseek(fp, 0, SEEK_SET);

	while(fgets(text, sizeof(text), fp)) {
		p = strtok(text, "\r\n");
		if(p == NULL) {
			p = strtok(text, "\n");
		}
		if(p != NULL){
			u32 i;
			int check[2] = {0,};
			for(i = 0; i < strlen(p); i++) {
				if(*(p + i) == ':') {
					check[0] = 1;
				}
				if(*(p + i) == '@') {
					check[1] = 1;
				}
			}

			if(check[0] == 1 && check[1] == 1) {
				line++;
			}
		}
	}
	return line;
}

u32 parse_parttype(char *type)
{
    if(!strcmp(type , "raw"))
        return 0x00;
    else if(!strcmp(type, "ext4"))
        return 0x83;
    else return -1;
}

u64 parse_size(char *sz)
{
	int l = strlen(sz);
	u64 n = strtoull(sz, 0, 10);
	if(l){
		switch(sz[l-1]){
			case 'k':
			case 'K':
				n *= 1024;
				break;
			case 'm':
			case 'M':
				n *= (1024*1024);
				break;
			case 'g':
			case 'G':
				n *= (1024*1024*1024);
		}
	}

	return n;
}

struct partition_list* parse_gpt_ptn(FILE *fp, u32 nline)
{
	u32 idx;
	struct partition_list *plist;
	char text[4096], *p;

	plist = malloc(sizeof(struct partition_list) * nline);
	if(plist == NULL) {
		FAIL_MSG("Memory allocation is failed : %s __ %d \n", __func__, __LINE__);
		return NULL;
	}

	idx = 0;
	fseek(fp, 0, SEEK_SET);

	DEBUG("========== Parsing Partition List==========\n");
	while(fgets(text, sizeof(text), fp))
	{
		DEBUG("PART %d====================\n",idx+1);
		p = strtok(text, ":");
		memcpy(plist[idx].name, p, strlen(p));
		DEBUG("name : %s \n", plist[idx].name);
		p = strtok(NULL , "@");
		plist[idx].size = BYTES_TO_SECTOR(parse_size(p));
		plist[idx].byte_size = parse_size(p);
		DEBUG("size : %llu sector(%llu byte)\n", plist[idx].size, plist[idx].byte_size);
		p = strtok(NULL, "\r\n");
		if(p == NULL) {
			p = strtok(NULL, "\n");
		}
		if(p != NULL){
			memcpy(plist[idx].path, p, strlen(p));
			DEBUG("location : %s \n", plist[idx].path);
		} else {
			DEBUG("location : (NULL) \n");
		}
		idx++;
		if(idx >= nline) {
			break;
		}
	}
	return plist;
}

struct partition_list* parse_mbr_ptn(FILE *fp, u32 nline)
{
	u32 idx;
	struct partition_list *plist;

  	plist = malloc(sizeof(struct partition_list) * nline);
	idx = 0;

	char text[4096], *p;
	fseek(fp, 0, SEEK_SET);

	while(fgets(text, sizeof(text), fp))
	{
        DEBUG("Disk Image Header Write Failed \n");
		p = strtok(text, ":");
		memcpy(plist[idx].name, p, strlen(p));
		DEBUG("location : %s \n", plist[idx].name);
		p = strtok(NULL , "@");	
		plist[idx].size = BYTES_TO_SECTOR(parse_size(p));
		DEBUG("location : %llu \n", plist[idx].size);
		p = strtok(NULL, "$");
		if(p != NULL){
			memcpy(plist[idx].path, p, strlen(p));
			DEBUG("location : %s \n", plist[idx].path);
		}
		p = strtok(NULL, "\r\n");
		if(p == NULL) {
			p = strtok(NULL, "\n");
		}
        DEBUG("Disk Image Header Write Failed \n");
		plist[idx].parttype = parse_parttype(p);
		DEBUG("location : 0x%x \n", plist[idx].parttype);
		idx++;
		if(idx >= nline) {
			break;
		}
	}
    DEBUG("path : %s \n", plist[0].path);
	return plist;
}
