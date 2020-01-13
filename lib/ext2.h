#include <stdint.h>

#define INODE_SIZE 128
#define INODE_POINTEROFFSET 40
#define INODE_POINTER2 44
#define BLOCK_SIZE 1024

#define SUPERBLOCK 1024 
#define BGDESCRIPTOR 2048
#define BLOCKBITMAP 3072 
#define INODEBITMAP 4096 
#define INODETABLE 5120

#define ROOTINODE 5248
#define ROOTDIRBLOCK 6144


typedef struct directory_entry {
	unsigned int 	inode;
	unsigned short rec_len;
    uint8_t name_len;
	uint8_t file_type;
	char name[256];
} dir_entry;

char *getDir(char *path);
char *getPathsansLastDir(char *fullpath);
char *getName(char *root);
char *getPathsanFile(char *fullpath);
int getfreeInodes(uint8_t *data);
int getfreeBlocks(uint8_t *data);
short getUnallocatedBlocks(uint8_t *data);
short getUnallocatedInodes(uint8_t *data);
int alreadyInDirectory(uint8_t *data, int dir_byte, char* dirname);
unsigned short ugetShort(uint8_t *data, int dir_entry_byte);
unsigned int ugetint(uint8_t *data, int dir_entry_byte);
int findInodeForFileEntry(uint8_t *data, char* name, int byte_address);
int last_entry(int dir_address, uint8_t *data);
int traversePath(char *path, uint8_t *data);
