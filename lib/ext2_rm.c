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
	//USAGE: ImageFile.img FileToBeRemoved
    int fd;
    uint8_t *data;
    struct stat sbuf;
    if (argc != 3) {
        fprintf(stderr, "USAGE: ImageFile.img FileToBeRemoved\n");
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
	//sets up BITMAP to image file
    data = mmap((caddr_t)0, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	//File to be linked
	char *fileName = getName(argv[2]);

	//Path to file
	char *pathName = getPathsanFile(argv[2]);

	//starting byte of the directory containing the file, if it exists 
	int file_dir_byte =traversePath(pathName,data);

	//the path to the directory doesn't exist, thus exit with error message
	if(file_dir_byte == -1){
        fprintf(stderr, "Error: Directory Path Does Not Exist\n\tNo File was Removed\n");
		exit(1);
    }

	//file_node gives the byte address for the files inode 
	int file_inode = findInodeForFileEntry(data, fileName, file_dir_byte);

	//if: file in the directory doesn't exist, or is a directory, exit with 
	//error message
	if(file_inode == -1){
        fprintf(stderr, "Error: File Does Not Exist\n\tNo File was Removed\n");
		exit(1);
    }

	//store the link count for the file (inode)
	short link_count = data[file_inode + 26];
	link_count |= data[file_inode + 27]<<8;

	//update it's link count, but don't update yet.
	link_count--;

	//from the byte address of the inode, get the inode number
	unsigned int inodenum = ((file_inode - INODETABLE)/INODE_SIZE) + 1;

	//current inode will keep track of the inode of each directory entry
	unsigned int current_inode = 0;

	unsigned short curr_offset = 0;
	unsigned short next_offset = 0;
	unsigned short prev_offset = 0;
	int reclen = 0;

	//search each directory entry until reaching the end of the file,
	//	by taking the reclen of each entry until the last entry, which contains
	// 	the remaining bytes
	while(next_offset < 1024){
		prev_offset = curr_offset;
		current_inode =  ugetint(data, file_dir_byte + next_offset);
		curr_offset = next_offset;
		next_offset += ugetShort(data, file_dir_byte + 4 + curr_offset);
		reclen = next_offset - curr_offset;
		//if the data entry contains the same inode as one as given by the 
		//	file, then the  exits
		if(current_inode == inodenum){
    	    break;
		}
	}
	unsigned short prev_entry_record_len = ugetShort(data, file_dir_byte + 4 + prev_offset);
	//Coalese the removed entry's reclen with the previous one
	prev_entry_record_len += reclen;
	
	//first change the record length of the current entry to the appropriate record length
	data[file_dir_byte + 4 + prev_offset] = prev_entry_record_len;
	data[file_dir_byte + 5 + prev_offset] = prev_entry_record_len >> 8;

	//set the removed directory entry's inode reference to 0
	data[file_dir_byte + curr_offset] = 0;
	data[file_dir_byte + curr_offset + 1] = 0;
	data[file_dir_byte + curr_offset + 2] = 0;
	data[file_dir_byte + curr_offset + 3] = 0;

	//set the removed directory entry's record length to 0
	data[file_dir_byte + curr_offset + 4] = 0;
	data[file_dir_byte + curr_offset + 5] = 0;

	//set the removed directory entry filename's length to 0
	data[file_dir_byte + curr_offset + 6] = 0;

	//set the removed directory entry's file type to 0
	data[file_dir_byte + curr_offset + 7] = 0;

	//Remove directory entry's name to 0, char by char 
	int j = 0;
	while( j < strlen(fileName)){
		data[file_dir_byte + curr_offset + 8 + j] = 0;
		j++;
	}

	data[file_inode + 26] = link_count;
	data[file_inode + 27] = link_count>>8;
	munmap(data,sbuf.st_size);
	close(fd);
    return 0;
}
