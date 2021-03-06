/*
 * Copyright (c) 2017, Tokyo Institute of Technology
 * Written by Tianqi Xu, xu.t.aa@m.titech.ac.jp.
 * All rights reserved. 
 * 
 * This file is part of HuronFS.
 * 
 * Please also read the file "LICENSE" included in this package for 
 * Our Notice and GNU Lesser General Public License.
 * 
 * This program is free software; you can redistribute it and/or modify it under the 
 * terms of the GNU General Public License (as published by the Free Software 
 * Foundation) version 2.1 dated February 1999. 
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU 
 * General Public License for more details. 
 * 
 * You should have received a copy of the GNU Lesser General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 59 Temple 
 * Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef MASTER_BASIC_H_
#define MASTER_BASIC_H_

#include <stdlib.h>
#include <sys/stat.h>
#include <map>
#include <set>

#include "CBB_const.h"
#include "CBB_communication_thread.h"
#include "Comm_api.h"
//#include "CBB_socket.h"
//#include "CBB_rwlocker.h"
//#include "CBB_map.h"
//#include "CBB_set.h"

namespace CBB
{
	namespace Master
	{
		class Master;

		class open_file_info;

		class Master_file_stat; 

		class node_info;

		//map start_point, data_size
		//unused
		//typedef std::map<off64_t, ssize_t> block_info_t;
		//map offset:size
		typedef std::map<off64_t, size_t> block_list_t; 
		//set node_id
		//typedef std::set<ssize_t> node_id_pool_t;
		typedef std::set<node_info*> node_info_pool_t;
		typedef std::set<Master_file_stat*> items_set_t;
		//map file_path, Master_file_stat
		typedef std::map<std::string, Master_file_stat> file_stat_pool_t;
		typedef std::set<ssize_t> file_no_pool_t;

		class node_info
		{
			public:
				//set file_no
				friend class Master;
				node_info(ssize_t id,
						const std::string& uri,
						std::size_t total_memory,
						Common::comm_handle_t handle); 
				~node_info()=default;
				ssize_t get_id()const;
				const std::string& get_uri()const;
			private:
				Common::comm_handle	handle;
				ssize_t 		node_id;
				std::string 		uri; 
				file_no_pool_t 		stored_files; 
				size_t 			avaliable_memory; 
				size_t 			total_memory;
		}; 

		class Master_file_stat
		{
			public:
				friend Master;
				Master_file_stat(const struct stat& file_stat,
						const std::string& filename,
						int exist_flag);

				Master_file_stat(const struct stat& file_stat,
						const std::string& filename,
						open_file_info* opened_file_info_p,
						int exist_flag);
				~Master_file_stat()=default;

				Master_file_stat();
				//virtual ~Master_file_stat();
				ssize_t get_fd()const;
				ssize_t get_fd();
				bool is_external()const;
				bool exist()const;
				int get_external_master();
				std::string& get_external_name();
				struct stat& get_status();
				int item_type()const;
				void set_file_full_path(file_stat_pool_t::iterator full_path_iterator);
				const struct stat& get_status()const;
				std::string& get_filename();
				const std::string& get_filename()const;
				const std::string& get_filefullpath()const;
			private:
				struct stat 	status;
				std::string 	name;
				int 		external_flag;
				int 		external_master;
				std::string 	external_name;
				int 		exist_flag;
				open_file_info* opened_file_info;
				file_stat_pool_t::iterator full_path;
		};

		class open_file_info
		{
			public:
				//map file start_point:node_id
				friend class Master_file_stat;
				friend class Master;

				open_file_info(ssize_t fileno,
						size_t block_size,
						const node_info_pool_t& IOnodes,
						int flag,
						Master_file_stat* file_stat); 
				open_file_info(ssize_t fileno,
						int flag,
						Master_file_stat* file_stat);
				~open_file_info()=default;
				//void _set_nodes_pool();
				const std::string& get_path()const;
				struct stat& get_stat();
				const struct stat& get_stat()const;
			private:
				ssize_t 		file_no;
				block_list_t 		block_list;
				node_info_pool_t 	IOnodes_set;
				node_info* 		primary_replica_node; //the node stores the main replica
				size_t 			block_size;
				int 			open_count;
				int 			flag; 
				Master_file_stat* 	file_status;
		}; 

		inline struct stat& open_file_info::get_stat()
		{
			return file_status->get_status();
		}

		inline const struct stat& open_file_info::get_stat()const
		{
			return file_status->get_status();
		}

		inline struct stat& Master_file_stat::get_status()
		{
			return status;
		}

		inline const struct stat& Master_file_stat::get_status()const
		{
			return status;
		}

		inline std::string& Master_file_stat::get_filename()
		{
			return name;
		}

		inline const std::string& Master_file_stat::get_filename()const
		{
			return name;
		}

		inline const std::string& Master_file_stat::get_filefullpath()const
		{
			return full_path->first;
		}

		inline bool Master_file_stat::is_external()const
		{
			return EXTERNAL == external_flag;
		}

		inline bool Master_file_stat::exist()const
		{
			return EXISTING == exist_flag;
		}

		inline int Master_file_stat::get_external_master()
		{
			return external_master;
		}

		inline void Master_file_stat::set_file_full_path(file_stat_pool_t::iterator full_path_iterator)
		{
			full_path = full_path_iterator;
		}

		inline std::string& Master_file_stat::get_external_name()
		{
			return external_name;
		}

		inline const std::string& open_file_info::get_path()const
		{
			return file_status->get_filefullpath();
		}

		inline ssize_t Master_file_stat::get_fd()
		{
			if(NULL != opened_file_info)
			{
				return opened_file_info->file_no;
			}
			else
			{
				return -1;
			}
		}

		inline ssize_t Master_file_stat::get_fd()const
		{
			if(NULL != opened_file_info)
			{
				return opened_file_info->file_no;
			}
			else
			{
				return -1;
			}
		}
		inline const std::string& node_info::
			get_uri()const
			{
				return this->uri;
			}
	}
}

#endif
