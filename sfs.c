/*
  Simple File System

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
dwdwwdwda
dwadd
*/

#include "params.h"
#include "block.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"
#include "fs.h"
///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */

typedef struct fs_super_block super_block;
typedef struct fs_group_desc group_desc;
typedef struct dir_entry dir_entry;
typedef struct fs_inode inode;

super_block *super;
group_desc * groupdesc;
dir_entry *root;
inode * rootInode;

int DataBlockBitMap[DATABLOCKS/32+1]={ 0 };
int iNodeBitMap[INODES/32+1]={ 0 };

void *sfs_init(struct fuse_conn_info *conn)
{
    disk_open(diskFilePath);
    super= (super_block* ) malloc(sizeof(super_block));
    groupdesc= (group_desc * ) malloc(sizeof(group_desc));  
	root= (dir_entry *) malloc (sizeof(dir_entry));
    char buf[512];
    fprintf(stderr, "in bb-init\n");
    log_msg("\nsfs_init() sizeof(mode_t) %d sizeof(uint16_t) %d\n",sizeof(mode_t),sizeof(uint16_t));
	
     //write struct to the diskfile
    //read contents of block into a buffer and copy them into super block
    block_read(1, (void *) &buf); 
    memcpy((void *) super, &buf, sizeof(super_block)); 
    
    if (strcmp( super->mounted, "mount")!=0)
    {
	log_msg("\nIN IF STATEMENT\n");
	super->s_mnt_count=0;
	memmove(super->mounted, (void *) "mount", 5);
	//first time mounting to this system
	//initialize data structures accordingly
	super->s_inodes_count= INODES;
	super->s_blocks_count=DATABLOCKS; //blocks count
	super->first_d_block=108;
	super->s_free_blocks_count=DATABLOCKS; //free blocks count
	super->s_free_inodes_count=INODES; //free inodes count
	//super->s_mtime=
	groupdesc->block_bitmap=3;
	groupdesc->inode_bitmap=4;
	groupdesc->used_dirs_count=0;
/*	USE IF YOU HAVE MORE THAN ONE BLOCK GROUP
	s_blocks_per_group; //# Blocks per group
	 s_frags_per_group; //# fragments per group
	 s_inodes_per_group; //# inodes per grp */

	//initialize bitmaps to 0

	block_write(2, (void *) groupdesc);
	block_write(groupdesc->block_bitmap,  (void *)DataBlockBitMap);
	block_write(groupdesc->inode_bitmap,  (void *)iNodeBitMap);

    }
    super->s_mnt_count++;//add one to the mount count

    block_write(1, (void *) super);//write an updated initialized superblock to disk
    log_msg("MOUNT COUNT: %d\n",super->s_mnt_count);
	
    root->inode=0;//give root the first INODE
    root->file_type=DIR; //set root type to a directory
    memmove(root->name, (void *) "/", 1); //set name to "/"
	root->name_len=1;
    root->parent=0;//initialize pointer fields to 0
    root->child=0;
    root->subdirs=0;
	block_write(46, (void *) root);
    //fill out inode for root
	SetBit(iNodeBitMap,0);
	//initialize inode for root dir
	rootInode= (inode *) malloc(sizeof(inode));
	rootInode->mode =(USER_X | OTHER_W |OTHER_R | OTHER_X | USER_W | USER_R |DIR);
	rootInode->uid=geteuid();
	rootInode->size=BLOCK_SIZE;
	time_t t;
    time(&t);
	rootInode->ctime= t;
	rootInode->atime= t;
	rootInode->mtime= t;
	block_write(5, (void *) rootInode);

    log_conn(conn);
    log_fuse_context(fuse_get_context());
    return SFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void sfs_destroy(void *userdata)
{	
	free(super);
	free(rootInode);
	free(groupdesc);
	free(root);
	disk_close();
    log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int sfs_getattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);
	statbuf->st_blksize=BLOCK_SIZE; 
	statbuf->st_blocks=DATABLOCKS - super->s_free_blocks_count; 
	statbuf->st_dev=0;
	statbuf->st_rdev=0;
	statbuf->st_uid=geteuid();
	statbuf->st_gid=0;
	statbuf->st_atime=0;
	statbuf->st_mtime=0;
	statbuf->st_ctime=0;
	statbuf->st_ino=0;
	statbuf->st_nlink=0;
	statbuf->st_size=0;
	statbuf->st_mode=0;
	dir_entry * de=(dir_entry*) malloc(sizeof(dir_entry));
    if (path_to_dir_entry(path, de)==-1){
		log_msg("\nPath to dir entry failed in getattr");
		errno= -ENOENT;
		return -ENOENT;	
	}
	
	inode * node= (inode * )malloc(sizeof(inode));
	numToInode(de->inode, node);
	
    statbuf->st_ino =  de->inode;
	statbuf->st_mode=node->mode;
	statbuf->st_nlink=1;
	statbuf->st_uid=rootInode->uid;
	statbuf->st_gid=0;
	statbuf->st_rdev=0;
	statbuf->st_size=sizeof(dir_entry);//what do i return for the size if de is a folder?    
	statbuf->st_blksize=BLOCK_SIZE; 
	statbuf->st_atime=node->atime;
	statbuf->st_mtime=node->mtime;
	statbuf->st_ctime=node->ctime;
	return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	log_msg("\nsfs_create\n");
	//write an inode and dir_entry 
	int i=0;	
	int num=0;
	for (i=0; i<INODES;i++)
	{
		if(GetBit(iNodeBitMap,i)==0)
		{
			num=i;
			break;
		}
	}
	
	log_msg("\nbeginning %d\n",mode);
	SetBit(iNodeBitMap,num);
	time_t seconds;
	seconds=time(NULL);
	inode * newnode=malloc(sizeof(inode));
	newnode->mode=mode;
	newnode->uid=geteuid();
	newnode->size=0;
	newnode->atime= seconds;
	newnode->ctime= seconds;
	newnode->mtime= seconds;
	newnode->gid=0;
	newnode->links_count=1;
	newnode->blocks=0;
	newnode->flags=0;
	newnode->block_group=0;
	log_msg("\nbefore first read %d\n",mode);
	char buf[512];	
	log_msg("\nfirst read %d\n",block_read(5 +num/3, &buf));
	log_msg("\nwhich if: %d\n", num%3);
	if (num%3==0){
		memcpy((void*)buf,newnode,sizeof(inode));
		block_write(5+num/3, (void *) &buf);
	}
	if (num%3==1){
		memcpy((void*) buf + sizeof(inode),newnode,sizeof(inode));
		log_msg("\nnot the blockwrite\n");		
		block_write(5+num/3, &buf);    
	}
	if (num%3==2){
		memcpy((void*)buf + sizeof(inode)*2,newnode,sizeof(inode));
		block_write(5+num/3, &buf);    
	}
	
	
	log_msg("\nmode_t mode %d\n",mode);
    int retstat = 0;
    log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);
    
	dir_entry *new_entry=malloc(sizeof(dir_entry));
	new_entry->inode=num;
	new_entry->parent=0;
	new_entry->child=0;
	new_entry->name_len=6;
	memcpy((void*) new_entry->name, "hello/",sizeof("hello/"));
	//new_entry->name="hello/";
	block_read(47+num/2, &buf);
    if (num%2==0)
	{
		memcpy((void *) buf,new_entry,sizeof(dir_entry));
		block_write(47+num/2, &buf);
	}
	if (num%2==1)
	{
		memcpy((void *) buf+sizeof(dir_entry),new_entry,sizeof(dir_entry));
		block_write(47+num/2, &buf);
	}
	log_msg("\nend of create!\n");

    return retstat;
}


/** Remove a file */
int sfs_unlink(const char *path)
{
    int retstat = 0;
    log_msg("sfs_unlink(path=\"%s\")\n", path);

    
    return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int sfs_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);

    
    return retstat;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int sfs_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    

    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);

   
    return retstat;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int sfs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    
    
    return retstat;
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
   
    
    return retstat;
}


/** Remove a directory */
int sfs_rmdir(const char *path)
{
    int retstat = 0;
    log_msg("sfs_rmdir(path=\"%s\")\n",
	    path);
    
    
    return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int sfs_opendir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    
    
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int path_to_dir_entry(const char * path, dir_entry * dirent)
{
	dir_entry *de;
	dir_entry *copy;
	
    //example /a/b/c/
    //delete first slash(root directory) 
    //get up to & including next slash (a/)
    //find d_entry for  (a/)
    //repeat until at end of path
    int i;
    char name[NAME_LEN];
    int namelen=0;
    for (i=0; i<strlen(path); i++)
    {
		name[namelen]= path[i];
		namelen++;
		
		if (path[i]=='/'||i==strlen(path)-1)
		{	//if root dir
			if (name[0]=='/')
			{
	    		de=root;
				log_msg("\nroot->inode %d de->inode %d\n",root->inode, de->inode);
				namelen=0;
			}
			else
			{
log_msg("\nbefore while loop\n");
				//if not root then
 				dirNumToEntry (de->subdirs, copy);//fill copy with first child of de
				log_msg("\nbefore while loop after dirnumtoentry strlen: %d\n", copy->name_len);
				while (copy->name_len != namelen ||!strncmp(name, copy->name, namelen))
				{	
					//if (strlen(copy->name)<length)
					//	length=strlen(copy->name)-1;
					log_msg("\nin while loop\n");
					dirNumToEntry(copy->child, copy);
					
					if (strcmp(copy->name, de->name))
					{
						return -1;//looped through d_entries. can't find match 
					}
				}
log_msg("\nafter while loop\n");
				de=copy;
			}
			
		
		}/*
		else if (i==strlen(path)-1)
		{
			
			log_msg("path[i]: %c\n",path[i]);
			return -1;
		}*/

    } //turn path into d_entry
	dirent=de;
	return 0;
} 


int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
	
	log_msg("\nbeginning of readdir\n");
    int retstat = 0;
    dir_entry *de=(dir_entry *)malloc(sizeof (dir_entry));
	dir_entry *copy=(dir_entry *)malloc(sizeof (dir_entry));
	//turn path into d_entry
	if (path_to_dir_entry(path,de)!=0)
		return -1;
    /*de now equals the d_entry that corresponds to the passed path*/

    //loop through d_entries in path and call filler funciton on their names
	if(de->subdirs==0){
		
		return retstat;
	}
	dirNumToEntry(de->subdirs,copy);	
	while(copy->name != de->name)
	{
		if( filler(buf, copy->name, NULL, 0)==0)
			return 0;
		dirNumToEntry(copy->child, copy);
	}   	    
	free(de);
	free(copy);
    log_msg("\nend of readdir\n");
    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;

    
    return retstat;
}

struct fuse_operations sfs_oper = {
  .init = sfs_init,
  .destroy = sfs_destroy,

  .getattr = sfs_getattr,
  .create = sfs_create,
  .unlink = sfs_unlink,
  .open = sfs_open,
  .release = sfs_release,
  .read = sfs_read,
  .write = sfs_write,

  .rmdir = sfs_rmdir,
  .mkdir = sfs_mkdir,

  .opendir = sfs_opendir,
  .readdir = sfs_readdir,
  .releasedir = sfs_releasedir
};

void sfs_usage()
{
    fprintf(stderr, "usage:  sfs [FUSE and mount options] diskFile mountPoint\n");
    abort();
}
//pass a directory number, fill passed dir_entry with corresponding struct
void dirNumToEntry ( int dirNum, dir_entry * d_entry)
{
	char * buf[BLOCK_SIZE];
	if (dirNum==0)
	{
		log_msg("\ncase is root\n");
		d_entry=root;
	}
	if (dirNum%2==0)
	{
		block_read(47+(dirNum/2), buf);
		memcpy(d_entry, buf, sizeof(dir_entry));
	}
	else
	{
		block_read(47+(dirNum/2), buf);
		memcpy(d_entry, buf+sizeof(dir_entry), sizeof(dir_entry)); //double check if you get errors
	}
	log_msg("\nEnd of dirNumToEntry\n");
}
int numToInode(int num, inode * node)
{	

	char buf[512];
	if (GetBit(iNodeBitMap, num)==0)
	{
		log_msg("The inode: %d is not set",num);
		return -1;
	}

	if (block_read(5+num/3, (void *) &buf)==-1)
	{
		log_msg("\nblock_read() failed in numToInode returned -1\n");
		exit(0);
	}
	if (num%3==0){
		memcpy((void *) node,  &buf, sizeof(inode) ); 
		log_msg("\nfirst if\n");	
	}
	if (num%3==1){
		memcpy((void *) node,  &buf[sizeof(struct fs_inode)-1] , sizeof(inode));
		log_msg("\nsecond if\n");	
	}
	if (num%3==2){
		memcpy((void *) node, &buf[2*sizeof(struct fs_inode)-1], sizeof(inode));
		log_msg("\tthird if\n");	
	}		
	return 0;
	
}

  int GetBit( int A[ ],  int k )
   {
      return ( (A[k/32] & (1 << (k%32) )) != 0 ) ;     
   }
  void  SetBit( int A[ ],  int k )
   {
      A[k/32] |= 1 << (k%32);  // Set the bit at the k-th position in A[i]
   }
   void  ClearBit( int A[ ],  int k )                
   {
      A[k/32] &= ~(1 << (k%32));
   }



int main(int argc, char *argv[])
{
    int fuse_stat;
    struct sfs_state *sfs_data;
    // sanity checking on the command line
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	sfs_usage();
	
    sfs_data = malloc(sizeof(struct sfs_state));
    if (sfs_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the diskfile and save it in internal data
    sfs_data->diskfile = argv[argc-2];
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    sfs_data->logfile = log_open();
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main, %s \n", sfs_data->diskfile);
    fuse_stat = fuse_main(argc, argv, &sfs_oper, sfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
