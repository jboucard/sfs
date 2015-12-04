#ifndef _FS_H_
#define _FS_H_


#define diskFilePath "/.autofs/ilab/ilab_users/jmk480/Desktop/assignment2/example/testfsfile"

#define N_BLOCKS 12
#define NAME_LEN 255
struct fs_inode {
	uint16_t  mode; //File Mode
	uint16_t  uid;  //Owner user id
	uint32_t  size; // size in bytes
	uint32_t  atime; //last access time
	uint32_t  ctime; //creation time
	uint32_t  mtime; //modification time
	uint32_t  dtime; //deletion time
	uint16_t  gid;  //low 16 bits of group id (*)
	uint16_t  links_count; //links count
	uint32_t  blocks; //blocks count
	uint32_t  flags; //file flags
	
	uint32_t  i_block[N_BLOCKS]; //pointers to blocks 
	uint32_t generation; //file version
	uint32_t file_acl; //File Access Control List
	uint32_t dir_acl; //Directory Access Control List
	uint32_t faddr;  //fragment address

	uint8_t frag; //fragment number
	uint8_t fsize; //Fragment Size
	
	// The number of the block group which contains this file's inode
	uint32_t block_group; 
	
	pthread_mutex_t lock;
	
	};

struct dir_entry{
	uint32_t inode; //inode #
	uint16_t rec_len;
	uint8_t name_len; //name length
	uint8_t file_type;
	char name[NAME_LEN]; //file name
	};


struct fs_super_block {
	uint32_t s_inodes_count; //Inodes count
	uint32_t s_blocks_count; //blocks count
	uint32_t s_r_blocks_count; //reserved blocks count
	uint32_t s_free_blocks_count; //free blocks count
	uint32_t s_free_inodes_count; //free inodes count
	uint32_t s_first_data_block; //first data block
	uint32_t s_log_block_size; //Block Size
	uint32_t s_log_frag_size; //fragment size
	uint32_t s_blocks_per_group; //# Blocks per group
	uint32_t s_frags_per_group; //# fragments per group
	uint32_t s_inodes_per_group; //# inodes per group
	uint32_t s_mtime; //mount time
	uint32_t s_wtime; //write time
	uint16_t s_mnt_count; //Mount count
	uint16_t s_state; //File system state
	uint16_t s_def_resuid; //Default uid for reserved blocks
	uint16_t s_def_resgid; //Default gid for reserved blocks
};

#endif
