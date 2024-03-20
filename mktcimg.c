#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>

#include "gpt.h"
#include "mbr.h"
#include "sparse.h"
#include "unpack.h"

#include "parse_args.h"

void OnCreateFsImage(tagDiskImageHeader *DiskImageHeader, u32 pcount, u64 disksize, char *area_name);
void MakeMBR(int);
int Write_BunchHeader(FILE *, tagDiskImageBunchHeaderType *);
int Write_Secondary_GPT(FILE *fd, struct guid_partition_tbl * ptbl);
int Write_Praimary_GPT(FILE *fd, struct guid_partition_tbl * ptbl);
int Write_GPT_Entries(FILE *fd, struct guid_partition_tbl * ptbl);
int Write_GPT_Header(FILE *fd, struct guid_partition_tbl * ptbl);
int Write_Protected_MBR(FILE *fd, struct guid_partition_tbl * ptbl);

u64 sector_size = 512ULL;
u64 sector_shift = 9ULL;

u64 DISK_MakeCRC(FILE *outfd)
{
    u8 * pBuffer; 
    u32 iReadSize;
    u32 nCRC32 = 0;
    u64 llCheckSize;

    pBuffer = malloc(sizeof(char)*64*1024);
    fseek(outfd, 0, SEEK_SET);
    do{

        iReadSize = fread(pBuffer, 1, 64*1024, outfd);
        if(iReadSize <= 0)
            break;

        llCheckSize += iReadSize;
        nCRC32 = tc_calcCRC32(pBuffer, iReadSize, nCRC32);

    }while(iReadSize == 64*1024);

    free(pBuffer);

    return nCRC32;
}

unsigned long long get_file_offset(FILE *fd)
{
    fseek(fd, 0, SEEK_END);
    return ftell(fd);
}

void OnCreateFsImage(tagDiskImageHeader *DiskImageHeader, 
        u32 pcount, u64 disksize, char *area_name)
{
    /* Initialize Disk Image Header */
    memset(DiskImageHeader, 0x0, sizeof(tagDiskImageHeader));
    memcpy(DiskImageHeader->tagHeader, TAG_AREA_IMAGE_HEADER, 8);
    DiskImageHeader->ulHeaderSize = sizeof(tagDiskImageHeader);
    strncpy((char *)DiskImageHeader->tagImageType, TAG_AREA_IMAGE_TYPE_DISK_IMAGE, 16);
    memcpy(DiskImageHeader->tagVersion, TAG_DISK_IMAGE_VERSION, 16);
    //strncpy((char *)DiskImageHeader->areaName, "SD Data" ,16);
    //strncpy((char *)DiskImageHeader->areaName, "SNOR Data" ,16);
    strncpy((char *)DiskImageHeader->areaName, area_name ,16);
    memcpy(DiskImageHeader->tagDiskSize, TAG_DISK_IMAGE_SIZE, 8);
    memcpy(DiskImageHeader->tagPartitionCount, TAG_DISK_IMAGE_PARTITION_CNT, 8);

    DiskImageHeader->ulPartitionCount= pcount;
    DiskImageHeader->llDiskSize = disksize * sector_size;
}

static void prepare_mbr(u8  *mbr, u32 lba)
{
    mbr[0x1be] = 0x00; // bootalbe == false
    mbr[0x1bf] = 0xFF; // CHS
    mbr[0x1c0] = 0xFF; // CHS
    mbr[0x1c1] = 0xFF; // CHS

    mbr[0x1c2] = 0xEE; //MBR_PROTECTED_TYPE; // SET GPT partitoin
    mbr[0x1c3] = 0xFF; // CHS
    mbr[0x1c4] = 0xFF; // CHS
    mbr[0x1c5] = 0xFF; // CHS

    mbr[0x1c6] = 0x01; // Relative Start Sector
    mbr[0x1c7] = 0x00;
    mbr[0x1c8] = 0x00;
    mbr[0x1c9] = 0x00;

    memcpy(mbr + 0x1ca, &lba, sizeof(u32));

    mbr[0x1fe] = 0x55; // MBR Signature
    mbr[0x1ff] = 0xAA; // MBR Signature
}

int Write_BunchHeader(FILE *fd , tagDiskImageBunchHeaderType *BunchHeader)
{
    fseek(fd, 0, DiskInfo.ullPartitionInfoStartOffset);
    if(!fwrite(BunchHeader, sizeof(char), sizeof(tagDiskImageBunchHeaderType), fd)){
        return -1;
    }
    DiskInfo.ullPartitionInfoStartOffset += get_file_offset(fd);
    return 0;
}

int Write_Protected_MBR(FILE *fd, struct guid_partition_tbl * ptbl)
{
    int res = 0;

    if(!fwrite(ptbl->mbr, sizeof(char), sector_size, fd)) {
        FAIL_MSG("Protected MBR Write Failed \n");
        res = -1;
    }

    return res;
}

int Write_GPT_Header(FILE *fd, struct guid_partition_tbl * ptbl)
{
    int res = 0;
    char *buf = (char*)malloc(sector_size);

    if(buf == NULL) {
        FAIL_MSG("Memory allocation is failed : %s __ %d \n", __func__, __LINE__);
        res = -1;
    }

    if(res == 0) {
        memset(buf, 0, sector_size);
        memcpy(buf, &(ptbl->guid_header), sizeof(ptbl->guid_header));
        if(!fwrite(buf, sizeof(char), sector_size, fd)) {
            res = -1;
            FAIL_MSG("GPT Header Write Failed \n");
        }
    }

    if(buf != NULL) {
        free(buf);
    }

    return res;
}

int Write_GPT_Entries(FILE *fd, struct guid_partition_tbl * ptbl)
{
    int res = 0;
    const int entry_sector_size = 32;
    char *buf = (char*)malloc(sector_size * entry_sector_size);

    if(buf == NULL) {
        FAIL_MSG("Memory allocation is failed : %s __ %d \n", __func__, __LINE__);
        res = -1;
    }

    if(res == 0) {
        memset(buf, 0, sector_size * entry_sector_size);
        memcpy(buf, ptbl->guid_entry, sizeof(struct gpt_partition_entry) * ENTRY_SIZE);
        if(!fwrite(buf, sizeof(char), sector_size * entry_sector_size, fd)) {
            res = -1;
            FAIL_MSG("GPT Entries Write Failed \n");
        }
    }

    if(buf != NULL) {
        free(buf);
    }

    return res;
}

int Write_Praimary_GPT(FILE *fd, struct guid_partition_tbl * ptbl)
{
    if(Write_Protected_MBR(fd, ptbl) != 0)
        return -1;
    if(Write_GPT_Header(fd, ptbl) != 0)
        return -1;
    if(Write_GPT_Entries(fd, ptbl) != 0)
        return -1;

    return 0;
}

int Write_Secondary_GPT(FILE *fd, struct guid_partition_tbl * ptbl)
{
    if(Write_GPT_Entries(fd, ptbl) != 0)
        return -1;
    if(Write_GPT_Header(fd, ptbl) != 0)
        return -1;

    return 0;
}

int OnCreateGPTHeader(struct guid_partition_tbl *ptbl,
        struct partition_list *plist, u64 storage_size, u32 nplist)
{
    u32 idx; 
    u64 npart, size;

    npart = GUID_RESERVED;

    prepare_mbr(ptbl->mbr, storage_size);
    prepare_guid_header(ptbl, storage_size);
    for(idx = 0; idx < nplist; idx++){
        size = plist[idx].size;
        if(size == 0) {
            size = storage_size - npart - GUID_RESERVED + 1;
            plist[idx].size = size;
        }
        if(guid_add_partition(ptbl, npart , npart+size-1, plist[idx].name) == -1) {
            return -1;
        }
        npart += size;
    }

    fill_crc32(ptbl);

    return 0;
}

int mktcimg_mbr(FILE *infd, FILE *outfd, u64 storage_size, char *area_name)
{
    struct partition_list *plist;
    partition_info *pinfo;
    tagDiskImageHeader  DiskImageHeader;
    tagDiskImageBunchHeaderType BunchHeader;

    u32 pNum = 0;
    u32 idx;
    u64 remain, disksector;
    char *fbuf;
    char *mbr = NULL, *ebr = NULL;

    FILE *fplist = NULL;
    pinfo = malloc(sizeof(partition_info));
    pNum = parse_file_line(infd);
    plist  = parse_mbr_ptn(infd, pNum);

    disksector = 0;

    prepare_mbr_partition(pinfo, pNum, plist, storage_size);

    /* FWDN HEADER */
    DiskInfo.ullPartitionInfoStartOffset = 0; 
    OnCreateFsImage(&DiskImageHeader, pNum , storage_size, area_name);
    if(!fwrite(&DiskImageHeader, sizeof(char), sizeof(tagDiskImageHeader), outfd)){
        FAIL_MSG("Disk Image Header Write Failed \n");
        return -1;
    }
    DiskInfo.ullPartitionInfoStartOffset = sizeof(tagDiskImageHeader); 


    BunchHeader.ullTargetAddress = 0;
    BunchHeader.ullLength = sizeof(char)*512;
    if(Write_BunchHeader(outfd, &BunchHeader)) goto error;

    mbr = malloc(sizeof(char)*512);
    memset(mbr, 0x00, sizeof(char)*512);
    /* MBR Write */
    disksector += make_mbr(pinfo, mbr);

    if(!fseek(outfd , 0 , get_file_offset(outfd))){
        FAIL_MSG("cant seek for MBR \n");
        goto error;
    }

    if(!fwrite(mbr, sizeof(char), sizeof(char)*512, outfd)){
        FAIL_MSG("MBR Write Failed \n");
        goto error;
    }

    ebr = malloc(sizeof(char)*512);

    DiskInfo.ullPartitionInfoStartOffset += get_file_offset(outfd); 

    for(idx = 0; idx < PARTITION_MAX ; idx++){
        if(plist[idx].size >  0){
            if(pinfo->nPrimaryPart >= 2 && pinfo->nExtendedPart != 0){
                if(idx > (pinfo->nPrimaryPart - 2)){

                    BunchHeader.ullTargetAddress = disksector * 512;
                    BunchHeader.ullLength = sizeof(char)*512;
                    if(Write_BunchHeader(outfd, &BunchHeader)) goto error;

                    memset(ebr, 0x00, sizeof(char)*512);
                    disksector += make_ebr(pinfo, idx - (pinfo->nPrimaryPart -1), ebr);

                    if(!fwrite(ebr, sizeof(char), sizeof(char)*512, outfd)){
                        FAIL_MSG("EBR Write Failed \n");
                        goto error;
                    }

                }
            }

            DEBUG("idx : %d  %s \n", idx , plist[idx].name);

            if(plist[idx].path[0] == '-'){
                BunchHeader.ullTargetAddress = disksector * 512;
                BunchHeader.ullLength = 0;
                if(Write_BunchHeader(outfd, &BunchHeader)) goto error;
                disksector += plist[idx].size;

            }else{

                fplist = fopen((char*)plist[idx].path, "r");

                if(fplist == NULL){
                    FAIL_MSG("file : %s can not open\n" , plist[idx].path);
                    goto error;
                }

                if(!check_sparse_image(fplist)){
                    DEBUG("sparse image is detected !!\n");
                    if(check_sparse_image_size(fplist, plist[idx].size) == -1) {
                        goto error;
                    }
                    sparse_image_write(fplist, outfd, disksector);
                    disksector += plist[idx].size;
                }else{
                    BunchHeader.ullTargetAddress = disksector * 512;
                    BunchHeader.ullLength = BYTES_TO_SECTOR(get_file_offset(fplist))*512;
                    if(Write_BunchHeader(outfd, &BunchHeader)) goto error;

                    disksector += plist[idx].size;

                    fseek(fplist, 0 , SEEK_SET);
                    fseek(outfd, 0, get_file_offset(outfd));
                    fbuf = malloc(sizeof(char) * 0x100000); /* 1MB Buffer */
                    if(fbuf == NULL){
                        FAIL_MSG("Memory allocation is failed : %s __ %d \n", __func__, __LINE__);
                        goto error;
                    }

                    remain = BYTES_TO_SECTOR(BunchHeader.ullLength)*512;

                    while(remain){

                        memset(fbuf, 0x0 , 0x100000);

                        if(remain >= 0x100000){
                            if(!fread(fbuf, 1, 0x100000, fplist)){
                                FAIL_MSG("read file : %s failed \n", plist[idx].path);
                                goto error;
                            }
                            if(!fwrite(fbuf, 1, 0x100000, outfd)){
                                FAIL_MSG("write outfile failed \n");
                                goto error;
                            }
                            remain -= 0x100000;
                        }else{
                            if(!fread(fbuf, 1, remain, fplist)){
                                FAIL_MSG("read file : %s failed \n", plist[idx].path);
                                goto error;
                            }
                            if(!fwrite(fbuf, 1, remain, outfd)){
                                FAIL_MSG("Write outfile failed \n");
                                goto error;
                            }
                            remain = 0;
                        }
                    }

                    DiskInfo.ullPartitionInfoStartOffset += get_file_offset(outfd);
                    free(fbuf);

                }
                    fclose(fplist);
            }

        }else break;
    }

    DiskImageHeader.ulCRC32 = DISK_MakeCRC(outfd);
    DEBUG ("CRC : %x \n" , DiskImageHeader.ulCRC32);
    fseek(outfd, 0, SEEK_SET);
    if(!fwrite(&DiskImageHeader, sizeof(char), sizeof(tagDiskImageHeader), outfd)){
        FAIL_MSG("outfile write error\n");
        goto error;
    }

    free(plist);
    free(pinfo);
    free(mbr);
    free(ebr);
    return 0;
error:
    if(plist != NULL) free(plist);
    if(pinfo != NULL) free(pinfo);
    if(fplist != NULL) fclose(fplist);
    if(mbr != NULL) free(mbr);
    if(ebr != NULL) free(ebr);
    return -2;
}

void strupr(char *str)
{
	int i=0;
	int len=0;

	len=strlen(str);

	for(i=0;i<len;i++)
	{
		*(str+i)=_toupper(*(str+i));
	}
}

int mktcimg_gpt(FILE *infd, FILE *outfd, FILE *gptfd, u64 storage_size, char *area_name)
{
    struct guid_partition_tbl ptbl;
    struct uefi_header *hdr = &ptbl.guid_header;
    struct partition_list *plist = NULL;
    tagDiskImageHeader	DiskImageHeader;
    tagDiskImageBunchHeaderType BunchHeader;

    u32 pNum = 0;
    u32 idx;
    char *fbuf = NULL;
    FILE *fplist = NULL;
    u64 remain;

    pNum = parse_file_line(infd);
    plist = parse_gpt_ptn(infd, pNum);
    if(plist == NULL) {
        goto error;
    }

    memset(&ptbl, 0 , sizeof(struct guid_partition_tbl));
    ptbl.mbr = malloc(sector_size);
    if(ptbl.mbr == NULL) {
        FAIL_MSG("Memory allocation is failed : %s __ %d \n", __func__, __LINE__);
        goto error;
    }

    /* FWDN HEADER */
    DiskInfo.ullPartitionInfoStartOffset = 0; 
    OnCreateFsImage(&DiskImageHeader, pNum , storage_size, area_name);
    if(!fwrite(&DiskImageHeader, sizeof(char), sizeof(tagDiskImageHeader), outfd)){
        FAIL_MSG("Disk Image Header Write Failed \n");
        goto error;
    }
    DiskInfo.ullPartitionInfoStartOffset = sizeof(tagDiskImageHeader); 

    BunchHeader.ullTargetAddress = 0;
    BunchHeader.ullLength = sizeof_guid_partition_tbl();
    if(Write_BunchHeader(outfd, &BunchHeader)) {
        goto error;
    }

    /* GPT HEADER AND PARTITION ENTRY */
    if(OnCreateGPTHeader(&ptbl, plist, storage_size, pNum) == -1) {
        goto error;
    }

    if(!fseek(outfd , 0 , get_file_offset(outfd))){
        FAIL_MSG("cant seek for GPT header \n");
        goto error;
    }

    if(Write_Praimary_GPT(outfd, &ptbl) != 0) {
        FAIL_MSG("GPT Header Write Failed \n");
        goto error;
    }

    if(gptfd != NULL) {
        if(Write_Praimary_GPT(gptfd, &ptbl) != 0) {
            FAIL_MSG("GPT Header Write Failed \n");
            goto error;
        }
    }

    if(ptbl.mbr != NULL) {
        free(ptbl.mbr);
        ptbl.mbr = NULL;
    }

    DiskInfo.ullPartitionInfoStartOffset += get_file_offset(outfd);

    for(idx = 0; idx < pNum ; idx++){
        if(plist[idx].size == 0 || plist[idx].path[0] == 0){
            BunchHeader.ullTargetAddress= ptbl.guid_entry[idx].first_lba * sector_size;
            BunchHeader.ullLength = 0;
            if(Write_BunchHeader(outfd, &BunchHeader)) goto error;
        }else{
            DEBUG("idx : %d  %s \n", idx , plist[idx].name);
            char tmp[sizeof(plist[idx].path)];
            strncpy(tmp,(char *)plist[idx].path,sizeof(plist[idx].path));
            strupr(tmp);
            if(!strcmp(tmp , "FORMAT"))
            {
                BunchHeader.ullTargetAddress= ptbl.guid_entry[idx].first_lba * sector_size;
                BunchHeader.ullLength = plist[idx].byte_size;

                if(Write_BunchHeader(outfd, &BunchHeader)) {
                        goto error;
                }
                fseek(outfd, 0, get_file_offset(outfd));

                fbuf = malloc(sizeof(char) * 0x100000); /* 1MB Buffer */
                if(fbuf == NULL){
                    FAIL_MSG("Memory allocation is failed : %s __ %d \n", __func__, __LINE__);
                    goto error;
                }

                remain = BunchHeader.ullLength;
                while(remain){
                    memset(fbuf, 0x00, 0x100000);
                    if(remain >= 0x100000){
                        if(!fwrite(fbuf, 1, 0x100000, outfd)){
                            FAIL_MSG("write outfile failed \n");
                            goto error;
                        }
                        remain -= 0x100000;
                        }else{
                            if(!fwrite(fbuf, 1, remain, outfd)){
                                FAIL_MSG("Write outfile failed \n");
                                goto error;
                            }
                            remain = 0;
                        }
                    }
                    DiskInfo.ullPartitionInfoStartOffset += get_file_offset(outfd);
                    free(fbuf);
                    fbuf = NULL;
            }
            else
            {
                fplist = fopen((char*)plist[idx].path, "r");

                if(fplist == NULL){
                    FAIL_MSG("file : %s can not open\n" , plist[idx].path);
                    goto error;
                }

                if(!check_sparse_image(fplist)){
                    DEBUG("sparse image is detected !!\n");
                    if(check_sparse_image_size(fplist, plist[idx].size) == -1) {
                            goto error;
                    }
                    sparse_image_write(fplist, outfd, ptbl.guid_entry[idx].first_lba);
                }else{
                    unsigned long long ullPartitionSize
                        = (ptbl.guid_entry[idx].last_lba - ptbl.guid_entry[idx].first_lba + 1)
                            * sector_size;
                    BunchHeader.ullTargetAddress= ptbl.guid_entry[idx].first_lba * sector_size;
                    BunchHeader.ullLength = BYTES_TO_SECTOR(get_file_offset(fplist))*sector_size;

                    if (BunchHeader.ullLength > ullPartitionSize) {
                        FAIL_MSG(
                            "File size(%llu byte) is larger than partition size(%llu byte)\n"
                            , BunchHeader.ullLength, ullPartitionSize);
                        goto error;
                    }

                    if(Write_BunchHeader(outfd, &BunchHeader)) {
                        goto error;
                    }
                    fseek(fplist, 0 , SEEK_SET);
                    fseek(outfd, 0, get_file_offset(outfd));

                    fbuf = malloc(sizeof(char) * 0x100000); /* 1MB Buffer */
                    if(fbuf == NULL){
                        FAIL_MSG("Memory allocation is failed : %s __ %d \n", __func__, __LINE__);
                        goto error;
                    }

                    remain = BYTES_TO_SECTOR(BunchHeader.ullLength) * sector_size;
                    while(remain){
                        memset(fbuf, 0, 0x100000);
                        if(remain >= 0x100000){
                            if(!fread(fbuf, 1, 0x100000, fplist)){
                                FAIL_MSG("read file : %s failed \n", plist[idx].path);
                                goto error;
                            }
                            if(!fwrite(fbuf, 1, 0x100000, outfd)){
                                FAIL_MSG("write outfile failed \n");
                                goto error;
                            }
                            remain -= 0x100000;
                        }else{
                            if(!fread(fbuf, 1, remain, fplist)){
                                FAIL_MSG("read file : %s failed \n", plist[idx].path);
                                goto error;
                            }
                            if(!fwrite(fbuf, 1, remain, outfd)){
                                FAIL_MSG("Write outfile failed \n");
                                goto error;
                            }
                            remain = 0;
                        }
                    }
                    DiskInfo.ullPartitionInfoStartOffset += get_file_offset(outfd);
                    free(fbuf);
                    fbuf = NULL;
                }
            }
            if(fplist != NULL)
            {
                fclose(fplist);
                fplist = NULL;
            }
        }
    }

// write secondary gpt entries
    BunchHeader.ullTargetAddress = (hdr->last_lba + 1) * sector_size;
    BunchHeader.ullLength = sector_size * 32/*sector size of entries*/;
    if(Write_BunchHeader(outfd, &BunchHeader)) {
        goto error;
    }

    fseek(outfd, 0, get_file_offset(outfd));
    if(Write_GPT_Entries(outfd, &ptbl)) {
        goto error;
    }

    if(gptfd != NULL) {
        if(Write_GPT_Entries(gptfd, &ptbl)) {
            goto error;
        }
    }

// write secondary gpt header
    DiskInfo.ullPartitionInfoStartOffset += get_file_offset(outfd);

    BunchHeader.ullTargetAddress = hdr->backup_lba * sector_size;
    BunchHeader.ullLength = sector_size;
    if(Write_BunchHeader(outfd, &BunchHeader)) {
        goto error;
    }

    prepare_backup_guid_header(&ptbl.guid_header);
    fill_crc32(&ptbl);

    fseek(outfd, 0, get_file_offset(outfd));

    if(Write_GPT_Header(outfd, &ptbl)) {
        goto error;
    }

    if(gptfd != NULL) {
        if(Write_GPT_Header(gptfd, &ptbl)) {
            goto error;
        }
    }

    DiskImageHeader.ulCRC32 = DISK_MakeCRC(outfd);
    fseek(outfd, 0, SEEK_SET);
    if(!fwrite(&DiskImageHeader, sizeof(char), sizeof(tagDiskImageHeader), outfd)){
        FAIL_MSG("outfile write error\n");
        goto error;
    }

    free(plist);
    return 0;
error:
    if(ptbl.mbr != NULL) {
        free(ptbl.mbr);
    }

    if(fbuf != NULL) {
        free(fbuf);
    }

    if(plist != NULL) {
        free(plist);
    }

    if(fplist != NULL) {
        fclose(fplist);
    }
    return -1;
}

int mktcimg_raw(FILE *infd, FILE *outfd, u64 storage_size, char *area_name)
{
    struct partition_list *plist;
    partition_info *pinfo;
    tagDiskImageHeader  DiskImageHeader;
    tagDiskImageBunchHeaderType BunchHeader;

    u32 pNum = 0;
    u32 idx;
    u64 remain, disksector;
    char *fbuf;

    FILE *fplist = NULL;
    pinfo = malloc(sizeof(partition_info));
    pNum = parse_file_line(infd);
    plist = parse_gpt_ptn(infd, pNum);

    disksector = 0;

    /* FWDN HEADER */
    DiskInfo.ullPartitionInfoStartOffset = 0; 
    OnCreateFsImage(&DiskImageHeader, pNum , storage_size, area_name);
    if(!fwrite(&DiskImageHeader, sizeof(char), sizeof(tagDiskImageHeader), outfd)){
        FAIL_MSG("Disk Image Header Write Failed \n");
        return -1;
    }
    DiskInfo.ullPartitionInfoStartOffset = sizeof(tagDiskImageHeader); 

    for(idx = 0; idx < PARTITION_MAX ; idx++){
        if(plist[idx].size >  0){
            if(pinfo->nPrimaryPart >= 2 && pinfo->nExtendedPart != 0){
                if(idx > (pinfo->nPrimaryPart - 2)){

                    BunchHeader.ullTargetAddress = disksector * 512;
                    BunchHeader.ullLength = sizeof(char)*512;
                    if(Write_BunchHeader(outfd, &BunchHeader)) goto error;

                }
            }

            DEBUG("idx : %d  %s \n", idx , plist[idx].name);

            if(plist[idx].path[0] == 0){
                BunchHeader.ullTargetAddress = disksector * 512;
                BunchHeader.ullLength = 0;
                if(Write_BunchHeader(outfd, &BunchHeader)) goto error;
                disksector += plist[idx].size;

            }else{

                fplist = fopen((char*)plist[idx].path, "r");

                if(fplist == NULL){
                    FAIL_MSG("file : %s can not open\n" , plist[idx].path);
                    goto error;
                }

                if(!check_sparse_image(fplist)){
                    DEBUG("sparse image is detected !!\n");
                    if(check_sparse_image_size(fplist, plist[idx].size) == -1) {
                        goto error;
                    }
                    sparse_image_write(fplist, outfd, disksector);
                    disksector += plist[idx].size;
                }else{
                    BunchHeader.ullTargetAddress = disksector * 512;
                    BunchHeader.ullLength = BYTES_TO_SECTOR(get_file_offset(fplist))*512;
                    if(Write_BunchHeader(outfd, &BunchHeader)) goto error;

                    disksector += plist[idx].size;

                    fseek(fplist, 0 , SEEK_SET);
                    fseek(outfd, 0, get_file_offset(outfd));
                    fbuf = malloc(sizeof(char) * 0x100000); /* 1MB Buffer */
                    if(fbuf == NULL){
                        FAIL_MSG("Memory allocation is failed : %s __ %d \n", __func__, __LINE__);
                        goto error;
                    }

                    remain = BYTES_TO_SECTOR(BunchHeader.ullLength)*512;

                    while(remain){

                        memset(fbuf, 0x0 , 0x100000);

                        if(remain >= 0x100000){
                            if(!fread(fbuf, 1, 0x100000, fplist)){
                                FAIL_MSG("read file : %s failed \n", plist[idx].path);
                                goto error;
                            }
                            if(!fwrite(fbuf, 1, 0x100000, outfd)){
                                FAIL_MSG("write outfile failed \n");
                                goto error;
                            }
                            remain -= 0x100000;
                        }else{
                            if(!fread(fbuf, 1, remain, fplist)){
                                FAIL_MSG("read file : %s failed \n", plist[idx].path);
                                goto error;
                            }
                            if(!fwrite(fbuf, 1, remain, outfd)){
                                FAIL_MSG("Write outfile failed \n");
                                goto error;
                            }
                            remain = 0;
                        }
                    }

                    DiskInfo.ullPartitionInfoStartOffset += get_file_offset(outfd);
                    free(fbuf);

                }
                    fclose(fplist);
            }

        }else break;
    }

    DiskImageHeader.ulCRC32 = DISK_MakeCRC(outfd);
    DEBUG ("CRC : %x \n" , DiskImageHeader.ulCRC32);
    fseek(outfd, 0, SEEK_SET);
    if(!fwrite(&DiskImageHeader, sizeof(char), sizeof(tagDiskImageHeader), outfd)){
        FAIL_MSG("outfile write error\n");
        goto error;
    }

    free(plist);
    free(pinfo);
    return 0;
error:
    if(plist != NULL) free(plist);
    if(pinfo != NULL) free(pinfo);
    if(fplist != NULL) fclose(fplist);
    return -2;
}

int split_gpt(char* gpt_name, FILE *gptfd)
{
	const int primary_sector = 34;

	char* gpt_primary_name;
	char* gpt_backup_name;

	char buf[primary_sector * sector_size];

	FILE *gpt_primary;
	FILE *gpt_backup;

	gpt_primary_name = (char*)malloc(strlen(gpt_name) + 6);
	gpt_backup_name = (char*)malloc(strlen(gpt_name) + 6);

	sprintf(gpt_primary_name, "%s.prim", gpt_name);
	sprintf(gpt_backup_name, "%s.back", gpt_name);

	gpt_primary = fopen(gpt_primary_name, "w+");
	gpt_backup = fopen(gpt_backup_name, "w+");

	fseek(gptfd, 0, SEEK_SET);

	fread(buf, 1, primary_sector * sector_size, gptfd);
	fwrite(buf, 1, primary_sector * sector_size, gpt_primary);

	fread(buf, 1, (primary_sector - 1) * sector_size, gptfd);
	fwrite(buf, 1, (primary_sector - 1) * sector_size, gpt_backup);

	fclose(gpt_primary);
	fclose(gpt_backup);

	free(gpt_primary_name);
	free(gpt_backup_name);

	return 0;
}

int make_fai(args_t *mktcimg_args)
{
    int res = 0;
    FILE *infd, *outfd, *gptfd = NULL;
    uint64_t storage_size = BYTES_TO_SECTOR(mktcimg_args->storage_size);

    infd = fopen(mktcimg_args->fplist, "r");
    if(infd == NULL) {
        FAIL_MSG("file : %s is not exist\n", mktcimg_args->fplist);
        return -1;
    }

    outfd = fopen(mktcimg_args->outfile, "w+");
    if(outfd == NULL){
        FAIL_MSG("file : %s can not create\n", mktcimg_args->outfile);
        fclose(infd);
        return -1;
    }

    if(!strcmp(mktcimg_args->parttype, "gpt")) {
		if(mktcimg_args->gptfile != NULL) {
			gptfd = fopen(mktcimg_args->gptfile, "w+");
			if(gptfd == NULL){
				FAIL_MSG("file : %s can not create\n", mktcimg_args->gptfile);
				fclose(infd);
				fclose(outfd);
				return -1;
			}
		}

		res = mktcimg_gpt (infd, outfd, gptfd, storage_size, mktcimg_args->area_name);
		if(res == 0 && gptfd != NULL) {
			res = split_gpt(mktcimg_args->gptfile, gptfd);
		}

		if(gptfd !=NULL) {
			fclose(gptfd);
		}
    } else if(!strcmp(mktcimg_args->parttype, "mbr")) {
        res = mktcimg_mbr(infd, outfd, storage_size, mktcimg_args->area_name);
    } else if(!strcmp(mktcimg_args->parttype, "raw")) {
        res = mktcimg_raw(infd, outfd, storage_size, mktcimg_args->area_name);
    } else {
        FAIL_MSG("Unkown parttype : %s\n", mktcimg_args->parttype);
        res = -1;
    }

    fclose(infd);
    fclose(outfd);

    return res;
}

void Set_Sector_Variable(args_t *mktcimg_args)
{
    u64 i;
    sector_size = mktcimg_args->sector_size;

    for(i = 0, sector_shift = sector_size; sector_shift > 1; i++) {
        sector_shift = sector_shift >> 1;
    }

    sector_shift = i;
}

int main(int argc , char **argv)
{
    int res = 0;
    args_t mktcimg_args;
    bSparseFill = 0;

    DEBUG("[mktcimg] v1.2.1 - %s %s\n", __DATE__, __TIME__);

    init_args(&mktcimg_args);

    if(parse_args(argc, argv, &mktcimg_args) == -1) {
        return -1;
    }

    if(check_args(&mktcimg_args) == -1) {
        return -1;
    }

    if(mktcimg_args.change_sector_size == 1) {
        //gpt only option
        if(strncmp(mktcimg_args.parttype, "gpt", 3) == 0) {
            Set_Sector_Variable(&mktcimg_args);
        } else {
            FAIL_MSG("\'--sector_size\' is only gpt partition's option\n");
            return -1;
        }
    }

    if(mktcimg_args.unpack != NULL) {
        if(unpack_fai(mktcimg_args.unpack ) == 0){
            SUCCESS_MSG("Complete to unpack\n");
        } else {
            FAIL_MSG("Failed to unpack\n");
            return -EFAULT;
        }
    } else {
        if(make_fai(&mktcimg_args) == 0) {
            SUCCESS_MSG("Complete to make fai file\n");
        } else {
            FAIL_MSG("Failed to make fai file\n");
			unlink(mktcimg_args.gptfile);
			unlink(mktcimg_args.outfile);
			return -EFAULT;
        }
    }

    print_args_info(&mktcimg_args);

    return res;
}
