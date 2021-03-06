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

#ifndef CBB_IO_TASK_H_
#define CBB_IO_TASK_H_

#include <vector>

#include "CBB_task_parallel.h"
#include "Comm_api.h"

namespace CBB
{
	namespace Common
	{
		class connection_task
		{
			public:
			connection_task()=default;
			virtual ~connection_task()=default;
			void setup_new_connection(const char* uri,
						  int	      port);
			void clear_new_conection();
			bool is_new_connection()const;
			//zero copy, be careful
			void set_uri(const char* uri);
			const char* get_uri()const;
			void set_port(int port);
			int get_port()const;
			void clear_addr();

			private:
				bool new_connection;
				const char* uri;   //server uri
				int port;	   //port
		};

		class basic_IO_task:
			public basic_task,
			public connection_task
		{
			public:
				basic_IO_task();
				basic_IO_task(int id, basic_IO_task* next);
				virtual ~basic_IO_task()=default;
				template<typename T> size_t push_back(const T& value);
				template<typename T> size_t push_backv(const T& value, size_t num);
				size_t push_back_string(const unsigned char* string);
				size_t push_back_string(const char* string);
				size_t push_back_string(const char* string, size_t size);
				size_t push_back_string(const std::string& string);
				template<typename T> size_t pop(T& var);
				template<typename T> size_t popv(T* var, size_t num);
				template<typename T> size_t pop_uncopy(T** var);
				template<typename T> size_t popv_uncopy(T** var, size_t num);
				template<typename T> size_t do_pop(T** value, size_t num);
				size_t do_push(const void* value, size_t num);
				size_t pop_string(unsigned char** var);
				size_t pop_string(char** var);
				virtual void reset()override;
				virtual void set_id(int id)override final;
				comm_handle_t get_handle()const;
				comm_handle_t get_handle();
				void set_handle(comm_handle_t handle);
				void set_handle(const comm_handle& handle);
				unsigned char* get_message();
				unsigned char* get_basic_message();
				void set_message_size(size_t message_size);
				size_t get_message_size()const;
				size_t get_total_message_size()const;
				int get_receiver_id()const;
				void set_receiver_id(int id);
				int get_sender_id()const;
				int get_error()const;
				void set_error(int error);
				void swap_sender_receiver();
				bool has_error()const;
			public:
				comm_handle handle;
				unsigned char message_buffer[MAX_BASIC_MESSAGE_SIZE];//the first few bytes are reserved for message meta
				//set to SUCCESS or SOCKET_KILLED
				int error;
				//remote id refers to the queue which communicates with
				int* receiver_id; //offset = 0
				int* sender_id;  //offset = sizeof(int)
				size_t* message_size; //offset = 2*sizeof(int)
				//extended_size
				//total meta size=2*int + 2*size_t
				unsigned char* basic_message;
				size_t current_point;
		};

		struct send_buffer_element
		{
			char* buffer;
			size_t size;

			send_buffer_element(char* buffer, size_t size);
		};

		typedef std::vector<send_buffer_element> send_buffer_t;

		class extended_IO_task:public basic_IO_task
		{
			public:
				extended_IO_task();
				extended_IO_task(int id ,extended_IO_task* next);
				virtual ~extended_IO_task();

				int get_mode()const;
				void init_response_large_transfer(extended_IO_task* request, int mode);
				void init_for_large_transfer(int mode);

				void setup_large_transfer(int mode);
				void set_mode(int mode);
				size_t get_received_data(void* buffer, size_t size);
				size_t get_received_data_all(void* buffer);
				void set_extended_data_size(size_t size);
				size_t get_extended_data_size()const;
				virtual void reset()override final;
				const send_buffer_t* get_send_buffer()const;
				unsigned char* get_receive_buffer(size_t size);
				void push_send_buffer(char* buf, size_t size);
				void push_recv_buffer(char* buf, size_t size);
				void clear_send_recv_buffer();
				size_t push_string_prealloc(const std::string& string);//push string to prealloc area instead of a given buffer, used in readdir
				size_t do_push_prealloc(const void* buf, size_t size);
				void setup_prealloc_buffer();//must be used after all push string prealloc finished
			private:

				//rma_read; rma_write
				int 	      mode;
				size_t* extended_size;
				send_buffer_t send_buffer;
				unsigned char* receive_buffer;
				unsigned char* current_receive_buffer_ptr;
				unsigned char  extended_buffer[MAX_EXTENDED_MESSAGE_SIZE];
		};

		class string_buf
		{
			public:
				string_buf(size_t total_size);
				~string_buf();
				size_t pop_string(char** string_ptr);
				void* get_buffer();
			private:
				size_t do_pop(void** buf, size_t size);
			private:
				unsigned char*  buf;
				size_t 		total_size;
				unsigned char*  cur_ptr;
		};

		int Send_attr(extended_IO_task* output_task,
			      const struct stat* file_stat);
		int Recv_attr(extended_IO_task* new_task,
				struct stat* file_stat);
		typedef task_parallel_queue<extended_IO_task> communication_queue_t;

		template<typename T> size_t basic_IO_task::
			push_back(const T& value)
		{
			return push_backv(value, 1);
		}

		template<typename T> inline size_t basic_IO_task::
			push_backv(const T& value, size_t num)
		{
			return do_push(&value, num*sizeof(T));
		}

		inline size_t basic_IO_task::
			do_push(const void* value, size_t num)
		{
			memcpy(basic_message + (*message_size), value, num);
			*(this->message_size) += num;
			return num;
		}

		template<typename T> inline size_t basic_IO_task::
			pop_uncopy(T** var)
		{
			return popv_uncopy(var, 1);
		}

		template<typename T> inline size_t basic_IO_task::
			popv_uncopy(T** var, size_t num)
		{
			return do_pop(var, num*sizeof(T));
		}

		template<typename T> inline size_t basic_IO_task::
			do_pop(T** var, size_t num)
		{
			*var=reinterpret_cast<T*>(
					basic_message + current_point);
			this->current_point += num;
			return num;
		}

		template<typename T> inline size_t basic_IO_task::
			pop(T& var)
		{
			return popv(&var, 1); 
		} 

		template<typename T> inline size_t basic_IO_task::
			popv(T* var, size_t num)
		{
			T* tmp=nullptr;
			size_t ret=popv_uncopy(&tmp, num);
			memcpy(var, tmp, ret);
			return ret;
		}

		inline size_t basic_IO_task::
			push_back_string(const unsigned char* string)
		{
			return push_back_string(
				reinterpret_cast<const char*>(string));
		}

		inline size_t basic_IO_task::
			push_back_string(const char* string)
		{
			size_t len=strlen(string)+1;
			push_back(len);
			do_push(string, len*sizeof(unsigned char));
			return len;
		}

		inline size_t basic_IO_task::
			push_back_string(const char* string, size_t size)
		{
			push_back(size+1);
			do_push(string, (size+1)*sizeof(unsigned char));
			return size+1;
		}

		inline size_t basic_IO_task::
			push_back_string(const std::string& String)
		{
			return push_back_string(String.c_str(), String.size());
		}

		inline size_t basic_IO_task::
			pop_string(unsigned char** var)
		{
			size_t len=0;
			pop(len);
			do_pop(var, len*sizeof(unsigned char));
			return len;
		}

		inline size_t basic_IO_task::
			pop_string(char** var)
		{
			return pop_string(reinterpret_cast<unsigned char**>(var));
		}
		
		inline unsigned char* basic_IO_task::
			get_basic_message()
		{
			return this->message_buffer+MESSAGE_META_OFF;
		}

		inline unsigned char* basic_IO_task::
			get_message()
		{
			return this->message_buffer;
		}

		inline comm_handle_t basic_IO_task::
			get_handle()const
		{
			return &(this->handle);
		}

		inline comm_handle_t basic_IO_task::
			get_handle()
		{
			return &(this->handle);
		}

		inline void basic_IO_task::reset()
		{
			set_message_size(0);
			basic_IO_task::clear_new_conection();
			this->current_point=0;
			this->error=SUCCESS;
		}

		inline void connection_task::
			setup_new_connection(const char* uri,
					     int 	 port)
			{
				this->new_connection=SETUP_CONNECTION;
				set_uri(uri);
				set_port(port);
			}

		inline void connection_task::
			clear_new_conection()
			{
				this->new_connection=NORMAL_IO;
			}

		inline bool connection_task::
			is_new_connection()const
			{
				return SETUP_CONNECTION==this->new_connection;
			}

		inline void connection_task::
			set_uri(const char* uri)
			{
				this->uri=uri;
			}

		inline const char* connection_task::
			get_uri()const
			{
				return this->uri;
			}

		inline void connection_task::
			set_port(int port)
			{
				this->port=port;
			}

		inline int connection_task::
			get_port()const
			{
				return this->port;
			}

		inline void connection_task::
			clear_addr()
			{
				set_uri(nullptr);
				set_port(-1);
			}

		inline void basic_IO_task::set_id(int id)
		{
			basic_task::set_id(id);
			*(this->sender_id)=id;
		}

		inline void basic_IO_task::
			set_handle(comm_handle_t handle)
		{
			this->handle=*handle;
		}

		inline void basic_IO_task::
			set_handle(const comm_handle& handle)
		{
			return set_handle(&handle);
		}


		inline size_t basic_IO_task::
			get_message_size()const
		{
			return *(this->message_size);
		}

		inline size_t basic_IO_task::
			get_total_message_size()const
		{
			return *(this->message_size)+MESSAGE_META_OFF;
		}

		inline void basic_IO_task::
			set_message_size(size_t message_size)
		{
			*(this->message_size)=message_size;
		}

		inline int basic_IO_task::
			get_receiver_id()const
		{
			return *(this->receiver_id);
		}

		inline void basic_IO_task::
			set_receiver_id(int id)
		{
			*(this->receiver_id)=id;
		}

		inline int basic_IO_task::
			get_error()const
		{
			return this->error;
		}

		inline int basic_IO_task::
			get_sender_id()const
		{
			return *(this->sender_id);
		}

		inline void basic_IO_task::
			set_error(int error)
		{
			this->error=error;
		}

		inline void basic_IO_task::
			swap_sender_receiver()
		{
			*(this->receiver_id)=*(this->sender_id);
			*(this->sender_id)=get_id();
		}

		inline bool basic_IO_task::
			has_error()const
		{
			return SUCCESS != this->error;
		}

		inline size_t extended_IO_task::
			get_received_data(void* buffer, size_t size)
		{
			_DEBUG("get_receive data\n");
			//bad hack, fix later
#ifdef TCP
			memcpy(buffer, current_receive_buffer_ptr, size);
			current_receive_buffer_ptr+=size;
			*(this->extended_size) -= size;
#else
			push_recv_buffer(reinterpret_cast<char*>(
						buffer), size);
#endif
			return size;
		}

		inline size_t extended_IO_task::
			get_received_data_all(void* buffer)
		{
			if(nullptr == current_receive_buffer_ptr)
			{
				return *(this->extended_size);
			}
			size_t ret=*(this->extended_size);
			_DEBUG("receive size %ld\n", *extended_size);
			memcpy(buffer, current_receive_buffer_ptr,
					*extended_size);
			*extended_size=0;
			return ret;
		}

		inline void extended_IO_task::
			reset()
		{
			basic_IO_task::reset();
			*(this->extended_size) = 0;
			this->send_buffer.clear();
			this->current_receive_buffer_ptr=
				this->receive_buffer;
			this->clear_send_recv_buffer();
		}

		inline size_t extended_IO_task::
			get_extended_data_size()const
		{
			return *this->extended_size;
		}

		inline void extended_IO_task::
			set_extended_data_size(size_t size)
		{
			*this->extended_size=size;
		}

		inline const send_buffer_t* extended_IO_task::
			get_send_buffer()const
		{
			return &this->send_buffer;
		}

		inline unsigned char* extended_IO_task::
			get_receive_buffer(size_t size)
		{
			if(nullptr == this->receive_buffer)
			{
				this->receive_buffer =
					new unsigned char[size];
			}
			this->current_receive_buffer_ptr=
				this->receive_buffer;

			return this->receive_buffer;
		}
		
		inline void extended_IO_task::
			push_send_buffer(char* buf, size_t size)
		{
			send_buffer.push_back(send_buffer_element(buf, size));
			*(this->extended_size)+=size;
		}

		inline void extended_IO_task::
			push_recv_buffer(char* buf, size_t size)
		{
			push_send_buffer(buf, size);
		}

		inline void extended_IO_task::
			clear_send_recv_buffer()
		{
			this->send_buffer.clear();
		}

		inline int extended_IO_task::
			get_mode()const
		{
			return this->mode;
		}

		inline void extended_IO_task::
			set_mode(int mode)
		{
			this->mode=mode;
		}

		inline size_t extended_IO_task::
			do_push_prealloc(const void* buf, size_t size)
			{
				memcpy(extended_buffer+(*extended_size), buf, 
						size);
				*(this->extended_size) += size;
				return size;
			}

		inline size_t extended_IO_task::
			push_string_prealloc(const std::string& string)
		{
			size_t ret=0;
			size_t string_size=string.size()+1;
			ret+=do_push_prealloc(&string_size, sizeof(string_size));
			ret+=do_push_prealloc(string.c_str(), string_size);
			return ret;
		}
				
		inline void extended_IO_task::
			setup_prealloc_buffer()
		{
			push_send_buffer(reinterpret_cast<char*>(this->extended_buffer),
					*(this->extended_size));
			//bad hack fix later
			*(this->extended_size)/=2;
		}

		inline void extended_IO_task::
			init_for_large_transfer(int mode)
			{
#ifdef CCI
				this->mode=mode;
				this->pop(this->handle.remote_rma_handle);
				this->handle.dump_remote_key();
#endif
			}

		inline void extended_IO_task::
			init_response_large_transfer(extended_IO_task* 	request,
					     	     int 		mode)
			{
#ifdef CCI
				this->mode=mode;
				request->pop(this->handle.remote_rma_handle);
				this->handle.dump_remote_key();
#endif
			}

		inline void extended_IO_task::
			setup_large_transfer(int mode)
			{
				this->mode=mode;
			}
		inline string_buf::
			string_buf(size_t total_size):
				buf(new unsigned char[total_size]),
				total_size(total_size),
				cur_ptr(buf)
		{}

		inline size_t string_buf::
			do_pop(void** buf, size_t size)
			{
				*buf=cur_ptr;
				cur_ptr+=size;
				return size;
			}

		inline size_t string_buf::
			pop_string(char** string_ptr)
			{
				size_t* size=nullptr;
				do_pop((void**)&size, sizeof(size_t));
				_DEBUG("size of string is %lu\n", *size);
				do_pop((void**)string_ptr, *size);
				return *size;
			}

		inline void* string_buf::
			get_buffer()
			{
				return buf;
			}

		inline string_buf::
			~string_buf()
			{
				delete[] buf;
			}
	}
}
#endif
