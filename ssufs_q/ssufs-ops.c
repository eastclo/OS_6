#include "ssufs-ops.h"

extern struct filehandle_t file_handle_array[MAX_OPEN_FILES];

int ssufs_allocFileHandle() {
	for(int i = 0; i < MAX_OPEN_FILES; i++) {
		if (file_handle_array[i].inode_number == -1) {
			return i;
		}
	}
	return -1;
}

int ssufs_create(char *filename){
	/* 1 */
	int inodenum, blocknum;

	if(open_namei(filename) != -1) //동일한 이름의 파일이 존재하는지 확인
		return -1;

	if((inodenum = ssufs_allocInode()) < 0) //free inode가 없을 경우 -1리턴
		return -1;

	//create file(inode 구조체 초기화 후 write)
	struct inode_t *inode = (struct inode_t *)malloc(sizeof(struct inode_t));
	ssufs_readInode(inodenum, inode);
	strcpy(inode->name, filename);
	inode->status = INODE_IN_USE;
	inode->file_size = 0;
	ssufs_writeInode(inodenum, inode); //생성

	free(inode);

	return inodenum;
}

void ssufs_delete(char *filename){
	/* 2 */
	int inodenum;

	if((inodenum = open_namei(filename)) < 0) //파일이 존재하지 않으면 스킵
		return;

	ssufs_freeInode(inodenum); //inode free
	//file handle 구조체에 해당 inode 관련 정보 삭제
	for(int i = 0; i < MAX_OPEN_FILES; i++) {
		if(file_handle_array[i].inode_number == inodenum) {
			file_handle_array[i].inode_number = -1;
			file_handle_array[i].offset = 0;	
		}
	}
}

int ssufs_open(char *filename){
	/* 3 */
	int inodenum;

	if((inodenum = open_namei(filename)) < 0) //해당 파일이 없으면 에러
		return -1;

	struct inode_t *inode = (struct inode_t *)malloc(sizeof(struct inode_t));
	ssufs_readInode(inodenum, inode);

	for(int i = 0; i < MAX_OPEN_FILES; i++) {
		if(file_handle_array[i].inode_number == -1) {
			file_handle_array[i].inode_number = inodenum;
			file_handle_array[i].offset = inode->file_size; //파일의 끝 부분으로 오프셋 이동
			free(inode);
			return i;
		}
	}

	return -1; //파일 open 개수 제한
}

void ssufs_close(int file_handle){
	file_handle_array[file_handle].inode_number = -1;
	file_handle_array[file_handle].offset = 0;
}

int ssufs_read(int file_handle, char *buf, int nbytes){
	/* 4 */
	int inodenum, offset, blocknum, readed_len = 0;
	char tmp[BLOCKSIZE];
	struct inode_t *inode = (struct inode_t *)malloc(sizeof(struct inode_t));

	inodenum = file_handle_array[file_handle].inode_number;
	offset = file_handle_array[file_handle].offset;
	ssufs_readInode(inodenum, inode);	

	if(offset + nbytes > inode->file_size) //읽기 요청이 파일 끝을 넘어가면 에러
		return -1;

	while(nbytes) {
		blocknum = inode->direct_blocks[offset / BLOCKSIZE];
		ssufs_readDataBlock(blocknum, tmp);

		int	start_point = offset%BLOCKSIZE; //현재 block에서 읽기 시작할 위치
		int start_to_end_len = BLOCKSIZE - start_point; //현재 block 위치에서 읽어야할 길이

		if(nbytes < start_to_end_len) //남은 요청 bytes 처리
			start_to_end_len = nbytes;

		memcpy(buf + readed_len, tmp + start_point, start_to_end_len);

		offset += start_to_end_len;
		nbytes -= start_to_end_len;
		readed_len += start_to_end_len;	
	}

	file_handle_array[file_handle].offset = offset;
	free(inode);

	return 0;
}

int ssufs_write(int file_handle, char *buf, int nbytes){
	/* 5 */
	int inodenum, offset, blocknum, allocated_block_cnt = 0, writed_len = 0;
	char tmp[BLOCKSIZE] = {0,};
	struct inode_t *inode = (struct inode_t *)malloc(sizeof(struct inode_t));

	inodenum = file_handle_array[file_handle].inode_number;
	offset = file_handle_array[file_handle].offset;
	ssufs_readInode(inodenum, inode);	

	for(int i = 0; i < MAX_FILE_SIZE; i++) //현재 할당된 데이터블록 개수 체크
		if(inode->direct_blocks[i] != -1)
			allocated_block_cnt++;

	//쓰기요청을 위해 새로 할당해야할 데이터 블록 개수 계산 
	int to_alloc_cnt = 0;
	int left_bytes = nbytes - (BLOCKSIZE - (offset % BLOCKSIZE));
	while(left_bytes >= 0) {
		to_alloc_cnt++;
		left_bytes -= BLOCKSIZE;
	}

	if(allocated_block_cnt + to_alloc_cnt > MAX_FILE_SIZE) //최대 파일크기 제한 초과
		return -1;

	for(int i = 0; i < to_alloc_cnt; i++) { //개수만큼 할당
		if((blocknum = ssufs_allocDataBlock()) < 0) { //더이상 free블록이 없으면 에러
			for(int j = 0; j < i + 1; j++) {
				ssufs_freeDataBlock(inode->direct_blocks[allocated_block_cnt + j]); //할당한 블록 해제
				inode->direct_blocks[allocated_block_cnt + j] = -1;
			}
			free(inode);
			return -1;
		}
		inode->direct_blocks[allocated_block_cnt + i] = blocknum;
		ssufs_writeDataBlock(blocknum, tmp);
	}

	//쓰기 시작
	while(nbytes) {
		blocknum = inode->direct_blocks[offset / BLOCKSIZE];
		ssufs_readDataBlock(blocknum, tmp);

		int	start_point = offset%BLOCKSIZE; //현재 block에서 읽기 시작할 위치
		int start_to_end_len = BLOCKSIZE - start_point; //현재 block 위치에서 읽어야할 길이

		if(nbytes < start_to_end_len) //남은 요청 bytes 처리
			start_to_end_len = nbytes;

		memcpy(tmp + start_point, buf + writed_len, start_to_end_len);

		ssufs_writeDataBlock(blocknum, tmp);

		offset += start_to_end_len;
		nbytes -= start_to_end_len;
		writed_len += start_to_end_len;	
	}

	file_handle_array[file_handle].offset = offset;
	inode->file_size = offset;
	ssufs_writeInode(inodenum, inode); 
	free(inode);

	return 0;
}

int ssufs_lseek(int file_handle, int nseek){
	int offset = file_handle_array[file_handle].offset;

	struct inode_t *tmp = (struct inode_t *) malloc(sizeof(struct inode_t));
	ssufs_readInode(file_handle_array[file_handle].inode_number, tmp);

	int fsize = tmp->file_size;

	offset += nseek;

	if ((fsize == -1) || (offset < 0) || (offset > fsize)) {
		free(tmp);
		return -1;
	}

	file_handle_array[file_handle].offset = offset;
	free(tmp);

	return 0;
}
