#include "common.h"

typedef struct {
	unsigned long long storage_size;
	char* parttype;
	char* area_name;
	char* outfile;
	char* gptfile;
	char* fplist;
    char* unpack;
	int sector_size;
	u64 change_sector_size;
} args_t;

int parse_args(int argc, char *argv[], args_t* mktcimg_args);
void init_args(args_t *mktcimg_args);
int check_args(args_t *mktcimg_args);
void print_args_info(args_t *mktcimg_args);
void ussage(void);