#ifndef BB_H_

#define BB_H_

#include <map>
#include <vector>
#include <netinet/in.h>

#include "include/Client.h"
#include "include/IO_const.h"

class CBB:Client
{

private:
	class  block_info
	{
		public:
		block_info(std::string ip, off_t start_point, size_t size);
		block_info(const char* ip, off_t start_point, size_t size);
		std::string ip;
		off_t start_point;
		size_t size;
	};
	
	class file_info
	{
		public:
		file_info(ssize_t file_no, int fd, size_t size, size_t block_size);
		file_info();
		ssize_t file_no;
		off_t now_point;
		int fd;
		ssize_t size;
		size_t block_size;
	};

public:
	//map fd file_info
	typedef std::map<int, file_info> _file_list_t;
	typedef std::vector<bool> _file_t;

public:
	CBB();
	~CBB();

	int _open(const char *path, int flag, mode_t mode);

	ssize_t _read(int fid, void *buffer, size_t size);

	ssize_t _write(int fid,const void *buffer, size_t size);

	int _close(int fid);
	
	void _getblock(int socket, off_t start_point, size_t size, std::vector<block_info> &block);
	
	int _get_fid();
	
	int _BB_fd_to_fid(int fd)
	{
		return fd-INIT_FD;
	}

	int _BB_fid_to_fd(int fid)
	{
		return fid+INIT_FD;
	}

private:
	int _fid_now;
	_file_list_t _file_list;
	_file_t _opened_file;
	struct sockaddr_in _master_addr;
	struct sockaddr_in _client_addr;
};


#endif