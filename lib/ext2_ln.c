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
	//USAGE: ImageFile.img LinkPath TargetPath
    int fd;
    uint8_t *data;
    struct stat sbuf;
    if (argc != 4) {
        fprintf(stderr, "usage: ImageFile.img LinkPath TargetPath\n");
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
        fprintf(stderr, "Error: Directory Path Does Not Exist\n\tNo Link Was Mad\n");
		exit(1);
    }

	//file_node gives the byte address for the files inode 
	int file_inode = findInodeForFileEntry(data, fileName, file_dir_byte);
	//if: file in the directory doesn't exist, or is a directory, exit with 
	//error message
	if(file_inode == -1){
        fprintf(stderr, "Error: File Does Not Exist\n\tNo Link Was Made\n");
		exit(1);
    }

	//store the link count for the file (inode)
	short link_count = data[file_inode + 26];
	link_count |= data[file_inode + 27]<<8;

	//update it's link count, but don't update yet.
	link_count++;
	
	//find directory to be changed, return Error if the path doesn't exist
	int target_dir = traversePath(argv[3],data);
	if(target_dir == -1){
        fprintf(stderr, "Error: Target Directory Path Does Not Exist\n\tNo Link Was Mad\n");
		exit(1);
    }

	
	unsigned short total_record_length = 0;
	unsigned short current_record_length = 0;

	//from the byte address of the inode, get the inode number
	unsigned int inodenum = ((file_inode - INODETABLE)/INODE_SIZE) + 1;

	//current inode will keep track of the inode of each directory entry
	unsigned int current_inode = 0;


	//search each directory entry until reaching the end of the file,
	//	by taking the reclen of each entry until the last entry, which contains
	// 	the remaining bytes
	while(total_record_length + current_record_length < 1024){
		total_record_length += current_record_length;
		current_record_length = ugetShort(data, target_dir+ 4+ total_record_length);
		current_inode =  ugetint(data, target_dir + total_record_length);

		//if the data entry contains the same inode as one as given by the 
		//	file, then the link already exits
		if(current_inode == inodenum){
    	    fprintf(stderr, "Error: Link in Target Path already exists\n\tNo Link Was Mad\n");
			exit(1);
		}
	}

	//get the len of the name of the inode which will be added
	uint8_t last_entry_name_len = data[target_dir + total_record_length + 6];

	//get the block size of the inode which will be added
	unsigned short last_entry_record_length = 4 + 2 + 1 + 1 + last_entry_name_len;
	while(last_entry_record_length%4 != 0)
		last_entry_record_length++;

	//first change the record length of the current entry to the appropriate record length
	data[target_dir + total_record_length + 4] = last_entry_record_length;
	data[target_dir + total_record_length + 5] = last_entry_record_length >> 8;

	//now update the last entry in the directory to be the linked file
	total_record_length += last_entry_record_length;
	current_record_length -= last_entry_record_length;

	//find empty entry store inode in it
	int offset = total_record_length + target_dir;


	//set the directory entry's inode on disk to the file's inode
	data[offset] = inodenum;
	data[offset + 1] = inodenum >> 8;
	data[offset + 2] = inodenum >> 16;
	data[offset + 3] = inodenum >> 24;

	//set the directory entry's record length to the remaining bytes in 
	//	the data block
	data[offset + 4] = current_record_length;
	data[offset + 5] = current_record_length >> 8;

	//set the directory entry's string length file name's length
	data[offset + 6] = strlen(fileName);

	//set the directory entry's type to 1 on disk, as it is a file
	data[offset + 7] = 1;

	//Copy the File Name into the directory entry on disk, char by char 
	int j = 0;
	while( j < strlen(fileName)){
		data[offset + 8 + j] = fileName[j];
		j++;
	}

	//ensures the decrement of the removed inode link by 1 is saved to disk,
	data[file_inode + 26] = link_count;
	data[file_inode + 27] = link_count>>8;

	munmap(data,sbuf.st_size);
	close(fd);
    return 0;
}
