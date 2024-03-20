#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "mbr.h"
#include "mktcimg.h"

int prepare_mbr_partition(partition_info *pinfo, u32 pNum, struct partition_list *plist, u64 disksize)
{
    u32 idx, pindex = 0;
    u64 usedsec = 0;
    u64 total_extpartsec = 0;
    u64 used_extpartsec = 0;

    u32 unprefixed = 0;

    if(pNum <= PRIMARY_PARTITION_MAX){
        pinfo->nPrimaryPart = pNum;
        pinfo->nExtendedPart = 0;
    }else{
        if(pNum <= PARTITION_MAX){
            pinfo->nPrimaryPart = PRIMARY_PARTITION_MAX;
            pinfo->nExtendedPart = pNum - (PRIMARY_PARTITION_MAX - 1);
        }else{
            FAIL_MSG("Max Partition is Exceeded !!\n");
            return -1;
        }
    }

    if(pinfo->nPrimaryPart > 0 ){
        usedsec += BYTES_TO_SECTOR(SIZE_OF_MBR);

        for(idx = 0; idx < pinfo->nPrimaryPart -1; idx++){
            pinfo->PrimaryPart[idx].Parttype = plist[pindex].parttype;
            pinfo->PrimaryPart[idx].nSector = plist[pindex].size;
            usedsec += plist[pindex].size;

            if(pinfo->PrimaryPart[idx].nSector == 0)
                unprefixed = pindex;

            pindex++;

        }

        if(disksize > usedsec){
            if(pinfo->nExtendedPart == 0){
                if(unprefixed){
                    pinfo->PrimaryPart[unprefixed].nSector
                        = disksize - usedsec;
                    plist[unprefixed].size = 
                    pinfo->PrimaryPart[unprefixed].nSector;
                }else{
                    pinfo->PrimaryPart[pinfo->nPrimaryPart - 1].nSector
                        = disksize - usedsec;
                }
            }else{
                pinfo->PrimaryPart[pinfo->nPrimaryPart - 1].nSector
                    = disksize - usedsec;
            }
        }

        if(pinfo->nExtendedPart == 0){
            pinfo->PrimaryPart[pinfo->nPrimaryPart - 1].Parttype
                = plist[pindex].parttype;
        }else{
            pinfo->PrimaryPart[pinfo->nPrimaryPart - 1].Parttype
                = PART_TYPE_EXTDOSLBA;

            total_extpartsec = pinfo->PrimaryPart[pinfo->nPrimaryPart - 1].nSector;

            for(idx = 0; idx < pinfo->nExtendedPart ; idx++){

                pinfo->ExtendedPart[idx].Parttype = plist[pindex].parttype;

                if(plist[pindex].size == 0) unprefixed = pindex;

                if(idx < pinfo->nExtendedPart - 1){
                    pinfo->ExtendedPart[idx].nSector = plist[pindex].size;
                    used_extpartsec += plist[pindex].size + BYTES_TO_SECTOR(SIZE_OF_EBR);
                }else{
                    if(total_extpartsec > (used_extpartsec + BYTES_TO_SECTOR(SIZE_OF_EBR))){
                        if(unprefixed){
                            if(unprefixed < pinfo->nPrimaryPart - 1){

                                pinfo->ExtendedPart[idx].nSector = plist[pindex].size;
                                used_extpartsec += plist[pindex].size + BYTES_TO_SECTOR(SIZE_OF_EBR);

                                pinfo->PrimaryPart[unprefixed].nSector
                                    = total_extpartsec - used_extpartsec ;
                                plist[unprefixed].size = 
                                pinfo->PrimaryPart[unprefixed].nSector;

                                pinfo->PrimaryPart[pinfo->nPrimaryPart - 1].nSector = used_extpartsec;

                            }else{
                                pinfo->ExtendedPart[idx].nSector = plist[pindex].size;
                                used_extpartsec += plist[pindex].size + BYTES_TO_SECTOR(SIZE_OF_EBR);

                                pinfo->ExtendedPart[unprefixed - (PRIMARY_PARTITION_MAX -1) ].nSector
                                    += total_extpartsec - used_extpartsec;

                                plist[unprefixed].size = 
                                pinfo->ExtendedPart[unprefixed - (PRIMARY_PARTITION_MAX -1) ].nSector;
                            }
                        }else{
                            pinfo->ExtendedPart[idx].nSector
                                = total_extpartsec - (used_extpartsec + BYTES_TO_SECTOR(SIZE_OF_EBR));
                        }

                    }else{
                        FAIL_MSG("Max Disk Size Exceeded !! \n");
                        return -1;
                    }
                }
                pindex++;
            }
        }
    }

    DumpHex(pinfo, sizeof(partition_info));
   // DumpHex(plist, sizeof(struct partition_list)*5);

    return 0;

}

int make_mbr(partition_info *pinfo, char *mbr)
{
    unsigned int idx;
    u64 StartLBAAddress;
    u64 SizeInSector;
    u8  systemid;

    char *pmbr;

    StartLBAAddress = BYTES_TO_SECTOR(SIZE_OF_MBR);

    for(idx = 0; idx < pinfo->nPrimaryPart; idx++){

        systemid = (u8)pinfo->PrimaryPart[idx].Parttype;
        SizeInSector = pinfo->PrimaryPart[idx].nSector;

        switch(idx){
            case 0:
                pmbr = &mbr[446];
                break;
            case 1:
                pmbr = &mbr[462];
                break;
            case 2:
                pmbr = &mbr[478];
                break;
            case 3:
                pmbr = &mbr[494];
                break;
            default :
                return 0;
        }

        /* boot_id , begin_head, begin_sector, begin_cylinder */
        memset(pmbr , 0x0, sizeof(char)*4);

        pmbr[4] = systemid;

        pmbr[5] = 0x00; // end_head
        pmbr[6] = 0x00; // end_sector
        pmbr[7] = 0x00; // end_cylinder
        pmbr[8] = (u8)(StartLBAAddress & 0xFF);
        pmbr[9] = (u8)((StartLBAAddress >> 8) & 0xFF);
        pmbr[10] = (u8)((StartLBAAddress >> 16) & 0xFF);
        pmbr[11] = (u8)((StartLBAAddress >> 24) & 0xFF);
        
        pmbr[12] = (u8)(SizeInSector & 0xFF);
        pmbr[13] = (u8)((SizeInSector >> 8) & 0xFF);
        pmbr[14] = (u8)((SizeInSector >> 16) & 0xFF);
        pmbr[15] = (u8)((SizeInSector >> 24) & 0xFF);

        StartLBAAddress += SizeInSector;
        //DumpHex(pmbr, 16);
    }

    mbr[510] = 0x55;
    mbr[511] = 0xAA;

    //DumpHex(mbr, 512);

    return BYTES_TO_SECTOR(SIZE_OF_MBR);
}

int make_ebr(partition_info *pinfo, u32 pindex, char *ebr)
{    
    unsigned int idx, idy;
    u64 StartLBAAddress;
    u64 SizeInSector;
    u8  systemid;

    u64 partsec;

    char *pebr;

    for(idx = 0; idx < 2 ; idx++){

        DEBUG("pindex : %x , next %x , idx : %x \n" , pindex , pinfo->nExtendedPart , idx);
        partsec = 0;
        systemid = 0;

        if(idx == 0){
            StartLBAAddress = BYTES_TO_SECTOR(SIZE_OF_EBR);
            partsec = pinfo->ExtendedPart[pindex].nSector;
        }else{
            StartLBAAddress = 0;
            for(idy = 0; idy <= pindex; idy++){
                StartLBAAddress += pinfo->ExtendedPart[idy].nSector + BYTES_TO_SECTOR(SIZE_OF_EBR); 
            }

            if((pindex + 1 != pinfo->nExtendedPart) 
                    && (pinfo->ExtendedPart[pindex +1].nSector > 0)){

                DEBUG("nsec : %llx \n" , pinfo->ExtendedPart[pindex+1].nSector );
                partsec = pinfo->ExtendedPart[pindex+1].nSector + BYTES_TO_SECTOR(SIZE_OF_EBR); 
            }
        }

        if(idx == 0){
            systemid = (u8)(pinfo->ExtendedPart[pindex].Parttype); 
        }else{
            systemid = PART_TYPE_EXTDOSLBA;
        }

        if(idx == 0) pebr = &ebr[446];
        else pebr = &ebr[462];

        if(partsec > 0){

            /* boot_id , begin_head, begin_sector, begin_cylinder */
            memset(pebr , 0x0, sizeof(char)*4);

            pebr[4] = systemid;

            pebr[5] = 0x00; // end_head
            pebr[6] = 0x00; // end_sector
            pebr[7] = 0x00; // end_cylinder
            pebr[8] = (u8)(StartLBAAddress & 0xFF);
            pebr[9] = (u8)((StartLBAAddress >> 8) & 0xFF);
            pebr[10] = (u8)((StartLBAAddress >> 16) & 0xFF);
            pebr[11] = (u8)((StartLBAAddress >> 24) & 0xFF);

            pebr[12] = (u8)(partsec & 0xFF);
            pebr[13] = (u8)((partsec >> 8) & 0xFF);
            pebr[14] = (u8)((partsec >> 16) & 0xFF);
            pebr[15] = (u8)((partsec >> 24) & 0xFF);

            StartLBAAddress += SizeInSector;

            DumpHex(pebr, 16);

        }
    }

    ebr[510] = 0x55;
    ebr[511] = 0xAA;
    DumpHex(ebr, 512);

    return BYTES_TO_SECTOR(SIZE_OF_EBR);

}



