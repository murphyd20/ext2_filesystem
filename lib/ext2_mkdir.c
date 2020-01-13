#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include "ext2.c"


int main(int argc, char *argv[]){
	// USAGE: ImageFile.img DirToBeCreated 
    int fd;
    uint8_t *data;
    struct stat sbuf;
    if (argc != 3) {
        fprintf(stderr, "USAGE: ImageFile.img DirToBeCreated \n");
        exit(1);
    }

    if ((fd = open(argv[1], O_RDWR)) == -1) {
        perror("open");
        exit(1);
    }
    if (stat(argv[1], &sbuf) == -1) {
        perror("stat");
        exit(1);
    }
	//Mmap maps the given image to an array of 8bit unsigned integers,
	//	stored in data
    data = mmap((caddr_t)0, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	int freeBlocks = getfreeInodes(data);
	int freeInodes = getfreeBlocks(data);
	short unallocatedBlocks = getUnallocatedBlocks(data);
	short unallocatedInodes = getUnallocatedInodes(data);

	//Checks to ensure there are enough Inodes
	if(freeInodes <= 0 || unallocatedInodes <= 0) {
        perror("No Free Inodes\n");
        exit(1);
    }
	//Checks to ensure there are enough Blocks
	if(freeBlocks <= 0 || unallocatedBlocks <= 0) {
        perror("No Free Blocks\n");
        exit(1);
    }
	
	//this gets the first free inode in the bitmap, this assumes that
	//	the inode count is at 16, and the first 11 are already in use
	//	and stores it in n;
	uint8_t in_bitmap= data[INODEBITMAP+1];
	int n = 0;
	while(n < 8){
		if((in_bitmap & (1 << n) ) == 0){
			n +=8;
			break;
		}
		n++;
	}
	//this gets the first free block in the bitmap, 
	int i = 0;
	int m = 0;
	int block = 1;
	while(i < 16){
		in_bitmap = data[BLOCKBITMAP+i];
		m = 0;
		while(m < 8) {
			if((in_bitmap & (1 << m) ) == 0){
				break;
			}
			m++;
		}
		block+=m;
		if(m != 8)
			break;
		i++;
	}

	//Directory to be added
	char *dirname = getDir(argv[2]);

	//Path to Directory of where the file will be stored
	char *pathName = getPathsansLastDir(argv[2]);

	//traverse the path to find where the file will be placed
	int dir_byte = traversePath(pathName,data);
	if(dir_byte == -1){
        fprintf(stderr, "Error: Directory Path Does Not Exist\n\tNo Directory Was Created\n");
		exit(1);
    }
	//current inode will keep track of the inode of each directory entry
	unsigned short curr_offset = 0;
	unsigned short next_offset = 0;
	int reclen = 0;
	int namelen = 0;

	//search each directory entry until reaching the end of the file,
	//	by taking the reclen of each entry until the last entry, which contains
	// 	the remaining bytes
	int j = 0;
	while(next_offset < 1024){
		//curr_offset tells us the current offset 
		curr_offset = next_offset;

		//next_offset tells us the next offset by looking at the current 
		//	entry's reclen and summing it with itself
		next_offset += ugetShort(data, dir_byte + 4 + curr_offset);
		
		//reclen gives us the difference, which is equal to the current entry's
		//	record length
		reclen = next_offset - curr_offset;

		//get the name length of the current directory entry and create a string
		//	containing the entry's name
		namelen = data[dir_byte + curr_offset + 6];
		char *currentName = malloc(namelen);
		j = 0;
		while( j < namelen){
			currentName[j] = data[dir_byte + curr_offset + 8 + j];
			j++;
		}

		//if the directory already has a directory entry with the exact name 
		//	given by the user, exit with 1 and print error
		if(strcmp(currentName, dirname) == 0){
		    fprintf(stderr, "Error: Directory Already Exists\n\tNo Directory Was Created\n");
			exit(1);
		}
		free(currentName);

	}
	int new_rec_len = namelen + 4 + 2 + 1 +1;
	while((new_rec_len % 4) != 0)
		new_rec_len++;

	//subtract the empty
	reclen -= new_rec_len;

	data[dir_byte + curr_offset + 4] = new_rec_len;
	data[dir_byte + curr_offset + 5] = new_rec_len>>8;

	//store the inode of the newly created directory into it's respective
	//	directory entry
	data[dir_byte + curr_offset + new_rec_len] = (n+1);
	data[dir_byte + curr_offset + new_rec_len + 1] = (n+1)>>8;
	data[dir_byte + curr_offset + new_rec_len + 2] = (n+1)>>16;
	data[dir_byte + curr_offset + new_rec_len + 3] = (n+1)>>24;

	//store the record length of the newly created directory into it's 
	//	respective directory entry
	data[dir_byte + curr_offset + new_rec_len + 4] = reclen;
	data[dir_byte + curr_offset + new_rec_len + 5] = reclen>>8;
	//store the string name length of the newly created directory into it's 
	//	respective directory entry
	data[dir_byte + curr_offset + new_rec_len + 6] = strlen(dirname);
	//store the file type(2) of the newly created directory into it's 
	//	respective directory entry
	data[dir_byte + curr_offset + new_rec_len + 7] = 2;

	//store the newly created directory's metadata in inode n
	data[n*128 + INODETABLE] = 0xc0;
	data[n*128 + INODETABLE+1] = 0x41;
	data[n*128 + INODETABLE + 5] = 4;
	data[n*128 + INODETABLE + 26] = 1;
	data[n*128 + INODETABLE + INODE_POINTEROFFSET] = block;
	data[n*128 + INODETABLE + INODE_POINTEROFFSET + 1 ] = block>>8;
	data[n*128 + INODETABLE + INODE_POINTEROFFSET + 2] = block>>16;
	data[n*128 + INODETABLE + INODE_POINTEROFFSET + 3] = block>>24;


	//Create the current and parent entries in the newly made Directory

	//store the Current directory inode in the first entry
	data[block*BLOCK_SIZE] = n+1;
	data[block*BLOCK_SIZE + 1] = (n+1)>>8;
	data[block*BLOCK_SIZE + 2] = (n+1)>>16;
	data[block*BLOCK_SIZE + 3] = (n+1)>>24;
	//the Current directory entry of size 12, so store 12 for record length
	data[block*BLOCK_SIZE + 4] = 0;
	data[block*BLOCK_SIZE + 5] = 12;
	//the Current directory name is '.', so store 1 for name length
	data[block*BLOCK_SIZE + 6] = 1;
	//the Current directory is type Directory, so store as 2
	data[block*BLOCK_SIZE + 7] = 2;
	//store the Current directory name is '.', then pad 3 bytes
	data[block*BLOCK_SIZE + 8] = '.';
	data[block*BLOCK_SIZE + 9] = 0;
	data[block*BLOCK_SIZE + 10] = 0;
	data[block*BLOCK_SIZE + 11] = 0;

	//store the Parent directory inode in the first entry
	data[block*BLOCK_SIZE + 12] = data[dir_byte];
	data[block*BLOCK_SIZE + 13] = data[dir_byte + 1];
	data[block*BLOCK_SIZE + 14] = data[dir_byte + 2];
	data[block*BLOCK_SIZE + 15] = data[dir_byte + 2];

	reclen = 1012;
	//the Parent directory stores the remain bytes, it's reclen = 1024
	data[block*BLOCK_SIZE + 16] = reclen;
	data[block*BLOCK_SIZE + 17] = reclen>>8;
	//the Parent directory name is '..', so store 2 for name length
	data[block*BLOCK_SIZE + 18] = 2;
	//the Parent directory is type directory, so store as 2
	data[block*BLOCK_SIZE + 19] = 2;

	//store the Parent directory name is '..', then pad 2 bytes
	data[block*BLOCK_SIZE + 20] = '.';
	data[block*BLOCK_SIZE + 21] = '.';
	data[block*BLOCK_SIZE + 22] = 0;
	data[block*BLOCK_SIZE + 23] = 0;

	//Update Inode BITMAP
	j = 0;
	while(j< strlen(dirname)){
		data[dir_byte + curr_offset + new_rec_len + 8 + j] = dirname[j];
		j++;	
	}
	data[INODEBITMAP+1] |= 1 << (n - 8);

	//Update Block BITMAP
	int num_blocks = 1;
	int total = 0;
	while(total < num_blocks){
		in_bitmap = data[BLOCKBITMAP+i];
		while(m < 8) {
			if(total == num_blocks)
				break;
			in_bitmap |= (1 << m);
			total++;
			m++;
		}
		m = 0;
		data[BLOCKBITMAP+i] = in_bitmap;
		if(total == num_blocks)
			break;
		i++;
	}

	//Updates free and unallocated Inode counters
	freeInodes--;
	unallocatedInodes--;
	data[BGDESCRIPTOR+14] = unallocatedInodes;
	data[SUPERBLOCK+16] = freeInodes;

	//Updates free and unallocated Block counters
	freeBlocks--;
	unallocatedBlocks--;
	data[BGDESCRIPTOR+12] = unallocatedBlocks;
	data[SUPERBLOCK+12] = freeBlocks;

	munmap(data,sbuf.st_size);
	close(fd);
    return 0;
}
