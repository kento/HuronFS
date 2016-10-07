#ifndef CBB_BASIC_H_
#define CBB_BASIC_H_

#include <map>
#include <set>
#include <string>
#include <sys/stat.h>

#include "CBB_error.h"
#include "CBB_const.h"
#include "Comm_api.h"

namespace CBB
{
	namespace Client
	{
		class block_info;
		class file_meta;
		class opened_file_info;
		class CBB_client;

		typedef std::map<ssize_t, Common::comm_handle> IOnode_fd_map_t;
		typedef std::set<int> _opened_fd_t;
		typedef std::map<std::string, file_meta*> _path_file_meta_map_t; //map path, fd

		class SCBB
		{
			public:
				friend class CBB_client;
				friend class file_meta;

				SCBB()=default;
				SCBB(int id, Common::comm_handle_t master_handle);
				~SCBB()=default;
				Common::comm_handle_t get_IOnode_fd(ssize_t IOnode_id);				
				void set_id(int id);				
				void set_master_handle(Common::comm_handle_t handle);
				Common::comm_handle_t get_master_handle();
				IOnode_fd_map_t& get_IOnode_list();
				Common::comm_handle_t insert_IOnode(ssize_t IOnode_id,
							Common::comm_handle_t IOnode_handle);
			private:
				IOnode_fd_map_t       IOnode_list; //map IOnode id: fd
				int 		      id;
				Common::comm_handle   master_handle;
		};

		class  block_info
		{
			public:
				friend class CBB_client;
				//block_info(ssize_t node_id, off64_t start_point, size_t size);
				block_info(off64_t start_point, size_t size);
			private:
				//ssize_t node_id;
				off64_t start_point;
				size_t 	size;
		};

		class file_meta
		{
			public:
				friend class CBB_client;
				friend class opened_file_info;
				file_meta(ssize_t file_no,
						size_t block_size,
						const struct stat* file_stat,
						SCBB* corresponding_SCBB);
				Common::comm_handle_t get_master_handle();
				int get_master_number();
			private:
				ssize_t 	file_no;
				int 		open_count;
				size_t 		block_size;
				struct stat 	file_stat;
				_opened_fd_t 	opened_fd;
				SCBB		*corresponding_SCBB;
				//int 		master_handle;
				_path_file_meta_map_t::iterator it;

		};

		class opened_file_info
		{
			public:
				friend class CBB_client;
				opened_file_info(int fd, int flag, file_meta* file_meta_p);
				opened_file_info();
				opened_file_info(const opened_file_info& src);
				~opened_file_info();
			private:
				const opened_file_info& operator=(const opened_file_info&)=delete;
				//current offset in file
				off64_t 	current_point;
				int 		fd;
				int 		flag;
				file_meta* 	file_meta_p;
		};

		inline void SCBB::
			set_id(int id)	
		{
			this->id=id;
		}

		inline void SCBB::
			set_master_handle(Common::comm_handle_t handle)	
		{
			copy_handle(this->master_handle, handle);
		}

		inline Common::comm_handle_t file_meta::
			get_master_handle()
		{
			return &this->corresponding_SCBB->master_handle;
		}

		inline int file_meta::
			get_master_number()
		{
			return this->corresponding_SCBB->id;
		}
		inline Common::comm_handle_t SCBB::
			insert_IOnode(ssize_t IOnode_id,
				      Common::comm_handle_t IOnode_handle)
		{
			Common::comm_handle_t ret=
				&(this->IOnode_list.insert(std::make_pair(IOnode_id, *IOnode_handle)).first->second);
			return ret;
		}

		inline IOnode_fd_map_t& SCBB::
			get_IOnode_list()
		{
			return this->IOnode_list;
		}

		inline Common::comm_handle_t SCBB::
			get_master_handle()
		{
			return &this->master_handle;
		}
	}
}
#endif
