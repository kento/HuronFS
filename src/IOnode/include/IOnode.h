/*
 * IOnode.h
 *
 *  Created on: Aug 8, 2014
 *      Author: xtq
 *      this file define I/O node class in I/O burst buffer
 */

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


#ifndef IONODE_H_
#define IONODE_H_

#include <string>
#include <sys/types.h>
#include <stdexcept>
#include <exception>
#include <stdlib.h>
#include <map>

#include "Server.h"
#include "CBB_data_sync.h"
#include "Comm_api.h"
#include "CBB_error.h"

namespace CBB
{
	namespace IOnode
	{
		class IOnode:
			public 	Common::Server,
			public	Common::CBB_data_sync
		{
			//API
			public:
				IOnode(const std::string& 	master_ip,
				       int 			master_port)
				       throw(std::runtime_error);
				virtual ~IOnode();
				int start_server();

				typedef std::map<ssize_t, Common::comm_handle> node_handle_pool_t; //map node id: handle
				typedef std::map<ssize_t, Common::comm_handle_t> handle_ptr_pool_t; //map node id: handle
				typedef std::map<ssize_t, std::string> node_ip_t; //map node id: ip

				//nested class
			private:

				struct file;
				struct block
				{
					block(off64_t 	start_point,
					      size_t 	size,
					      bool 	dirty_flag,
					      bool 	valid,
					      file* 	file_stat)
					      throw(std::bad_alloc);
					~block();
					block(const block&);

					void allocate_memory()throw(std::bad_alloc);
					int lock();
					int unlock();
					
					size_t 			data_size;
					void* 			data;
					off64_t 		start_point;
					bool 			dirty_flag;
					bool 			valid;
					int 			exist_flag;
					file*			file_stat;
					Common::remote_task* 	write_back_task;
					pthread_mutex_t 	locker;
					//remote handler will delete this struct if TO_BE_DELETED is setted
					//this appends when user unlink file while remote handler is writing back
					bool 			TO_BE_DELETED;
				};
				typedef std::map<off64_t, block*> block_info_t; //map: start_point : block*

				struct file
				{
					file(const char *path, int exist_flag, ssize_t file_no);
					~file();

					std::string 		file_path;
					int 			exist_flag;
					bool 			dirty_flag;
					int 			main_flag; //main replica indicator
					ssize_t 		file_no;
					block_info_t  		blocks;
					handle_ptr_pool_t 	IOnode_pool;
				};

				//map: file_no: struct file
				typedef std::map<ssize_t, file> file_t; 

				static const char * IONODE_MOUNT_POINT;

				//private function
			private:
				//don't allow copy
				IOnode(const IOnode&); 
				//unregister IOnode from master
				int _unregister();
				//register IOnode to master,  on success return IOnode_id,  on failure throw runtime_error
				ssize_t _register(Common::communication_queue_array_t* output_queue,
						Common::communication_queue_array_t* input_queue) throw(std::runtime_error);
				//virtual int _parse_new_request(int sockfd,
				//		const struct sockaddr_in& client_addr); 
				virtual int _parse_request(Common::extended_IO_task* new_task)override final;

				virtual int remote_task_handler(Common::remote_task* new_task)override final;
				virtual int data_sync_parser(Common::data_sync_task* new_task)override final;
				virtual void configure_dump()override final;
				virtual CBB_error connection_failure_handler(
						Common::extended_IO_task* new_task)override final;

				int _send_data(Common::extended_IO_task* new_task);
				int _receive_data(Common::extended_IO_task* new_task);
				int _open_file(Common::extended_IO_task* new_task);
				int _close_file(Common::extended_IO_task* new_task);
				int _flush_file(Common::extended_IO_task* new_task);
				int _rename(Common::extended_IO_task* new_task);
				int _truncate_file(Common::extended_IO_task* new_task);
				int _append_new_block(Common::extended_IO_task* new_task);
				int _register_new_client(Common::extended_IO_task* new_task);
				int _close_client(Common::extended_IO_task* new_task);
				int _unlink(Common::extended_IO_task* new_task);
				int _node_failure(Common::extended_IO_task* new_task);
				int _heart_beat(Common::extended_IO_task* new_task);
				int _get_sync_data(Common::extended_IO_task* new_task);
				int _register_new_IOnode(Common::extended_IO_task* new_task);
				int _promoted_to_primary(Common::extended_IO_task* new_task);
				int _replace_replica(Common::extended_IO_task* new_task);
				int _remove_IOnode(Common::extended_IO_task* new_task);

				int _get_replica_node_info(Common::extended_IO_task* new_task, file& _file);
				int _get_IOnode_info(Common::extended_IO_task* new_task, file& _file);

				block *_buffer_block(off64_t start_point,
						size_t size)throw(std::runtime_error);

				size_t _write_to_storage(block* block_data)throw(std::runtime_error); 
				size_t _read_from_storage(const  std::string& path,
							  block* block_data)throw(std::runtime_error);
				void _append_block(Common::extended_IO_task* new_task,
						   file& 		     file_stat)throw(std::runtime_error);
				virtual std::string _get_real_path(const char* path)const override final;
				std::string _get_real_path(const std::string& path)const;
				int _remove_file(ssize_t file_no);

				Common::comm_handle_t 
					_connect_to_new_IOnode(ssize_t destination_node_id,
							       ssize_t my_node_id,
							       const char* node_ip);

				//send data to new replica node after IOnode fail over
				int _sync_init_data(Common::data_sync_task* new_task);
				int _sync_write_data(Common::data_sync_task* new_task);

				int _sync_data(file& file,
						off64_t start_point,
						off64_t offset,
						ssize_t size,
						Common::comm_handle_t handle);
				int _send_sync_data(Common::comm_handle_t handle,
						file* requested_file,
						off64_t start_point,
						off64_t offset,
						ssize_t size);
				size_t update_block_data(block_info_t& 		   blocks,
							 file& 			   file,
							 off64_t 		   start_point,
							 off64_t 		   offset,
							 size_t 		   size,
							 Common::extended_IO_task* new_task);

				int _setup_queues();
				int _get_sync_response();

				//private member
			private:

				ssize_t 	   my_node_id; //node id
				file_t 		   _files;
				int 		   _current_block_number;
				int 		   _MAX_BLOCK_NUMBER;
				size_t 		   _memory; //remain available memory; 

				std::string		   master_uri;

				Common::comm_handle	   master_handle;
				Common::comm_handle	   my_handle;

				std::string 	   _mount_point;
				node_handle_pool_t IOnode_handle_pool;
		};

		inline int IOnode::block::lock()
		{
			return pthread_mutex_lock(&locker);
		}

		inline int IOnode::block::unlock()
		{
			return pthread_mutex_unlock(&locker);
		}
	}
}

#endif
