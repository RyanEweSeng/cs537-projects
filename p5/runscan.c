#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "ext2_fs.h"
#include "read_ext2.h"

int main(int argc, char **argv) {
	if (argc != 3) {
		printf("expected usage: ./runscan inputfile outputfile\n");
		exit(0);
	}

    // check if the output directory exists
    if (opendir(argv[2])) {
        printf("Error: output directory already exists\n");
        exit(1);
    }

    // create output directory
    if(mkdir(argv[2], 0755) == -1) {
        printf("Error: creating directory failure\n");
    }
	
    // open disk image
	int fd;
	fd = open(argv[1], O_RDONLY);

	ext2_read_init(fd);

	struct ext2_super_block super;
	struct ext2_group_desc group;
	
	// example read first the super-block and group-descriptor
	read_super_block(fd, 0, &super);
	read_group_desc(fd, 0, &group);
	
	//printf("There are %u inodes in an inode table block and %u blocks in the idnode table\n", inodes_per_block, itable_blocks);
	
    // iterate the first inode block
	off_t start_inode_table = locate_inode_table(0, &group);
    for (unsigned int i = 0; i < 14; i++) {
        printf("inode %u: \n", i);
        
        struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
        read_inode(fd, 0, start_inode_table, i, inode);
	    
        // the maximum index of the i_block array should be computed from i_blocks / (2<<s_log_block_size)
		unsigned int i_blocks = inode->i_blocks / (2<<super.s_log_block_size);
        printf("number of blocks %u\n", i_blocks);
        printf("Is directory? %s \tIs Regular file? %s\n", S_ISDIR(inode->i_mode) ? "true" : "false", S_ISREG(inode->i_mode) ? "true" : "false");
	
        // read inode and store in a buffer
        char buf[1024];
        lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
        read(fd, buf, block_size);

        // check if file is a .jpg
        int is_jpg = 0;
        if (buf[0] == (char)0xff &&
            buf[1] == (char)0xd8 &&
            buf[2] == (char)0xff &&
            (buf[3] == (char)0xe0 || buf[3] == (char)0xe1 || buf[3] == (char)0xe8))
        {
	        is_jpg = 1;
        }

        // if it is a .jpg file, copy contents into an output file
        if (is_jpg) {
            // build output file path (i.e. output_dir/file-x.jpg) using snprintf from https://stackoverflow.com/questions/4881937/building-strings-from-variables-in-c
            int digits = (i == 0) ? 1 : log10(i) + 1;
            int filename_size = strlen(argv[2]) + strlen("/") + strlen("file-") + digits + strlen(".jpg") + 1;
            char* filename = malloc(filename_size);
            snprintf(filename, filename_size, "%s/file-%u.jpg", argv[2], i);

            int file_ptr = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);
            
            // get file size
            uint file_size = inode->i_size;

            // store jpg image data in an array for write()
            char file_data[file_size];

            // initialize the current block we are going to copy
            char curr_block[block_size];
            uint block_num = 0;
            lseek(fd, BLOCK_OFFSET(inode->i_block[block_num]), SEEK_SET);
            read(fd, curr_block, block_size);

            // iterate through all the direct blocks and copy the data
            for (uint curr_byte = 0; curr_byte < file_size && curr_byte < block_size*EXT2_N_BLOCKS; ) {
                if (block_num == EXT2_IND_BLOCK) { // single indirect block
                    // get indirect block
                    char ind_block[block_size];
                    lseek(fd, BLOCK_OFFSET(inode->i_block[block_num]), SEEK_SET);
                    read(fd, ind_block, block_size);

                    uint MAX_BLOCKS_IND = block_size * (EXT2_N_BLOCKS + 256);
                    uint ind_curr_byte;

                    // iterate through indirect block
                    for (int i = 0; i < 256; i++) {
                        for (ind_curr_byte = curr_byte; ind_curr_byte < file_size && ind_curr_byte < MAX_BLOCKS_IND; ) {
                            char curr_ind_block[block_size];
                            lseek(fd, BLOCK_OFFSET(ind_block[i]), SEEK_SET);
                            read(fd, curr_ind_block, block_size);

                            uint count = 0;
                            for (uint i = 0; i < block_size && ind_curr_byte + i < file_size; i++) {
                                file_data[ind_curr_byte + i] = curr_ind_block[i];
                                count++;
                            }
                            
                            if (ind_curr_byte + count < file_size) {
                                ind_curr_byte += count;
                                printf("ind curr byte: %u\n", ind_curr_byte);
                                continue;
                            } else {
                                ind_curr_byte += count;
                                printf("ind curr byte done: %u\n", ind_curr_byte);
                                goto done;
                            }
                        }
                    }
                    
                    done:
                        curr_byte = ind_curr_byte;   
                        block_num++;
                } else if (block_num == EXT2_DIND_BLOCK){
                   break;
                } else { // direct block
                    // keep track of where we are in the current block
                    uint count = 0;
                    // copy the block data and check if we are within the block and file_size
                    for (uint i = 0; i < block_size && curr_byte + i < file_size; i++) {
                        file_data[curr_byte + i] = curr_block[i];
                        count++;
                    }

                    // check if we reach the end of a block, then we move to the next block
                    if (curr_byte + count < file_size) {
                        block_num++;
                        lseek(fd, BLOCK_OFFSET(inode->i_block[block_num]), SEEK_SET);
                        read(fd, curr_block, block_size);
                    }

                    //printf("curr byte  \t %u\n", curr_byte);
                    //printf("count      \t %u\n", count);
                    //printf("file size  \t %u\n", file_size);
                    curr_byte += count;
                }
            }

            // write to file
            write(file_ptr, file_data, file_size);
    
            // print i_block numbers
	        for(unsigned int i=0; i<EXT2_N_BLOCKS; i++) {
                if (i < EXT2_NDIR_BLOCKS)                                 // direct blocks
                    printf("Block %2u : %u\n", i, inode->i_block[i]);
		    	else if (i == EXT2_IND_BLOCK)                             // single indirect block
		    		printf("Single   : %u\n", inode->i_block[i]);
		    	else if (i == EXT2_DIND_BLOCK)                            // double indirect block
			    	printf("Double   : %u\n", inode->i_block[i]);
			    else if (i == EXT2_TIND_BLOCK)                            // triple indirect block
				    printf("Triple   : %u\n", inode->i_block[i]);
		    }
        }
		
        free(inode);
    }
	
	close(fd);
}

