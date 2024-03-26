#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "parse_args.h"
#include "gpt.h"

#ifdef WINDOWS
#include "../windows/getopt.h"
#else //LINUX
#include <getopt.h>
#endif

extern int bSparseFill;

struct option options[] = {
	{"unpack", required_argument, 0, 'u'},
	{"parttype", required_argument, 0, 'p'},
	{"storage_size", required_argument, 0, 's'},
	{"fplist", required_argument, 0, 'f'},
	{"outfile", required_argument, 0, 'o'},
	{"area_name", required_argument, 0, 'a'},
	{"gptfile", required_argument, 0, 'g'},
	{"sector_size", required_argument, 0, 'b'},
	{"sparse_fill", required_argument, 0, 'r'},
	{"partition_offset", required_argument, 0, 't'},
	{"last_part_align", no_argument, 0, 'l'},

	{"help", no_argument, 0, 'h'},
};

static char* malloc_string(const char *pString)
{
	char *ret = NULL;
	unsigned int len = 0;

	if(pString) {
		len = strlen(pString);
		ret = (char*)malloc((len * sizeof(char)) + sizeof(char));
		if (ret) {
			strcpy(ret, pString);
		}
	}
	return ret;
}

int check_long_options(int argc, char *argv[])
{
	int size = sizeof(options) / sizeof(struct option);
	int find;
	char* temp;
	int i, j;

	for(i = 0; i < argc; i++) {
		if(strncmp(argv[i], "--", 2) != 0) {
			continue;
		}

		temp = argv[i] + 2;
		find = -1;
		for(j = 0; j < size; j++) {
			if(strcmp(options[j].name, temp) == 0){
				find = 0;
				break;
			}
		}

		if(find == -1) {
			FAIL_MSG("Invaild option : %s\n", argv[i]);
			return -1;
		}
	}

	return 0;
}

int parse_args(int argc, char *argv[], args_t *mktcimg_args)
{
	int opt = -1;
	int res = 0;
	int index = 0;

	if(argc <= 1) {
		res = -1;
	}

	if(check_long_options(argc, argv) == -1) {
		return -1;
	}

	while(1) {
		opt = getopt_long(argc, argv, "b:u:p:s:f:o:a:g:t:h:r:l", options, &index);
		if(opt == -1) break; //End of Option

		switch(opt) {
		case 0:		//only long option
			break;
		case 'u':	//unpack
			mktcimg_args->unpack = malloc_string(optarg);
			break;
		case 'p':	//parttype
			mktcimg_args->parttype = malloc_string(optarg);
			break;
		case 's':	//storage_size
			mktcimg_args->storage_size = strtoull(optarg, 0, 10);
			break;
		case 'f':	//fplist
			mktcimg_args->fplist = malloc_string(optarg);
			break;
		case 'o':	//outfile
			mktcimg_args->outfile = malloc_string(optarg);
			break;
		case 'a':	//area_name
			mktcimg_args->area_name = malloc_string(optarg);
			break;
		case 'g':	//gptfile
			mktcimg_args->gptfile = malloc_string(optarg);
			break;
		case 'b':	//sector_size
			mktcimg_args->sector_size = strtoull(optarg, 0, 10);
			mktcimg_args->change_sector_size = 1;
			break;
		case 't':	//partition_offset
			mktcimg_args->partition_offset = strtoull(optarg, 0, 10);
			break;
		case 'l':	//last partition align
			mktcimg_args->last_part_align = 1;
			break;
		case 'r':	//sparse_fill
			bSparseFill = 1;
			break;
		case 'h':	//help
			ussage();
			return -1;
		default:
			res = -1;
			break;
		}
	}

	if(res == -1) {
		ussage();
	}

	return res;
}

void init_args(args_t *mktcimg_args)
{
	mktcimg_args->sector_size = 512;
	mktcimg_args->storage_size = 0;
	mktcimg_args->parttype = "gpt";
	mktcimg_args->area_name = "SD Data";
	mktcimg_args->outfile = "SD_Data.fai";
	mktcimg_args->gptfile = NULL;
	mktcimg_args->fplist = NULL;
	mktcimg_args->unpack = NULL;
	mktcimg_args->change_sector_size = 0;
	mktcimg_args->partition_offset = GUID_RESERVED;
	mktcimg_args->last_part_align = 0;
}

void print_args_info(args_t *mktcimg_args)
{
	DEBUG("\n===== arguments info =====\n\n");
	if(mktcimg_args->unpack == NULL) {
		DEBUG("--storage_size : %llu\n", mktcimg_args->storage_size);
		DEBUG("--parttype : %s\n", mktcimg_args->parttype);
		DEBUG("--area_name : \"%s\"\n", mktcimg_args->area_name);
		DEBUG("--outfile : %s\n", mktcimg_args->outfile);
		DEBUG("--gptfile : %s\n", mktcimg_args->gptfile);
		DEBUG("--fplist : %s\n", mktcimg_args->fplist);
		DEBUG("--sector_size : %d\n", mktcimg_args->sector_size);
		DEBUG("--sparse_fill : %d\n", bSparseFill);
		DEBUG("--partition_offset : %llu LBA(%llu)\n", mktcimg_args->partition_offset, (mktcimg_args->partition_offset * 512));
		DEBUG("--last_part_align : %d\n", mktcimg_args->last_part_align);
	} else {
		DEBUG("--unpack : %s\n", mktcimg_args->unpack);
	}
	DEBUG("\n===========================\n\n");
}

void ussage(void)
{
	DEBUG(
	"\033[35m======= unpack fai =======\033[0m\n"
	"Usage :\n"
	"  mktcimg --unpack [.fai]"
	"\n"
	"Example :\n"
	"  mktcimg --unpack SD_Data.fai\n"
	"\n"
	"Options :\n"
	"-u, --unpack [.fai]\n"
	"          Default value : NULL\n"
	"\n"
	"\033[35m======= make fai =======\033[0m\n"
	"Usage :\n"
	"  mktcimg --parttype [mbr | gpt | raw] --storage_size [bytes] --fplist [partition list file] --outfile [.fai] --area_name \"area map name\" --gptfile [.gpt]\n"
	"\n"
	"Example :\n"
	"  mktcimg --parttype gpt --storage_size 7818182656 --fplist gpt_partition.list --outfile SD_Data.fai --area_name \"SD Data\" --gptfile SD_Data.gpt\n"
	"  mktcimg --storage_size 7818182656 --fplist gpt_partition.list\n"
	"\n"
	"Options :\n"
	"-p, --parttype [mbr | gpt | raw]          Set partition type\n"
	"                                          Default value : gpt\n"
	"-s, --storage_size [bytes]                Set size of storage in bytes\n"
	"                                          Default value : 0\n"
	"-f, --fplist [partition list file]        List of partition\n"
	"                                          Default value : NULL\n"
	"                                          Check \"README_xxx_partition_table.txt\" for how to write the file contents\n"
	"-o, --outfile [.fai]                      \n"
	"                                          Default value : SD_Data.fai\n"
	"-a, --area_name \"area map name\"         Not use this option after tcc805x\n"
	"                                          Used to check storage type.\n"
	"                                          Default value : \"SD Data\"\n"
	"-g, --gptfile [.gpt]                      Create files with primary GPT and secondary GPT\n"
	"                                          Three files are created as shown below\n"
	"                                              .gpt : primary GPT + secondary GPT\n"
	"                                              .gpt.prim : primary GPT\n"
	"                                              .gpt.back : secondary GPT\n"
	"                                          Default value : NULL\n"
	"-b, --sector_size                         Set size of sector\n"
	"                                          Only GPT partition type is supported\n"
	"                                          Only 512 and 4096 are supported\n"
	"                                          Default value : 512 byte\n"
	"-r, --sparse_fill                         Sparse Image CHUNK_TYPE_FILL Function Disable \n"
	"                                          If the option is used, the fill in the sparse image acts as doncare.\n"
	"                                          Default value : 0(CHUNK_TYPE_FILL Function enable)\n"
	"-t, --partition_offset                    Set offset of first partition\n"
	"                                          Must be upper than GUID_RESERVED(34)\n"
	"                                          Default value : 34(GUID_RESERVED)\n"
	"-l, --last_part_align                     Force set last partition align 4096\n"
	"-h, --help                                Print help message\n"
	);
}

int check_args(args_t *mktcimg_args)
{
	int res = 0;
	if(mktcimg_args->unpack == NULL) {
		if(mktcimg_args->area_name == NULL) {
			FAIL_MSG("Please check argument of \"--area_name\"\n");
			res = -1;
		}

		if(mktcimg_args->fplist == NULL) {
			FAIL_MSG("Please check argument of \"--fplist\"\n");
			res = -1;
		}

		if(mktcimg_args->outfile == NULL) {
			FAIL_MSG("Please check argument of \"--outfile\"\n");
			res = -1;
		}

		if(mktcimg_args->parttype == NULL) {
			FAIL_MSG("Please check argument of \"--parttype\"\n");
			res = -1;
		}

		if(mktcimg_args->storage_size == 0) {
			FAIL_MSG("Please check argument of \"--storage_size\"\n");
			res = -1;
		}

		if(mktcimg_args->partition_offset < GUID_RESERVED) {
			FAIL_MSG("Please check argument of \"--partition_offset\"\n");
			FAIL_MSG("Must be upper than 34(GUID_RESERVED)\n");
			res = -1;
		}

		if(mktcimg_args->sector_size != 512
			&& mktcimg_args->sector_size != 4096) {
				FAIL_MSG("Please check argument of \"--sector_size\"\n");
				FAIL_MSG("Only 512 and 4096 are supported\n");
				res = -1;
		}
	}

	return res;
}
