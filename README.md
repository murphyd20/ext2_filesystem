# ext2_filesystem
An implementation of the ext2 file system, as well as tools to modify ext2-format virtual disks.

ext2_cp does copy a file into the path on disk specified, however files needing single indirection for data blocks was left uncoded; thus file's larger than size 12 KB will lead to catastrophic computational results. Also all the file's copied onto disk have limited Metadata in their corresponding Inode.
	>>ext2_cp OneFile.img hs.txt /

ext2_mkdir makes a new directory as specified, the only recommendation I would have is to avoid directory names of extermely large size, as each directory only has 1 block of 1024 Bytes. Also the  argument with a directory must end in /.

ext2_ln will add the inode of the file given into the data block of the directory as well is increment by 1 the link count for the file inode. Again be wary that each directory is only 1024 bytes, so naming conventions might be an issue, as well as the  argument with a directory must end in /.Also be wary that to link files in the root directory must have the '/' before the filename.

ext2_rm will remove entry containing the inode of the file given into the data block of the directory as well is decrement by 1 the link count for the file inode. Again be wary that each directory is only 1024 bytes, so naming conventions might be an issue, as well as the argument with a directory must end in /.
