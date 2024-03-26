
#include "common.h"



int unpack_fai(char *filename);
int check_part_type(mbr_record *mbr);

int parse_bunch(FILE *funpack, unsigned int pcnt , tagDiskImageBunchHeaderType *hbunch);
int unpack_mbr_partition(FILE *funpack, unsigned int pcnt , tagDiskImageBunchHeaderType *hbunch);
int unpack_gpt_partition(FILE *funpack, unsigned long long disk_size 
        ,unsigned int pcnt , tagDiskImageBunchHeaderType *hbunch);
int make_gpt_partition_list(struct partition_list *plist, unsigned int partcnt);
