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

int main(int argc, char *argv[])
{
	// USAGE: traversePath ImageFile.img FileName Path  
    int fd;
    uint8_t *data;
    struct stat sbuf;
    if (argc != 4) {
        fprintf(stderr, "usage: traversePath ImageFile.img FileName Path\n");
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
    data = mmap((caddr_t)0, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	// get the free blocks and inodes in the SuperBlock 
	int freeBlocks = getfreeInodes(data);
	int freeInodes = getfreeBlocks(data);
	// get the free blocks and inodes in the Block Group Descriptor Table
	short unallocatedBlocks = getUnallocatedBlocks(data);
	short unallocatedInodes = getUnallocatedInodes(data);

	if(freeInodes <= 0 || unallocatedInodes <= 0) {
        perror("No Free Inodes\n");
        exit(1);
    }
	if(freeBlocks <= 0 || unallocatedBlocks <= 0) {
        perror("No Free Blocks\n");
        exit(1);
    }
		

	struct stat file;
	stat(argv[2], &file);

	uint8_t in_bitmap= data[INODEBITMAP+1];
	int n = 0;
	while(n < 8){
		if((in_bitmap & (1 << n) ) == 0){
			break;
		}
		n++;
	}

	data[INODEBITMAP+1] = in_bitmap;

	int i = 0;
	int m = 0;
	int block = 0;
	while(i < 16){
		//printf("in_bitmap = %d \n",in_bitmap);
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
	data[BLOCKBITMAP+i] = in_bitmap;
	int num_blocks = 1 + file.st_size / 1024;
	n +=9;
	int current_inode = INODETABLE + (n-1) * INODE_SIZE;
	unsigned int current_block = (block + 1)*1024;
	
	//traverse the path to find where the file will be placed
	int dir_byte =traversePath(argv[3],data);
	if(dir_byte == -1){
        fprintf(stderr, "Error: Directory Path Does Not Exist\n\tNo File Was Written\n");
		exit(1);
    }

	//get reclen of last entry, which is the size of the remaining bytes 

	//d_e_b gets the last entry of the directory given
	int dir_entry_byte = last_entry(dir_byte, data);
	if(alreadyInDirectory(data, dir_byte, argv[2]) == -1){
        fprintf(stderr, "Error: File Already Exists\n\tNo File Was Written\n");
		exit(1);
    }


	//t_r_l gets the reclen of the last entry in the directory 
	short temp_rec_len = data[dir_entry_byte + 4];
	temp_rec_len |= data[dir_entry_byte + 5]<<8;

	//t_s_l gets the file name length
	uint8_t temp_str_len = data[dir_entry_byte + 6];
	short new_rec_len = sizeof(short) + sizeof(int) + 1 + 1 + temp_str_len;


	while((new_rec_len % 4) != 0)
		new_rec_len++;

	
	temp_rec_len = temp_rec_len - new_rec_len;
	//set old value to new value
	data[dir_entry_byte + 4] = new_rec_len;
	data[dir_entry_byte + 5] = new_rec_len>>8;
	//shift address by newvalue
	dir_entry_byte += new_rec_len;

	data[dir_entry_byte] = n;
	data[dir_entry_byte + 1] = n >> 8;
	data[dir_entry_byte + 2] = n >> 16;
	data[dir_entry_byte + 3] = n >> 24;

	data[dir_entry_byte + 4] = temp_rec_len;
	data[dir_entry_byte + 5] = temp_rec_len >> 8;
	data[dir_entry_byte + 6] = strlen(argv[2]);
	data[dir_entry_byte + 7] = 1;
	
	int j = 0;
	while( j < strlen(argv[2])){
		data[dir_entry_byte + 8 + j] = argv[2][j];
		j++;
	}

	//create the inode for the file
	data[current_inode + 4] = file.st_size;
	data[current_inode + 5] = file.st_size>>8;
	data[current_inode + 26] = 1;

	int pointer = 0;
	FILE *fp;
	fp = fopen(argv[2],"r");
	j = 0;
	char c;
	current_block = current_block/1024;

	//loop until the number of pointers in the inode to data blocks equal the 
	//	number of data blocks required to copy all of the file's data
	while(pointer < (num_blocks*4)){
		//	Write block number to pointer
		data[current_inode + INODE_POINTEROFFSET + pointer] = current_block;
		data[current_inode + INODE_POINTEROFFSET + 1 + pointer] |= current_block>>8;
		data[current_inode + INODE_POINTEROFFSET + 2 + pointer] |= current_block>>16;
		data[current_inode + INODE_POINTEROFFSET + 3 + pointer] |= current_block>>24;

		//	Write to BLOCK
		while(j < file.st_size){
			fread(&c, 1 , 1 , fp );
			j++;
			data[(current_block*1024) + j] = c;
		}
		pointer += 4;
		current_block++;
	}



	//FINALLY UPDATE BITMAPS

	//Flip the bit in inode Bitmap that is now in use 
	data[INODEBITMAP+1] |= 1 << (n - 9);

	//decrement free Inode by 1
	freeInodes--;
	unallocatedInodes--;
	data[BGDESCRIPTOR+14] = unallocatedInodes;
	data[SUPERBLOCK+16] = freeInodes;

	//decrement free Blocks by the number of data blocks that store the
	//files bytes
	freeBlocks -= num_blocks;
	unallocatedBlocks -= num_blocks;
	data[BGDESCRIPTOR+12] = unallocatedBlocks;
	data[SUPERBLOCK+12] = freeBlocks;

	//Go through the Block bitmap and flip each bit corresponding to the 
	//	block in use
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

	munmap(data,sbuf.st_size);
	fclose(fp);
	close(fd);
    return 0;
}
