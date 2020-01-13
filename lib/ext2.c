#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "ext2.h"

int print = 0;

//gets the name of the directory, which is the last name in the path 
char *getDir(char *path){
	int i = 0;
	int j = 0;
	while(i< strlen(path)){
		if(path[i] == '/' && i < (strlen(path) - 1))
			j=i;
		i++;
	}
	j++;
	char* nameofdir = malloc(i-j-1);
	i=0;
	while((j + i+1) < strlen(path)){
		nameofdir[i] = path[j+i];
		i++;
	}
	return nameofdir;
}

//gets the string of the path, except the last directory
char *getPathsansLastDir(char *fullpath){
	int i = 0;
	int j = 0;
	while(i< strlen(fullpath)){
		if(fullpath[i] == '/' && i < (strlen(fullpath) - 1))
			j=i;
		i++;
	}
	j++;
	i = 0;
	char* path = malloc(j);
	i=0;
	while(i < j){
		path[i] = fullpath[i];
		i++;
	}
	return path;
}

//gets the name of the file, which is the last name in the root 
char *getName(char *root){
	int i = 0;
	int j = 0;
	while(i< strlen(root)){
		if(root[i] == '/')
			j=i;
		i++;
	}
	j++;
	char* nameoffile = malloc(i-j);
	i=0;
	while((j + i) < strlen(root)){
		nameoffile[i] = root[j+i];
		i++;
	}
	return nameoffile;
}
//gets the string of the path, except the file
char *getPathsanFile(char *fullpath){
	int i = 0;
	int j = 0;
	while(i< strlen(fullpath)){
		if(fullpath[i] == '/')
			j=i;
		i++;
	}
	j++;
	char* path = malloc(j);
	i=0;
	while(i < j){
		path[i] = fullpath[i];
		i++;
	}
	return path;
}

//get the number of free Inodes from the SUPERBLOCK
int getfreeInodes(uint8_t *data){
	int freeBlocks = data[SUPERBLOCK+12];
	freeBlocks |= data[SUPERBLOCK+13] << 8;
	freeBlocks |= data[SUPERBLOCK+14] << 16;
	freeBlocks |= data[SUPERBLOCK+15] << 24;
	return freeBlocks;
}

//get the number of free Blocks from the SUPERBLOCK
int getfreeBlocks(uint8_t *data){
	int freeInodes = data[SUPERBLOCK+16];
	freeInodes |= data[SUPERBLOCK+17]<<8;
	freeInodes |= data[SUPERBLOCK+18]<<16;
	freeInodes |= data[SUPERBLOCK+19]<<24;
	return freeInodes;
}

//get the number of free Blocks from the Block Group Descriptor Table
short getUnallocatedBlocks(uint8_t *data){
	short unallocatedBlocks = data[BGDESCRIPTOR+12];
	unallocatedBlocks |= data[BGDESCRIPTOR+13] << 8;
	return unallocatedBlocks;
}

//get the number of free Inodes from the Block Group Descriptor Table
short getUnallocatedInodes(uint8_t *data){
	short unallocatedInodes = data[BGDESCRIPTOR+14];
	unallocatedInodes |= data[BGDESCRIPTOR+15] << 8;
	return unallocatedInodes;
}

//returns -1 if the string of the name already exists within the directory
int alreadyInDirectory(uint8_t *data, int dir_byte, char* dirname){
	int namelen = 0;
	int j = 0;
	int curr_offset = 0;
	int next_offset = 0;

	while(next_offset < 1024){
		curr_offset = next_offset;
		next_offset += ugetShort(data, dir_byte + 4 + curr_offset);
		namelen = data[dir_byte + curr_offset + 6];
		char *currentName = malloc(namelen);
		j = 0;
		while( j < namelen){
			currentName[j] = data[dir_byte + curr_offset + 8 + j];
			j++;
		}
		if(strncmp(currentName, dirname, namelen) == 0){
			return -1;
		}
		free(currentName);
	}
	return 0;
}


//stores 2 bytes into an SHORT
unsigned short ugetShort(uint8_t *data, int dir_entry_byte){
	unsigned short ret_short = data[dir_entry_byte];
	ret_short |= data[dir_entry_byte + 1]<<8;
	return ret_short;
}

//stores 4 bytes into an INT
unsigned int ugetint(uint8_t *data, int dir_entry_byte){
	unsigned int ret_int = data[dir_entry_byte];
	ret_int |= data[dir_entry_byte + 1]<<8;
	ret_int |= data[dir_entry_byte + 2]<<16;
	ret_int |= data[dir_entry_byte + 3]<<24;
	return ret_int;
}

//return the inode byte address of the file given the name and byte_address
int findInodeForFileEntry(uint8_t *data, char* name, int byte_address){
	int i = 24;
	while(i<1024){
		int k = i;
		dir_entry *entry = malloc(sizeof(dir_entry));
		entry->inode = data[byte_address + k];
		entry->inode |= data[byte_address + k+1]<<8;
		entry->inode |= data[byte_address + k+2]<<16;
		entry->inode |= data[byte_address + k+3]<<24;
		entry->rec_len = data[byte_address + k+4];
		entry->rec_len |= data[byte_address + k+5]<<8;
		entry->name_len = data[byte_address + k+6];
		entry->file_type = data[byte_address + k+7];
		int j = 0;
		while(j < entry->name_len){
			entry->name[j] = data[byte_address + k + j + 8];
			j++;
		}
		entry->name[j] = '\0';
		if(strcmp(entry->name,name) == 0 && entry->file_type == 1){
			int current_inode = (entry->inode - 1) * 128 + INODETABLE;
			free(entry);
			return current_inode;
		}
		i = i + entry->rec_len;
		if(i == 24){
			//dir was not found, dir is empty
			return -1;
		}
	}
	//Inode was Not Found
	return -1;

}

//returns the block number of the directory at the end of the path
int last_entry(int dir_address, uint8_t *data){
	int i = 0;
	int k = 0;
	int byte_address = dir_address;
	while(i<1024){
		k = i;
		dir_entry *entry = malloc(sizeof(dir_entry));
		entry->rec_len = data[byte_address + k+4];
		entry->rec_len |= data[byte_address + k+5]<<8;
	
		if(entry->rec_len == 0 || (entry->rec_len + i) == 1024)
			 return byte_address + i;
		i = i + entry->rec_len;
		free(entry);
	}
	return byte_address + i;
}

//returns the block number of the directory at the end of the path
int traversePath(char *path, uint8_t *data){
	int startchar = 1;
	int currchar = 0;
	int termchar = 1;
	int dirlen = 0;
	int current_inode = ROOTINODE;
	int curr_data_block;
	int i = 24;
	int j = 0;
	int byte_address;

	//go through the path, one directory at a time
	while(startchar < strlen(path) ){

		//get the next directory's name

		//while loop will find the length of the next directory's name
		while(termchar < strlen(path)){
			termchar++;
			dirlen++;

			//if the next char is '/', we have the length of the name
			//	of the next directory, so exit while loop
			if(path[termchar] == '/')
				break;
		}

		//allocate memory for the name of the next directory
		char *str_next_dir = malloc(dirlen);

		//copy each char into the next directory's name in the correct order
		while(currchar < dirlen){

			str_next_dir[currchar] = path[startchar+currchar];
			currchar++;
		}

		//append the terminating char \0 to the name
		str_next_dir[dirlen] = '\0';
		i = 24;

		//now that we know the 
		//////START TRAVERSING HERE/////////
		curr_data_block = data[current_inode+40];
		curr_data_block |= data[current_inode+41]<<8;
		curr_data_block |= data[current_inode+42]<<16;
		curr_data_block |= data[current_inode+43]<<24;

		byte_address = curr_data_block*1024;

		//iterate through the directory block
		while(i<1024){
			int k = i;

			//create a directory entry struct for each entry and store all 
			//	of the value in the directory entry struct 
			dir_entry *entry = malloc(sizeof(dir_entry));
			entry->inode = data[byte_address + k];
			entry->inode |= data[byte_address + k+1]<<8;
			entry->inode |= data[byte_address + k+2]<<16;
			entry->inode |= data[byte_address + k+3]<<24;
			entry->rec_len = data[byte_address + k+4];
			entry->rec_len |= data[byte_address + k+5]<<8;
			entry->name_len = data[byte_address + k+6];
			entry->file_type = data[byte_address + k+7];

			j = 0;
			while(j < entry->name_len){
				entry->name[j] = data[byte_address + k + j + 8];
				j++;
			}
			entry->name[j] = '\0';
			if(strcmp(entry->name,str_next_dir) == 0 && entry->file_type == 2){
				current_inode = (entry->inode - 1) * 128 + INODETABLE;
				free(entry);
				break;
			}
			i = i + entry->rec_len;
			if(i == 24){
				//dir was not found
				return -1;
			}
		}
		if(i == 1024){
			//dir was not found
			return -1;
		}
		termchar++;
		startchar = termchar;
		dirlen = 0;
		currchar = 0;
		free(str_next_dir);
	}
	//get the data block for the directory provided by the directory inode
	curr_data_block = data[current_inode+40];
	curr_data_block |= data[current_inode+41]<<8;
	curr_data_block |= data[current_inode+42]<<16;
	curr_data_block |= data[current_inode+43]<<24;
	// multiply by 1024 to get the byte address of the block for the directory
	//	at the end of the path
	byte_address = curr_data_block*1024;

	return byte_address;
}
