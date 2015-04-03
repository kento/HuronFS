/*OB`
 * Master.cpp
 *
 *  Created on: Aug 8, 2014
 *      Author: xtq
 */
#include <sstream>
#include <cstring>

#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include "Master.h"
#include "CBB_internal.h"
#include "Communication.h"

const char *Master::MASTER_MOUNT_POINT="CBB_MASTER_MOUNT_POINT";

Master::file_info::file_info(ssize_t fileno,
		size_t block_size,
		const node_t& IOnodes,
		int flag,
		file_stat* file_stat):
	file_no(fileno),
	p_node(IOnodes), 
	nodes(node_pool_t()),
	block_size(block_size),
	open_count(1),
	flag(flag),
	file_status(file_stat)
{}

Master::file_info::file_info(ssize_t fileno,
		int flag,
		file_stat* file_stat):
	file_no(fileno),
	p_node(), 
	nodes(node_pool_t()),
	block_size(0),
	open_count(1),
	flag(flag),
	file_status(file_stat)
{}

void Master::file_info::_set_nodes_pool()
{
	for(node_t::const_iterator it=p_node.begin();
			it != p_node.end();++it)
	{
		nodes.insert(it->second);
	}
	return;
}

Master::file_stat::file_stat(const struct stat* fstat, file_info* opened_file_info):
	fstat(*fstat),
	opened_file_info(opened_file_info),
	it(NULL)
{}

Master::file_stat::file_stat():
	fstat(),
	opened_file_info(NULL),
	it(NULL)
{}


Master::node_info::node_info(ssize_t id, const std::string& ip, size_t total_memory, int socket):
	node_id(id),
	ip(ip), 
	stored_files(file_t()), 
	avaliable_memory(total_memory), 
	total_memory(total_memory), 
	socket(socket)
{}

Master::Master()throw(std::runtime_error):
	Server(MASTER_PORT), 
	_registed_IOnodes(IOnode_t()), 
	_file_stat(file_stat_t()), 
	_buffered_files(File_t()), 
	_IOnode_socket(IOnode_sock_t()), 
	_node_number(0), 
	_file_number(0), 
	_node_id_pool(new bool[MAX_NODE_NUMBER]), 
	_file_no_pool(new bool[MAX_FILE_NUMBER]), 
	_current_node_number(0), 
	_current_file_no(0),
	_mount_point(std::string())
{
	memset(_node_id_pool, 0, MAX_NODE_NUMBER*sizeof(bool)); 
	memset(_file_no_pool, 0, MAX_FILE_NUMBER*sizeof(bool)); 
	const char *master_mount_point=getenv(MASTER_MOUNT_POINT);
	
	if(NULL == master_mount_point)
	{
		throw std::runtime_error("please set master mount point");
	}
	_mount_point=std::string(master_mount_point);
	try
	{
		_init_server(); 
	}
	catch(std::runtime_error)
	{
		throw; 
	}
}

Master::~Master()
{
	Server::stop_server(); 
	for(IOnode_t::iterator it=_registed_IOnodes.begin();
			it!=_registed_IOnodes.end();++it)
	{
		Send(it->second->socket, I_AM_SHUT_DOWN);
		close(it->second->socket);
		delete it->second;
	}
	for(File_t::iterator it=_buffered_files.begin();
			it!=_buffered_files.end();++it)
	{
		delete it->second;
	}
	delete _node_id_pool;
	delete _file_no_pool; 
}

ssize_t Master::_add_IO_node(const std::string& node_ip, std::size_t total_memory, int socket)
{
	ssize_t id=0; 
	if(-1 == (id=_get_node_id()))
	{
		return -1;
	}
	if(_registed_IOnodes.end() != _find_by_ip(node_ip))
	{
		return -1;
	}
	_registed_IOnodes.insert(std::make_pair(id, new node_info(id, node_ip, total_memory, socket)));
	node_info *node=_registed_IOnodes.at(id); 
	_IOnode_socket.insert(std::make_pair(socket, node)); 
	Server::_add_socket(socket); 
	++_node_number; 
	//int open;
	//Recv(socket, open);
	return id; 
}

ssize_t Master::_delete_IO_node(int socket)
{
	node_info *node=_IOnode_socket.at(socket);
	ssize_t id=node->node_id;
	_registed_IOnodes.erase(id);
	Server::_delete_socket(socket); 
	close(socket); 
	_IOnode_socket.erase(socket); 
	_node_id_pool[id]=false;
	--_node_number; 
	delete node;
	return id;
}

ssize_t Master::_get_node_id()
{
	if(MAX_NODE_NUMBER == _node_number)
	{
		return -1;
	}
	for(; _current_node_number<MAX_NODE_NUMBER; ++_current_node_number)
	{
		if(!_node_id_pool[_current_node_number])
		{
			_node_id_pool[_current_node_number]=true; 
			return _current_node_number; 
		}
	}
	for(_current_node_number=0;  _current_node_number<MAX_NODE_NUMBER; ++_current_node_number)
	{
		if(!_node_id_pool[_current_node_number])
		{
			_node_id_pool[_current_node_number]=true; 
			return _current_node_number;
		}
	}
	return -1;
}

void Master::_release_file_no(ssize_t file_no)
{
	--_file_number;
	_file_no_pool[file_no]=false;
	return;
}

ssize_t Master::_get_file_no()
{
	if(MAX_FILE_NUMBER == _file_number)
	{
		return -1;
	}
	for(; _current_file_no<MAX_FILE_NUMBER; ++_current_file_no)
	{
		if(!_file_no_pool[_current_file_no])
		{
			_file_no_pool[_current_file_no]=true; 
			return _current_file_no; 
		}
	}
	for(_current_file_no=0;  _current_file_no<MAX_FILE_NUMBER; ++_current_file_no)
	{
		if(!_file_no_pool[_current_file_no])
		{
			_file_no_pool[_current_file_no]=true; 
			return _current_file_no;
		}
	}
	return -1;
}

void Master::_send_node_info(int clientfd)const 
{
	_LOG("requery for IOnode info\n");
	Send(clientfd, static_cast<int>(_registed_IOnodes.size()));

	for(IOnode_t::const_iterator it=_registed_IOnodes.begin(); it!=_registed_IOnodes.end(); ++it)
	{
		const node_info *node=it->second;
		Sendv(clientfd, node->ip.c_str(), node->ip.size());
		Send(clientfd, node->total_memory);
		Send_flush(clientfd, node->avaliable_memory);
	}
	return; 
}

void Master::_send_file_info(int clientfd)const 
{
	ssize_t file_no=0;
	_LOG("requery for File info\n");
	Recv(clientfd, file_no);

	try
	{
		const file_info *const file=_buffered_files.at(file_no);
		Send(clientfd, SUCCESS);
		Sendv(clientfd, file->get_path().c_str(), file->get_path().size());
		Send(clientfd, file->get_stat().st_size);
		Send(clientfd, file->block_size);
		Send_flush(clientfd, file->flag);
	}
	catch(std::out_of_range &e)
	{
		Send_flush(clientfd, NO_SUCH_FILE);
	}
	return; 
}

Master::IOnode_t::iterator Master::_find_by_ip(const std::string &ip)
{
	IOnode_t::iterator it=_registed_IOnodes.begin();
	for(; it != _registed_IOnodes.end() && ! (it->second->ip == ip); ++it);
	return it;
}

int Master::_parse_new_request(int clientfd, const struct sockaddr_in& client_addr)
{
	int request, ans=SUCCESS;
	std::string ip(inet_ntoa(client_addr.sin_addr));
	Recv(clientfd, request);
	switch(request)
	{
	case REGIST:
		_parse_regist_IOnode(clientfd, ip);break;
	case NEW_CLIENT:
		_parse_new_client(clientfd, ip);break;
	default:
		Send(clientfd, UNRECOGNISTED); 
		close(clientfd); 
	}
	return ans;
}

int Master::_parse_regist_IOnode(int clientfd, const std::string& ip)
{
	size_t total_memory;
	_LOG("regist IOnode\nip=%s\n",ip.c_str());
	Recv(clientfd, total_memory);
	Send_flush(clientfd, _add_IO_node(ip, total_memory, clientfd));
	_DEBUG("total memory %lu\n", total_memory);
	return 1;
}

int Master::_send_file_meta(int clientfd)const
{
	char *path=NULL;
	_LOG("requery for File meta data\n");
	struct stat buff;
	Recvv(clientfd, &path);
	try
	{
		//const file_info &file=_buffered_files.at(file_no);
		Send(clientfd, SUCCESS);
		Sendv_flush(clientfd, &buff, sizeof(struct stat));
	}
	catch(std::out_of_range &e)
	{
		if(-1 == stat(path, &buff))
		{
			Send_flush(clientfd, NO_SUCH_FILE);
		}
		else
		{
			Send(clientfd, SUCCESS);
			Sendv_flush(clientfd, &buff, sizeof(struct stat));
		}
	}
	free(path);
	return SUCCESS; 
}

void Master::_send_block_info(int clientfd, const node_id_pool_t& node_pool, const node_t& node_set)const
{
	Send(clientfd, static_cast<int>(node_pool.size()));
	for(node_id_pool_t::const_iterator it=node_pool.begin();
			it!=node_pool.end();++it)
	{
		Send(clientfd, *it);
		const std::string& ip=_registed_IOnodes.at(*it)->ip;
		Sendv(clientfd, ip.c_str(), ip.size());
	}

	Send(clientfd, static_cast<int>(node_set.size()));

	for(node_t::const_iterator it=node_set.begin(); it!=node_set.end(); ++it)
	{
		Send(clientfd, it->first);
		Send(clientfd, it->second);
	}
	Send_flush(clientfd, SUCCESS);
	return; 
}

int Master::_parse_attr(int clientfd)
{
	_DEBUG("requery for File info\n");
	std::string real_path, relative_path;
	Server::_recv_real_relative_path(clientfd, real_path, relative_path);
	_DEBUG("file path=%s\n", real_path.c_str());
	try
	{
		struct stat& file_stat=_file_stat.at(relative_path).fstat;
		//_DEBUG("buffered file file_no=%ld\n", fd);
		//_get_buffered_file_attr(, &fstat);
		Send(clientfd, SUCCESS);
		Send_attr(clientfd, &file_stat);
		return SUCCESS;
	}
	catch(std::out_of_range &e)
	{
		try
		{
			file_stat* new_file_status=_create_new_file_stat(relative_path.c_str());
		//if(-1 != stat(real_path.c_str(), &fstat))
			Send(clientfd, SUCCESS);
			Send_attr(clientfd, &(new_file_status->fstat));
			//ssize_t file_no;	
			//_open_file(relative_path.c_str(), O_RDONLY|O_WRONLY, file_no);
			_DEBUG("finished\n");
			return SUCCESS;
		}
		catch(std::invalid_argument &e)
		{
			Send_flush(clientfd, errno);
			return FAILURE;
		}
	}
}

int Master::_parse_readdir(int clientfd)const
{
	_DEBUG("requery for dir info\n");
	std::string real_path=Server::_recv_real_path(clientfd);
	_DEBUG("file path=%s\n", real_path.c_str());
	DIR *dir=opendir(real_path.c_str());
	if(NULL != dir)
	{
		const struct dirent* entry=NULL;
		dir_t files;
		while(NULL != (entry=readdir(dir)))
		{
			files.push_back(std::string(entry->d_name));
		}
		Send(clientfd, SUCCESS);
		Send(clientfd, static_cast<int>(files.size()));
		for(dir_t::const_iterator it=files.begin();
				it!=files.end();++it)
		{
			Sendv(clientfd, it->c_str(), it->length());
		}
		Send_flush(clientfd, SUCCESS);
		closedir(dir);
		return SUCCESS;
	}
	else
	{
		Send_flush(clientfd, errno);
		return FAILURE;
	}
}

/*int Master::_get_buffered_file_attr(ssize_t fd, struct stat* fstat)const
{
	const file_info* file=_buffered_files.at(fd);
	memcpy(fstat, &file->fstat, sizeof(file->fstat));
	return 1;
}*/

int Master::_parse_registed_request(int clientfd)
{
	int request, ans=SUCCESS; 
	Recv(clientfd, request); 
	ssize_t id;
	switch(request)
	{
	case PRINT_NODE_INFO:
		_send_node_info(clientfd);break;
	case OPEN_FILE:
		_parse_open_file(clientfd);break; 
	case READ_FILE:
		_parse_read_file(clientfd);break;
	case WRITE_FILE:
		_parse_write_file(clientfd);break;
	case FLUSH_FILE:
		_parse_flush_file(clientfd);break;
	case CLOSE_FILE:
		_parse_close_file(clientfd);break;
	case GET_FILE_META:
		_send_file_meta(clientfd);break;
	case GET_ATTR:
		_parse_attr(clientfd);break;
	case READ_DIR:
		_parse_readdir(clientfd);break;
	case RM_DIR:
		_parse_rmdir(clientfd);break;
	case UNLINK:
		_parse_unlink(clientfd);break;
	case ACCESS:
		_parse_access(clientfd);break;
	/*case RENAME:
		_parse_rename(clientfd);break;*/
	case MKDIR:
		_parse_mkdir(clientfd);break;
	case TRUNCATE:
		_parse_truncate_file(clientfd);break;
	case CLOSE_CLIENT:
		_parse_close_client(clientfd);break;
	case I_AM_SHUT_DOWN:
		id=_IOnode_socket.at(clientfd)->node_id;
		_DEBUG("IOnode %ld shutdown\nIP Address=%s, Unregisted\n", id, _registed_IOnodes.at(id)->ip.c_str()); 
		_delete_IO_node(clientfd);break;
	default:
		Send_flush(clientfd, UNRECOGNISTED); 
	}
	return ans; 
}

int Master::_parse_new_client(int clientfd, const std::string& ip)
{
	_LOG("new client ip=%s\n", ip.c_str());
	Server::_add_socket(clientfd); 
	Send_flush(clientfd, SUCCESS);
	return SUCCESS;
}

int Master::_parse_close_client(int clientfd)
{
	_LOG("close client\n");
	Server::_delete_socket(clientfd);
	close(clientfd);
	return SUCCESS;
}
	
int Master::_parse_open_file(int clientfd)
{
	char *file_path=NULL;
	_LOG("request for open file\n");
	Recvv(clientfd, &file_path); 
	int flag ,ret=SUCCESS;
	Recv(clientfd, flag); 
	try
	{
		ssize_t file_no;
		if(flag & O_CREAT)
		{
			mode_t mode;
			Recv(clientfd, mode);
			_create_file(file_path, mode);
			flag &= ~(O_CREAT | O_TRUNC);
		}
		_open_file(file_path, flag, file_no); 
		file_info *opened_file=_buffered_files.at(file_no);
		size_t block_size=opened_file->block_size;
		delete file_path; 
		Send(clientfd, SUCCESS);
		Send(clientfd, file_no);
		Send(clientfd, block_size);
		Send_attr(clientfd, &opened_file->get_stat());
		/*Send(clientfd, static_cast<int>(nodes.size()));
		for(node_t::const_iterator it=nodes.begin(); it!=nodes.end(); ++it)
		{
			Send(clientfd, it->first);
			std::string ip=_registed_IOnodes.at(it->second).ip;
			Sendv(clientfd, ip.c_str(),ip.size());
		}*/
	}
	catch(std::runtime_error &e)
	{
		_DEBUG("%s\n",e.what());
		Send_flush(clientfd, UNKNOWN_ERROR);
		ret=FAILURE;
	}
	catch(std::invalid_argument &e)
	{
		_DEBUG("%s\n",e.what());
		Send_flush(clientfd, FILE_NOT_FOUND);
		ret=FAILURE;
	}
	catch(std::bad_alloc &e)
	{
		Send_flush(clientfd, TOO_MANY_FILES);
		ret=FAILURE;
	}
	return ret;
}

void Master::_create_file(const char* file_path, mode_t mode)throw(std::runtime_error)
{
	std::string real_path=_get_real_path(file_path);
	int fd=creat64(real_path.c_str(), mode);
	if(-1 == fd)
	{
		throw std::runtime_error("error on create file");
	}
	else
	{
		close(fd);
		return ;
	}
}

const Master::node_t& Master::_open_file(const char* file_path, int flag, ssize_t& file_no)throw(std::runtime_error, std::invalid_argument, std::bad_alloc)
{
	file_stat_t::iterator it; 
	file_info *file=NULL; 
	//file not buffered
	if(_file_stat.end() ==  (it=_file_stat.find(file_path)))
	{
		file_no=_get_file_no(); 
		if(-1 != file_no)
		{
			file_stat* file_status=_create_new_file_stat(file_path);
			file=_create_new_file_info(file_no, flag, file_status);
		}
		else
		{
			throw std::bad_alloc();
		}
	}
	//file buffered
	else
	{
		file_stat& file_status=it->second;
		if(NULL == file_status.opened_file_info)
		{
			file_no=_get_file_no(); 
			file=_create_new_file_info(file_no, flag, &file_status);
		}
		else
		{
			file=file_status.opened_file_info;
			file_no=file->file_no;
		}
		++file->open_count;
	}
	return file->p_node;
}

Master::file_info* Master::_create_new_file_info(ssize_t file_no, int flag, file_stat* stat)throw(std::invalid_argument)
{
	try
	{
		file_info *new_file=new file_info(file_no, flag, stat); 
		//_get_file_meta(*new_file);
		_buffered_files.insert(std::make_pair(file_no, new_file)).first;
		new_file->block_size=_get_block_size(stat->fstat.st_size); 
		stat->opened_file_info=new_file;
		_send_request_to_IOnodes(*new_file);
		//_file_stat.insert(std::make_pair(file_path, file_no)); 
		return new_file;
	}
	catch(std::invalid_argument &e)
	{
		//file open error
		_release_file_no(file_no);
		throw;
	}
}

Master::file_stat* Master::_create_new_file_stat(const char* relative_path)throw(std::invalid_argument)
{
	std::string relative_path_string = std::string(relative_path);
	std::string real_path=_get_real_path(relative_path_string);
	_DEBUG("file path=%s\n", real_path.c_str());
	int fd=open64(real_path.c_str(), O_RDONLY);

	if(-1 == fd)
	{
		throw std::invalid_argument("file can not open");
	}
	file_stat_t::iterator it=_file_stat.insert(std::make_pair(relative_path_string, file_stat())).first;
	file_stat* new_file_status=&(it->second);
	fstat(fd, &new_file_status->fstat);
	new_file_status->it=it;
	close(fd);
	return new_file_status;
}


/*int Master::_get_file_meta(struct file_info& file) throw(std::invalid_argument)
{
	std::string real_path=_get_real_path(file.get_path());
	_DEBUG("file path=%s\n", real_path.c_str());
	int fd=open64(real_path.c_str(), file.flag);

	if(-1 == fd)
	{
		throw std::invalid_argument("file can not open");
	}

	fstat(fd, &file.get_stat());
	close(fd);
	file.block_size=_get_block_size(file.get_stat().st_size); 
	return SUCCESS;
}*/


int Master::_parse_unlink(int clientfd)
{
	_LOG("request for unlink\n"); 
	std::string relative_path;
	std::string real_path;
	Server::_recv_real_relative_path(clientfd, real_path, relative_path);
	_LOG("path=%s\n", real_path.c_str());
	try
	{
		ssize_t fd=_file_stat.at(relative_path).get_fd();
		_remove_file(fd);
		unlink(real_path.c_str());
		Send_flush(clientfd, SUCCESS);
		return SUCCESS;
	}
	catch(std::out_of_range& e)
	{
		if(-1 != unlink(real_path.c_str()))
		{
			Send_flush(clientfd, SUCCESS);
			return SUCCESS;
		}
		else
		{
			Send_flush(clientfd, errno);
			return FAILURE;
		}
	}
}

int Master::_parse_rmdir(int clientfd)
{
	_LOG("request for rmdir\n");
	std::string file_path=Server::_recv_real_path(clientfd);
	_LOG("path=%s\n", file_path.c_str());
	if(-1 != rmdir(file_path.c_str()))
	{
		Send_flush(clientfd, SUCCESS);
		return SUCCESS;
	}
	else
	{
		Send_flush(clientfd, errno);
		return FAILURE;
	}
}

int Master::_send_request_to_IOnodes(struct file_info& file)
{
	node_block_map_t node_block_map;
	file.p_node=_select_IOnode(0, file.get_stat().st_size, file.block_size, node_block_map);
	file._set_nodes_pool();
	for(node_block_map_t::const_iterator it=node_block_map.begin(); 
			node_block_map.end()!=it; ++it)
	{
		//send read request to each IOnode
		//buffer requset, file_no, file_path, start_point, block_size
		int socket=_registed_IOnodes.at(it->first)->socket;
		Send(socket, OPEN_FILE);
		Send(socket, file.file_no); 
		Send(socket, file.flag);
		Sendv(socket, file.get_path().c_str(), file.get_path().size());
		_DEBUG("%s\n", file.get_path().c_str());
		
		Send(socket, static_cast<int>(it->second.size()));
		for(block_info_t::const_iterator blocks_it=it->second.begin();
			blocks_it!=it->second.end(); ++blocks_it)
		{
			Send(socket, blocks_it->first);
			Send(socket, blocks_it->second);
		}
		Send_flush(socket, SUCCESS);
		int ret;
		Recv(socket, ret);
	}
	return SUCCESS; 
}

inline off64_t Master::_get_block_start_point(off64_t start_point, size_t& size)const
{
	off64_t block_start_point=(start_point/BLOCK_SIZE)*BLOCK_SIZE;
	size=start_point-block_start_point+size;
	return block_start_point;
}

Master::node_t Master::_select_IOnode(off64_t start_point,
		size_t file_size,
		size_t block_size,
		node_block_map_t& node_block_map)const
{
	off64_t block_start_point=_get_block_start_point(start_point, file_size);
	//int node_number = (file_size+block_size-1)/block_size,count=(node_number+_node_number-1)/_node_number, count_node=0;
	node_t nodes;
	if(0 == _node_number)
	{
		return nodes;
	}
	int node_number = (file_size+block_size-1)/block_size,count=(node_number+_node_number-1)/_node_number, count_node=0;
	size_t remaining_size=file_size;
	if(0 == count && 0 == node_number)
	{
		count=1;
		node_number=1;
	}
	for(IOnode_t::const_iterator it=_registed_IOnodes.begin();
			_registed_IOnodes.end() != it;++it)
	{
		for(int i=0;i<count && (count_node++)<node_number;++i)
		{
			nodes.insert(std::make_pair(block_start_point, it->first));
			//node_pool.insert(it->first);
			node_block_map[it->first].insert(std::make_pair(block_start_point, MIN(remaining_size, block_size)));
			block_start_point += block_size;
			remaining_size -= block_size;
		}
	}
	return nodes; 
}

int Master::_parse_read_file(int clientfd)
{
	ssize_t file_no;
	size_t size;
	off64_t start_point;
	file_info *file=NULL; 
	_LOG("request for reading\n");
	Recv(clientfd, file_no); 
	try
	{
		file=_buffered_files.at(file_no);
		Send_flush(clientfd, SUCCESS);
	}
	catch(std::out_of_range &e)
	{
		Send_flush(clientfd, OUT_OF_RANGE);
		return -1; 
	}
	
	Recv(clientfd, start_point);
	Recv(clientfd, size);
	//_prepare_read_block(*file, start_point, size);
	node_t nodes;
	node_id_pool_t node_pool;
	/*if(file->fstat.st_size < start_point+size)
	  {
	  if(static_cast<size_t>(start_point) > file->fstat.st_size)
	  {
	  size=0;
	  }
	  else
	  {
	  size=file->fstat.st_size-start_point;
	  }
	  }
	  Send(clientfd, size);*/
	_get_IOnodes_for_IO(start_point, size, *file, nodes, node_pool);
	//_send_IO_request(file_no, *file, nodes, size, READ_FILE);
	_send_block_info(clientfd, node_pool, nodes);
	return 1;
}

/*void Master::_send_IO_request(ssize_t file_no, const file_info& file, const node_t& node_set, size_t size, int mode)const
{
	for(node_t::const_iterator it=node_set.begin(); 
			node_set.end()!=it; ++it)
	{
		//send read request to each IOnode
		//buffer requset, file_no, file_path, start_point, block_size
		int socket=_registed_IOnodes.at(it->second).socket;
		size_t my_block_size=file.block_size;//it->first> file.size?file.size-it->first;
		Send(socket, WRITE_FILE);
		Send(socket, file_no);
		Sendv(socket, file.path.c_str(), file.path.size());
		Send(socket, it->first); 
		Send(socket, my_block_size); 
	}
	int socket=_registed_IOnodes.at(node_set.begin()->second)->socket;
	Send(socket, mode);
	Send(socket, file_no);
	//total I/O size
	Send(socket, size);
	Send(socket, static_cast<int>(node_set.size()));
	for(node_t::const_iterator it=node_set.begin();
			node_set.end()!=it; ++it)
	{
		Send(socket, it->first); 
	}
	Send(socket, SUCCESS);

	return;
}*/

int Master::_allocate_one_block(const struct file_info& file)throw(std::bad_alloc)
{
	//temp solution
	int node_id=*file.nodes.begin();
	//node_info &selected_node=_registed_IOnodes.at(node_id);
	return node_id;
/*	if(selected_node.avaliable_memory >= BLOCK_SIZE)
	{
		selected_node.avaliable_memory -= BLOCK_SIZE;
		return node_id;
	}
	else
	{
		throw std::bad_alloc();
	}*/
}

void Master::_append_block(struct file_info& file, int node_id, off64_t start_point)
{
	file.p_node.insert(std::make_pair(start_point, node_id));
	file.nodes.insert(node_id);
}

void Master::_send_append_request(ssize_t file_no, const node_block_map_t& append_blocks)const
{
	for(node_block_map_t::const_iterator nodes_it=append_blocks.begin();
			nodes_it != append_blocks.end();++nodes_it)
	{
		int socket = _registed_IOnodes.at(nodes_it->first)->socket;
		int ret;
		Send(socket, APPEND_BLOCK);
		Send(socket, file_no);
		Recv(socket, ret);
		if(SUCCESS == ret)
		{
			Send(socket, static_cast<int>(nodes_it->second.size()));
			for(block_info_t::const_iterator block_it=nodes_it->second.begin();
					block_it != nodes_it->second.end();++block_it)
			{
				Send(socket, block_it->first);
				Send(socket, block_it->second);
			}
		}
		Send_flush(socket, SUCCESS);
	}
	return;
}

Master::node_t& Master::_get_IOnodes_for_IO(off64_t start_point, size_t &size, struct file_info& file, node_t& node_set, node_id_pool_t& node_id_pool)throw(std::bad_alloc)
{
	off64_t current_point=_get_block_start_point(start_point, size);
	ssize_t remaining_size=size;
	node_t::iterator it;
	node_block_map_t node_append_block;
	for(;remaining_size>0;remaining_size-=BLOCK_SIZE, current_point+=BLOCK_SIZE)
	{
		if(file.p_node.end() != (it=file.p_node.find(current_point)))
		{
			node_set.insert(std::make_pair(it->first, it->second));
			node_id_pool.insert(it->second);
		}
		else
		{
			try
			{
				int node_id=_allocate_one_block(file);
				_append_block(file, node_id, current_point);
				node_set.insert(std::make_pair(current_point, node_id));
				node_id_pool.insert(node_id);
				size_t IO_size=MIN(remaining_size, static_cast<ssize_t>(BLOCK_SIZE));
				node_append_block[node_id].insert(std::make_pair(current_point, IO_size));
			}
			catch(std::bad_alloc& e)
			{
				throw;
			}
		}
	}
	_send_append_request(file.file_no, node_append_block);
	return node_set;
}

int Master::_parse_write_file(int clientfd)
{
	_LOG("request for writing\n");
	ssize_t file_no;
	size_t size;
	off64_t start_point;
	Recv(clientfd, file_no);
	//Recv(clientfd, mode); 
	try
	{
		file_info &file=*_buffered_files.at(file_no);
		std::string real_path=_get_real_path(file.get_path());
		Send_flush(clientfd, SUCCESS);
		Recv_attr(clientfd, &file.get_stat());
		Recv(clientfd, start_point);
		Recv(clientfd, size);
		_DEBUG("real_path=%s, file_size=%ld\n", real_path.c_str(), file.get_stat().st_size);

		node_t nodes;
		node_id_pool_t node_pool;
		_get_IOnodes_for_IO(start_point, size, file, nodes, node_pool);
		_send_block_info(clientfd, node_pool, nodes);
		return SUCCESS;
	}
	catch(std::out_of_range &e)
	{
		Send_flush(clientfd, errno);	
		return FAILURE;
	}
}

int Master::_parse_access(int clientfd)const
{
	int mode;
	_LOG("request for access\n");
	std::string file_path=Server::_recv_real_path(clientfd);	
	Recv(clientfd, mode);
	_LOG("path=%s\n, mode=%d", file_path.c_str(), mode);
	if(-1 != access(file_path.c_str(), mode))
	{
		_LOG("SUCCESS\n");
		Send_flush(clientfd, SUCCESS);
		return SUCCESS;
	}
	else
	{
		Send_flush(clientfd, errno);
		return FAILURE;
	}
}

size_t Master::_get_block_size(size_t size)
{
	return BLOCK_SIZE;
	//return (size+_node_number-1)/_node_number;
}

int Master::_parse_flush_file(int clientfd)
{
	_LOG("request for writing\n");
	ssize_t file_no;
	Recv(clientfd, file_no);
	try
	{
		file_info &file=*_buffered_files.at(file_no);
		Send_flush(clientfd, SUCCESS);
		for(node_pool_t::iterator it=file.nodes.begin();
				it != file.nodes.end();++it)
		{
			_DEBUG("write back request to IOnode %ld, file_no %ld\n", *it, file_no);
		//	int socket=_IOnode_socket.at(*it)->socket;
		//	Send(socket, FLUSH_FILE);
		//	Send(socket, file_no);
		}
		return SUCCESS;
	}
	catch(std::out_of_range &e)
	{
		Send_flush(clientfd, FAILURE);
		return FAILURE;
	}
}

int Master::_remove_file(int file_no)
{
	file_info &file=*_buffered_files.at(file_no);
	for(node_pool_t::iterator it=file.nodes.begin();
			it != file.nodes.end();++it)
	{
		int socket=_registed_IOnodes.at(*it)->socket;
		Send(socket, UNLINK);
		Send_flush(socket, file_no);
	}
	_file_stat.erase(file.file_status->it);
	_buffered_files.erase(file_no);
	return SUCCESS;
}

int Master::_parse_close_file(int clientfd)
{
	_LOG("request for closing file\n");
	ssize_t file_no;
	Recv(clientfd, file_no);
	try
	{
		file_info &file=*_buffered_files.at(file_no);
		_LOG("file no %ld\n", file_no);
		Send_flush(clientfd, SUCCESS);
		//if(0 == --file.open_count)
		//{

		for(node_pool_t::iterator it=file.nodes.begin();
				it != file.nodes.end();++it)
		{
			_DEBUG("write back request to IOnode %ld\n", *it);
			int socket=_registed_IOnodes.at(*it)->socket;
			//Send(socket, FLUSH_FILE);
			//Send(socket, CLOSE_FILE);
			//Send_flush(socket, file_no);
			int ret;
			//Recv(socket, ret);
			//			int ret=0;
			//			Recv(socket, ret);
		}
		//File_t::iterator file=_buffered_files.find(file_no);
		//std::string &path=file->second.path;
		//_file_no.erase(path);
		//_buffered_files.erase(file);
		//}
		return SUCCESS;

	}
	catch(std::out_of_range &e)
	{
		Send_flush(clientfd, FAILURE);
		return FAILURE;
	}
}

//implment later
/*
int Master::_parse_rename(int clientfd)
{
	_LOG("request for rename file\n");
	std::string old_real_path, old_relative_path, new_real_path, new_relative_path;
	_recv_real_relative_path(clientfd, old_real_path, old_relative_path);
	_recv_real_relative_path(clientfd, new_real_path, new_relative_path); 
	_LOG("old file path=%s, new file path=%s\n", old_real_path.c_str(), new_real_path.c_str());
	try
	{
		file_stat& stat=_file_stat.at(old_relative_path);
		_file_stat.insert(std::make_pair(new_relative_path, stat));
		_file_stat.erase(old_relative_path);
		//_buffered_files.at(stat.get_fd())->get_path()=new_relative_path;
		Send(clientfd, SUCCESS);
		return SUCCESS;
	}
	catch(std::out_of_range &e)
	{
		int ret=rename(old_real_path.c_str(), new_real_path.c_str());
		if(-1 == ret)
		{
			Send(clientfd, errno);
		}
		else
		{
			Send(clientfd, SUCCESS);
		}
		return SUCCESS;
	}
}*/

int Master::_parse_mkdir(int clientfd)
{
	_LOG("request for mkdir\n");
	mode_t mode;
	std::string real_path=_recv_real_path(clientfd);
	Recv(clientfd, mode);
	_LOG("path=%s\n", real_path.c_str());
	if(-1 == mkdir(real_path.c_str(), mode))
	{
		Send_flush(clientfd, errno);
	}
	else
	{
		Send_flush(clientfd, SUCCESS);
	}
	return SUCCESS;
}

inline std::string Master::_get_real_path(const char* path)const
{
	return _mount_point+std::string(path);
}

inline std::string Master::_get_real_path(const std::string& path)const
{
	return _mount_point+path;
}

int Master::_parse_truncate_file(int clientfd)
{
	_LOG("request for mkdir\n");
	off64_t size;
	std::string real_path, relative_path;
	_recv_real_relative_path(clientfd, real_path, relative_path);
	Recv(clientfd, size);
	_LOG("path=%s\n", real_path.c_str());
	try
	{
		file_stat& file_status=_file_stat.at(relative_path);
		ssize_t fd=file_status.get_fd();
		file_info* file=file_status.opened_file_info;
		if(file->get_stat().st_size > size)
		{
			//send free to IOnode;
			for(off64_t block_start_point=0;
					file->get_stat().st_size > static_cast<off64_t>(block_start_point+BLOCK_SIZE);
					block_start_point+=BLOCK_SIZE)
			{
				node_t::iterator it=file->p_node.find(block_start_point);
				ssize_t node_id=it->second;
				int IOnode_socket=_registed_IOnodes[node_id]->socket;
				Send(IOnode_socket, TRUNCATE);
				Send(IOnode_socket, fd);
				Send_flush(IOnode_socket, block_start_point);
				file->p_node.erase(it);
			}
		}
		file->get_stat().st_size=size;
		Send_flush(clientfd, SUCCESS);
		return SUCCESS;
	}
	catch(std::out_of_range &e)
	{
		//ignore
		;
	}
	int ret=truncate(real_path.c_str(), size); 
	if(-1 == ret)
	{
		Send(clientfd, errno);
	}
	else
	{
		Send(clientfd, SUCCESS);
	}
	return SUCCESS;
}
