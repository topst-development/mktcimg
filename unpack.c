#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "mbr.h"
#include "gpt.h"
#include "unpack.h"

#define SZ_BUFF (1*1024*1024)

#define HEADER_SIZE_PROTECTED_MBR     sector_size
#define HEADER_SIZE_GPT_HEADER        sector_size
#define HEADER_SIZE_GPT_ENTRY         (sector_size * 32)

#define OFFSET_PROTECTED_MBR 0
#define OFFSET_GPT_HEADER   (OFFSET_PROTECTED_MBR + HEADER_SIZE_PROTECTED_MBR)
#define OFFSET_GPT_ENTRY    (OFFSET_GPT_HEADER + HEADER_SIZE_GPT_HEADER)

enum partition_type{UNKNOWN , MBR , GPT};

int check_part_type(mbr_record *mbr)
{
    unsigned int part_type = UNKNOWN;

    if(mbr->ptable[0].type == 0xEE){
        DEBUG("This Part Type is GPT\n");
        part_type = GPT;
    }else{
        DEBUG("This Part Type is MBR\n");
        part_type = MBR;
    }
    return part_type;
}

int calc_bunch(unsigned int pcnt, unsigned int part_type)
{
    switch(part_type){
        case 1:
            if(pcnt <= 4) return pcnt+1; // partition count + MBR
            else return pcnt + 1 + (pcnt - 3); // partition count + MBR + EBR
            DEBUG("pcnt : %x \n " , pcnt);
            break;
        case 2:
            return pcnt + 3; // partition count + MBR + GPT_HEADER + PART_ENTRY
            DEBUG("pcnt : %x \n " , pcnt);
            break;
        default:
            return -1;
    }
}

int parse_bunch(FILE *funpack, unsigned int pcnt , tagDiskImageBunchHeaderType *hbunch)
{
    unsigned int idx;

    fseek(funpack, sizeof(tagDiskImageHeader), SEEK_SET);

    for(idx = 0; idx < pcnt ; idx++){

        fread(&hbunch[idx], sizeof(char), sizeof(tagDiskImageBunchHeaderType), funpack);
        DumpHex(&hbunch[idx], sizeof(tagDiskImageBunchHeaderType));

        if(hbunch[idx].ullLength == 0){
            continue;
        }else fseek(funpack, hbunch[idx].ullLength, SEEK_CUR);
    }

    return 0;
}

int unpack_mbr_partition(FILE *funpack, unsigned int pcnt , tagDiskImageBunchHeaderType *hbunch)
{
    unsigned int idx, partcnt = 0;
    unsigned long long remain = 0;
    mbr_record  *mbr;

    char *readbuf;
    FILE *outfd;
    char *partname;

    readbuf = malloc(sizeof(char) * 0x1000000);
    mbr = malloc(sizeof(mbr_record));
    partname = malloc(sizeof(char*) * 256);

    fseek(funpack, sizeof(tagDiskImageHeader), SEEK_SET);

    for(idx = 0; idx < pcnt ; idx++){

        fread(&hbunch[idx], sizeof(char), sizeof(tagDiskImageBunchHeaderType), funpack);
        DumpHex(&hbunch[idx], sizeof(tagDiskImageBunchHeaderType));

        if(hbunch[idx].ullLength == 0){
            partcnt++;
            continue;
        }
        else if (hbunch[idx].ullLength == 0x200){

            fread(mbr, sizeof(char), sizeof(mbr_record), funpack);
            if(mbr->mbr_sig == 0xAA55){
                DEBUG("found MBR OR EBR skip this bunch section \n");
                continue;
            }
        }
        else {

            remain = hbunch[idx].ullLength;
            sprintf(partname , "partition_%d.img\n", partcnt);
            outfd = fopen(partname, "w+");
            
            while(remain){
                memset(readbuf, 0x0, 0x100000);

                if(remain >= 0x1000000){
                    if(!fread(readbuf, sizeof(char), 0x100000, funpack))
                        return -1;
                    if(!fwrite(readbuf, sizeof(char), 0x100000, outfd))
                        return -1;

                    remain -= 0x100000;

                }else{
                    if(!fread(readbuf, sizeof(char), remain, funpack))
                        return -1;
                    if(!fwrite(readbuf, sizeof(char), remain, outfd))
                        return -1;

                    remain = 0;
                }
            }
            fclose(outfd);
            partcnt++;
        }
    }

    free(partname);
    free(readbuf);
    free(mbr);
    return 0;
}

int unpack_gpt_partition(FILE *funpack, unsigned long long disk_size 
        ,unsigned int pcnt , tagDiskImageBunchHeaderType *hbunch)
{
    unsigned int idx ,nameidx, partcnt = 0;
    unsigned long long remain = 0;

    struct guid_partition_tbl *guid_record;

    char *readbuf;
    FILE *outfd;
    char *partname;

    readbuf = malloc(sizeof(char) * 0x1000000);
    guid_record = malloc(sizeof(struct guid_partition_tbl));
    guid_record->mbr = malloc(sector_size);
    partname = malloc(sizeof(char) * 128);

    DEBUG("Jume to %d (0x%x)\n", (int)sizeof(tagDiskImageHeader), (int)sizeof(tagDiskImageHeader));
    fseek(funpack, sizeof(tagDiskImageHeader), SEEK_SET); // set to 0x60

    // fai header
    DEBUG("Current position : %d ( %X ) \n", (int)ftell(funpack), (int)ftell(funpack));

    DEBUG("disk_size : %llx \n" , disk_size);

    for(idx = 0; idx < pcnt ; idx++){
        DEBUG("=====================================\n\n");
        DEBUG("Read address: %d ( 0x%X ) \n", (int)ftell(funpack), (int)ftell(funpack));
        fread(&hbunch[idx], sizeof(char), sizeof(tagDiskImageBunchHeaderType), funpack);
        DumpHex(&hbunch[idx], sizeof(tagDiskImageBunchHeaderType));

        DEBUG("Target address : %llu Length : %llu\n", hbunch[idx].ullTargetAddress, hbunch[idx].ullLength);
        DEBUG("pcnt : %d , part_cnt : %d \n" , idx , partcnt);

        if(hbunch[idx].ullTargetAddress !=0 && hbunch[idx].ullLength == 0){
            DEBUG("<< EMPTY PARTITION >>\n");
            partcnt++;
            continue;
        }else if (hbunch[idx].ullTargetAddress == OFFSET_PROTECTED_MBR
                && hbunch[idx].ullLength == HEADER_SIZE_PROTECTED_MBR){
            DEBUG("<< PROTECTED MBR (%ld)>>\n", sizeof(guid_record->mbr));
            fread(guid_record->mbr, sizeof(char), sizeof(guid_record->mbr), funpack);

            DEBUG("Size of Protected MBR structure : %llu\n", sector_size);
            outfd = fopen("protected_mbr.img", "w+");
            if ( outfd == NULL )
                continue;

            if(!fwrite(guid_record->mbr, sizeof(char), sizeof(guid_record->mbr), outfd)){
                DEBUG("Protected mbr writing failed\n");
            }
            fclose(outfd);

            DEBUG("pcnt : %d , part_cnt : %d  uuid : %s \n"
                    , idx , partcnt, guid_record->guid_entry[0].unique_partition_guid);
            continue;
        }else if (hbunch[idx].ullTargetAddress == OFFSET_GPT_ENTRY
                && hbunch[idx].ullLength == HEADER_SIZE_GPT_ENTRY){
            DEBUG("<< GPT ENTRY INFO >>\n");
            fread(guid_record->guid_entry, sizeof(char), 
                    sizeof(struct gpt_partition_entry) * ENTRY_SIZE, funpack);

            outfd = fopen("gpt_entry.img", "w+");
            if ( outfd == NULL )
                continue;

            if(!fwrite(guid_record->guid_entry , sizeof(char),  sizeof(struct gpt_partition_entry) *
                        ENTRY_SIZE , outfd)){
                DEBUG("GPT header writing failed\n");
            }
            fclose(outfd);

            int i;
            for(i=0; i<ENTRY_SIZE; i++){
                memset(partname, 0, 128);
                for(nameidx = 0 ; (nameidx < MAX_GPT_NAME_SIZE / 2); nameidx ++)
                    partname[nameidx] = guid_record->guid_entry[i].name[nameidx*2];

                if(partname[0] != 0x00)
                    DEBUG("partname : %s\n" , partname);
            }
            //DumpHex(guid_record->guid_entry , sizeof(struct gpt_partition_entry) * ENTRY_SIZE);
            continue;

        } else if (hbunch[idx].ullTargetAddress == OFFSET_GPT_HEADER && hbunch[idx].ullLength == HEADER_SIZE_GPT_HEADER){
            DEBUG("<< GPT Header (%d) >>\n", (int)sizeof(guid_record->alloc));
            int ret = fread(guid_record->alloc, sizeof(char), sizeof(guid_record->alloc), funpack);
            //int ret = fread(guid_record->alloc,  sizeof(guid_record->alloc), 1,  funpack);
            DEBUG("Size of read bytes : %d\n", ret);
            outfd = fopen("gpt_header.img", "w+");
            if ( outfd == NULL )
                continue;

            if(!fwrite(guid_record->alloc , sizeof(char), sizeof(guid_record->alloc), outfd)){
                FAIL_MSG("GPT header writing failed\n");
            }
            fclose(outfd);
            //DumpHex(guid_record->alloc , sizeof(guid_record->alloc));
            continue;
        } else if (hbunch[idx].ullTargetAddress == 0 && hbunch[idx].ullLength == 0x4400){
        /* PROTECTED MBR */
            DEBUG("<< PROTECTED MBR (%d)>>\n", 512);//sizeof(guid_record->mbr));
            fread(guid_record->mbr, sizeof(char), 512/*sizeof(guid_record->mbr)*/, funpack);

            DEBUG("Size of Protected MBR structure : %d\n", (int)sector_size);
            outfd = fopen("protected_mbr.img", "w+");
            if ( outfd == NULL )
                continue;

            if(!fwrite(guid_record->mbr, sizeof(char), 512/*sizeof(guid_record->mbr)*/, outfd)){
                DEBUG("Protected mbr writing failed\n");
            }
            fclose(outfd);

            DEBUG("pcnt : %d , part_cnt : %d  uuid : %s \n"
                    , idx , partcnt, guid_record->guid_entry[0].unique_partition_guid);

            /* GPT Header */
            DEBUG("<< GPT Header (%d) >>\n", (int)sizeof(guid_record->alloc));
            int ret = fread(guid_record->alloc, sizeof(char), sizeof(guid_record->alloc), funpack);
            //int ret = fread(guid_record->alloc,  sizeof(guid_record->alloc), 1,  funpack);
            DEBUG("Size of read bytes : %d\n", ret);
            outfd = fopen("gpt_header.img", "w+");
            if ( outfd == NULL )
                continue;

            if(!fwrite(guid_record->alloc , sizeof(char), sizeof(guid_record->alloc), outfd)){
                FAIL_MSG("GPT header writing failed\n");
            }
            fclose(outfd);

            /* GPT ENTRY INFO */
            DEBUG("<< GPT ENTRY INFO >>\n");
            fread(guid_record->guid_entry, sizeof(char),
                    sizeof(struct gpt_partition_entry) * ENTRY_SIZE, funpack);

            outfd = fopen("gpt_entry.img", "w+");
            if ( outfd == NULL )
                continue;

            if(!fwrite(guid_record->guid_entry , sizeof(char),  sizeof(struct gpt_partition_entry) *
                        ENTRY_SIZE , outfd)){
                DEBUG("GPT header writing failed\n");
            }
            fclose(outfd);

            int i;
            for(i=0; i<ENTRY_SIZE; i++){
                memset(partname, 0, 128);
                for(nameidx = 0 ; (nameidx < MAX_GPT_NAME_SIZE / 2); nameidx ++)
                    partname[nameidx] = guid_record->guid_entry[i].name[nameidx*2];

                if(partname[0] != 0x00)
                    DEBUG("partname : %s\n" , partname);
            }

         } else {

            remain = hbunch[idx].ullLength;
         //   fread(&guid_record->guid_entry[partcnt], sizeof(char),sizeof(struct gpt_partition_entry), funpack);
            for(nameidx = 0 ; (nameidx < MAX_GPT_NAME_SIZE / 2); nameidx ++){
                partname[nameidx] = guid_record->guid_entry[partcnt].name[nameidx*2];
            }
            //sprintf( partname, "%d" , partcnt);
            DEBUG("partname : %s (%d)\n" , partname, partcnt);
            strcat(partname ,".img");
            outfd = fopen(partname, "w+");

            while(remain){
                memset(readbuf, 0x0, 0x100000);

                if(remain >= 0x1000000){
                    if(!fread(readbuf, sizeof(char), 0x100000, funpack))
                        return -1;
                    if(!fwrite(readbuf, sizeof(char), 0x100000, outfd))
                        return -1;
                    remain -= 0x100000;

                }else{
                    if(!fread(readbuf, sizeof(char), remain, funpack))
                        return -1;
                    if(!fwrite(readbuf, sizeof(char), remain, outfd))
                        return -1;
                    remain = 0;
                }
            }
            fclose(outfd);
            partcnt++;
        }
    }

    free(partname);
    free(readbuf);
    free(guid_record->mbr);
    free(guid_record);
    return 0;
}


int unpack_fai(char *filename)
{
    FILE *funpack;
    tagDiskImageHeader *hfwdn;
    tagDiskImageBunchHeaderType *hbunch;
    mbr_record  *mbr;

    unsigned int part_type ;
    unsigned int bunch_cnt ;
    unsigned long long disk_size;

    part_type = 0;
    bunch_cnt = 0;
    disk_size = 0;

    hfwdn = malloc(sizeof( tagDiskImageHeader));
    mbr = malloc(sizeof(mbr_record));

    funpack = fopen(filename, "r+");
    if(funpack == NULL) {
        FAIL_MSG("file : %s is not exist\n", filename);
        return -1;
    }
    fread(hfwdn, sizeof(char), sizeof(tagDiskImageHeader), funpack);
    DumpHex(hfwdn, sizeof(tagDiskImageHeader));

    DEBUG("number of patition : %d \n" , hfwdn->ulPartitionCount);

    disk_size = (unsigned long long)(hfwdn->llDiskSize);

    DEBUG("Disk Size : %llx \n" , hfwdn->llDiskSize);

    fseek(funpack, sizeof(tagDiskImageHeader) + sizeof(tagDiskImageBunchHeaderType), SEEK_SET);
    fread(mbr, sizeof(char), sizeof(mbr_record), funpack);
    DumpHex(mbr, sizeof(mbr_record));

    part_type = check_part_type(mbr);

    bunch_cnt = calc_bunch(hfwdn->ulPartitionCount, part_type);
    DEBUG("bunch cnt : %d \n" , bunch_cnt);

    hbunch = malloc(sizeof(tagDiskImageBunchHeaderType) * bunch_cnt);
    parse_bunch(funpack, bunch_cnt, hbunch);
 
    unpack_gpt_partition(funpack, disk_size, bunch_cnt, hbunch);
#if 0
    switch(part_type){
        case 1:
            unpack_mbr_partition(funpack, bunch_cnt, hbunch);
            break;
        case 2:
            unpack_gpt_partition(funpack, disk_size, bunch_cnt, hbunch);
            break;
        default :
            return -1;
    }
#endif
    
    free(hbunch);
    free(hfwdn);
    free(mbr);

    fclose(funpack);

    return 0;

}
