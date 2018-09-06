/*
 NAME: Kevin Zhang, Jack Li
 EMAIL: kevin.zhang.13499@gmail.com, jackli2014@gmail.com
 ID: 104939334, 604754714 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include "ext2_fs.h"

/* Globals */
int fd;
struct ext2_super_block super_block;
uint32_t groups;
uint32_t block_size;
uint32_t blocks_count;
uint32_t inodes_count;
uint32_t inode_size;
uint32_t blocks_per_group;
uint32_t inodes_per_group;
uint32_t first_data_block;

void format_time(time_t sec, char *str)
{
    struct tm *time = gmtime(&sec);
    strftime(str, 18, "%m/%d/%y %H:%M:%S", time);
}

void directory_entries(uint32_t inode, uint32_t block)
{
    struct ext2_dir_entry dir_entry;
    uint32_t i;
    for (i = 0; i < block_size; i += dir_entry.rec_len) {
        pread(fd, &dir_entry, sizeof(dir_entry), block * block_size + i);
        if (dir_entry.inode) {
            printf("DIRENT,%u,%u,%u,%u,%u,'%s'\n",
                   inode, // parent inode number ... the I-node number of the directory that contains this entry
                   i, // logical byte offset of this entry within the directory
                   dir_entry.inode, // inode number of the referenced file
                   dir_entry.rec_len, // entry length
                   dir_entry.name_len, // name length
                   dir_entry.name // name
            );
        }
    }
}

void indirect_blocks(uint32_t n, uint32_t i_block, int level, uint32_t offset) {
    uint32_t n_blocks = block_size/sizeof(uint32_t);
    uint32_t blocks[n_blocks];
    uint32_t i;
    for (i = 0; i < n_blocks; i++)
        blocks[i] = 0;
    pread(fd, blocks, block_size, 1024 + (i_block - 1) * block_size);
    //iterate through each of the nblocks 1 by 1 where in the case of level 2 and level 3 you iterate by each level 2 and level 3 block
    for (i = 0; i < n_blocks; i++) {
	  //check if the block index was read in instead of staying 0
	  if (blocks[i]) {
		printf("INDIRECT,%u,%u,%u,%u,%u\n",
			   n,
			   level,
			   offset+i,
			   i_block,
			   blocks[i]);
		if (level == 2){
		  //recurse in level 0
		  indirect_blocks(n, blocks[i], level-1, offset);
		  //since not by passing offset in by reference doesn't actually change it for this scope, we need to manually update it 
		  //each doubly-indirect block is 256 so increase it by 256
		  offset+=256;
		}
		else if (level == 3) {
		  //same logic here except each triply-indirect block is 65536
		  indirect_blocks(n, blocks[i], level-1, offset);
		  offset+= 65536;
		}
	  }
	}
}

void inode_summary(uint32_t index, uint32_t n, uint32_t inode_table)
{
    struct ext2_inode inode;
    pread(fd, &inode, sizeof(inode), inode_table * block_size + index * sizeof(inode));
    
    uint16_t mode = inode.i_mode;
    uint16_t links_count = inode.i_links_count;
    if (mode && links_count) {
        /* Determine file type */
        char file_type = '?';
        if ((mode & 0x8000) == 0x8000)
            file_type = 'f';
        else if ((mode & 0x4000) == 0x4000)
            file_type = 'd';
        else if ((mode & 0xA000) == 0xA000)
            file_type = 's';
    
        char creation_time[18], modification_time[18], access_time[18];
        format_time(inode.i_ctime, creation_time);
        format_time(inode.i_mtime, modification_time);
        format_time(inode.i_atime, access_time);
        
        /* First twelve fields */
        printf("INODE,%u,%c,%o,%u,%u,%u,%s,%s,%s,%u,%u",
               n, // inode number
               file_type, // file type
               inode.i_mode & 0x0FFF, // mode (low order 12-bits)
               inode.i_uid, // owner
               inode.i_gid, // group
               inode.i_links_count, // link count
               creation_time, // creation time
               modification_time, // modification time
               access_time, // access time
               inode.i_size, // file size,
               inode.i_blocks // number of (512 byte) blocks of disk space taken up by this file
               );
        
        /* Next fifteen fields, if need be */
        if (file_type == 'f' || file_type == 'd' || (file_type == 's' && inode.i_size > 60)) {
            int i;
            for (i = 0; i < EXT2_N_BLOCKS; i++)
                printf(",%u",inode.i_block[i]);
        }
        printf("\n");
        
        /* directory entries */
        int i;
        for (i = 0; i < EXT2_NDIR_BLOCKS; i++) {
            if (inode.i_block[i] && file_type == 'd')
                directory_entries(n, inode.i_block[i]);
        }
	    /* indirect block references */
	    //indirect level 1 blocks are from 13 to 268
	    //indirect level 2(double linked) blocks are from 269 to 268+65536 = 65804
	    //indirect level 3 is all of the rest besides ones with 0s
        if (inode.i_block[EXT2_IND_BLOCK])
	        indirect_blocks(n, inode.i_block[EXT2_IND_BLOCK], 1, 12);
	    if (inode.i_block[EXT2_DIND_BLOCK])
	        indirect_blocks(n, inode.i_block[EXT2_DIND_BLOCK], 2, 268);
	    if (inode.i_block[EXT2_TIND_BLOCK])
	        indirect_blocks(n, inode.i_block[EXT2_TIND_BLOCK], 3, 65804);
    }
}

void free_block_entries(uint32_t group, uint32_t block_bitmap)
{
    char bytes[block_size];
    pread(fd, bytes, block_size, block_bitmap * block_size);
    uint32_t index = first_data_block + group * blocks_per_group;
    
    uint32_t i;
    for (i = 0; i < block_size; i++) {
        char c = bytes[i];
        uint32_t j;
        for (j = 0; j < 8; j++) {
            if (!(c & 1))
                printf("BFREE,%u\n", index);
            index++;
            c >>= 1;
        }
    }
}

void free_inode_entries(uint32_t group, uint32_t inode_bitmap, uint32_t inode_table)
{
    uint32_t size = inodes_per_group / 8;
    char bytes[size];
    pread(fd, bytes, size, inode_bitmap * block_size);
    uint32_t first_inode_block = group * inodes_per_group + 1;
    uint32_t curr_inode_block = first_inode_block;
    
    uint32_t i;
    for (i = 0; i < size; i++) {
        char c = bytes[i];
        uint32_t j;
        for (j = 0; j < 8; j++) {
            if (!(c & 1)) // free
                printf("IFREE,%u\n", curr_inode_block);
            else // allocated print summary for it
                inode_summary(curr_inode_block - first_inode_block, curr_inode_block, inode_table);
            curr_inode_block++;
            c >>= 1;
        }
    }
}

void group_summary(uint32_t group)
{
    struct ext2_group_desc group_desc;
    pread(fd, &group_desc, sizeof(group_desc), 1024 + block_size + group * sizeof(group_desc));
    
    /*å
    uint32_t blocks = super_block.s_blocks_per_group;
    uint32_t remainder_blocks = super_block.s_blocks_count % blocks;
    if (group == groups - 1 && (remainder_blocks))
        blocks = remainder_blocks;
    
    uint32_t inodes = super_block.s_inodes_per_group;
    uint32_t remainder_inodes = super_block.s_inodes_count % inodes;
    if (group == groups - 1 && (remainder_inodes))
        inodes = remainder_inodes;
     */
    uint32_t blocks = ((group != groups - 1) || !(blocks_count % blocks_per_group)) ? blocks_per_group : blocks_count % blocks_per_group;
    uint32_t inodes = ((group != groups - 1) || !(inodes_count % inodes_per_group)) ? inodes_per_group : inodes_count % inodes_per_group;
    
    printf("GROUP,%u,%u,%u,%u,%u,%u,%u,%u\n",
           group, // group number
           blocks, // total number of blocks in this group
           inodes, // total number of i-nodes in this group
           group_desc.bg_free_blocks_count, // number of free blocks
           group_desc.bg_free_inodes_count, // number of free inodes
           group_desc.bg_block_bitmap, // block number of free block bitmap for this group
           group_desc.bg_inode_bitmap, // block number of free i-node bitmap for this group
           group_desc.bg_inode_table); // block number of first block of i-nodes in this group
    
    free_block_entries(group, group_desc.bg_block_bitmap); // free block entries
    free_inode_entries(group, group_desc.bg_inode_bitmap, group_desc.bg_inode_table); // free I-node entries
}

int main(int argc, char **argv)
{
    /* Pre-processing */
    if (argc != 2) {
        fprintf(stderr, "Incorrect number of arguments.\n");
        exit(1);
    }
    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Invalid file system.\n");
        exit(1);
    }
    /* superblock summary */
    pread(fd, &super_block, sizeof(super_block), 1024);
    block_size = EXT2_MIN_BLOCK_SIZE << super_block.s_log_block_size;
    blocks_count = super_block.s_blocks_count;
    inodes_count = super_block.s_inodes_count;
    inode_size = super_block.s_inode_size;
    blocks_per_group = super_block.s_blocks_per_group;
    inodes_per_group = super_block.s_inodes_per_group;
    first_data_block = super_block.s_first_data_block;
    printf("SUPERBLOCK,%u,%u,%u,%u,%u,%u,%u\n",
           blocks_count, // total number of blocks
           inodes_count, // total number of i-nodes
           block_size, // block size
           inode_size, // i-node size
           blocks_per_group, // blocks per group
           inodes_per_group, // i-nodes per group
           super_block.s_first_ino); // first non-reserved i-node
    
    groups = ceil((double) blocks_count/blocks_per_group); // round up

    /* group summary (everything else) */
    uint32_t i;
    for (i = 0; i < groups; i++)
        group_summary(i);
    
    exit(0);
}
