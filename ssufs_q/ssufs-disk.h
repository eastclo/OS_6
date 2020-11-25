#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#define BLOCKSIZE 64
#define NUM_BLOCKS 35
#define NUM_DATA_BLOCKS 30
#define NUM_INODE_BLOCKS 4
#define NUM_INODES 8
#define NUM_INODES_PER_BLOCK 2
#define MAX_FILE_SIZE 4
#define MAX_FILES 8
#define MAX_OPEN_FILES 20
#define MAX_NAME_STRLEN 8
#define INODE_FREE 'x'
#define INODE_IN_USE '1'
#define DATA_BLOCK_FREE 'x'
#define DATA_BLOCK_USED '1'

struct superblock_t
{
	char name[MAX_NAME_STRLEN];
	char inode_freelist[NUM_INODES];
	char datablock_freelist[NUM_DATA_BLOCKS];
};

struct inode_t
{
	int status;
	char name[MAX_NAME_STRLEN];
	int file_size;
	int direct_blocks[MAX_FILE_SIZE];
};

struct filehandle_t
{
	int offset;
	int inode_number;
};

int open_namei(char *filename); //ssufs에서 filename을 이름으로 갖는 파일의 inodenum을 반환한다.

void ssufs_formatDisk(); //ssufs를 초기화하는 함수
int ssufs_allocInode(); //node_freelist에서 비어있는 첫 노드의 인덱스를 반환한다.
void ssufs_freeInode(int inodenum); //inodenum에 해당하는 inode를 free한다.
void ssufs_readInode(int inodenum, struct inode_t *inodeptr); //inodenum에 해당하는 inode_t의 정보를 inodeptr에 읽어온다.
void ssufs_writeInode(int inodenum, struct inode_t *inodeptr); //inodeptr이 가리키는 inode_t의 정보를 inodenum에 작성한다.
int ssufs_allocDataBlock(); //atablock_freelist에서 비어있는 첫 노드의 인덱스를 반환한다
void ssufs_freeDataBlock(int blocknum); //blocknum에 해당하는 DataBlock을 free한다.
void ssufs_readDataBlock(int blocknum, char *buf); //buf 배열에 blocknum에 해당하는 DataBlock의 데이터를 읽어온다.
void ssufs_writeDataBlock(int blocknum, char *buf); //blocknum에 해당하는 DataBlock에 buf 배열의 데이터를 작성한다.
void ssufs_dump(); //현재 ssufs의 상태를 출력한다.
