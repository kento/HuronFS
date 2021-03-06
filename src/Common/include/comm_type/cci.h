/*
 * Copyright (c) 2017, Lawrence Livermore National Security, LLC. Produced at
 * the Lawrence Livermore National Laboratory. 
 * Written by Tianqi Xu, xu.t.aa@m.titech.ac.jp, LLNL-CODE-722817.
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

#ifndef CCI_H_
#define CCI_H_

#include "Comm_basic.h"

namespace CBB
{
	namespace Common
	{
		struct exchange_args
		{
			int epollfd;
			int socket;
			const char* uri;
			size_t uri_len;
		};

		class CBB_cci:
			public Comm_basic
		{
			public:
			CBB_cci()=default;
		virtual	~CBB_cci()=default;

		virtual CBB_error init_protocol()
			throw(std::runtime_error)override final;

		virtual CBB_error 
			init_server_handle(ref_comm_handle_t  server_handle,
					   const std::string& my_uri,
					   int		      port)
			throw(std::runtime_error)override final;

		virtual CBB_error end_protocol(comm_handle_t server_handle)override final;

		virtual CBB_error Close(comm_handle_t handle)override final;

		virtual bool compare_handle(comm_handle_t src,
				comm_handle_t des)override final;

		virtual CBB_error
			get_uri_from_handle(comm_handle_t     handle,
					    const char**      uri)override final;
		virtual CBB_error
			Connect(const char* uri,
				int	    port,
				ref_comm_handle_t handle,
				void* 	    buf,
				size_t*	    size)
			throw(std::runtime_error)override final;

		virtual size_t 
			Recv_large(comm_handle_t sockfd, 
				   void*         buffer,
				   size_t 	 count)
			throw(std::runtime_error)override final;

		virtual size_t 
			Send_large(comm_handle_t sockfd, 
				   const void*   buffer,
				   size_t 	 count)
			throw(std::runtime_error)override final;

		size_t Recv_large(comm_handle_t sockfd, 
				  void*         buffer,
				  size_t 	count,
				  off64_t	offset)
			throw(std::runtime_error);

		size_t Send_large(comm_handle_t handle, 
				  const void*   buffer,
				  size_t 	count,
				  off64_t	offset)
			throw(std::runtime_error);

		size_t Recv_large(comm_handle_t sockfd, 
				  const unsigned char*   completion,
				  size_t	comp_size,
				  void*         buffer,
				  size_t 	count,
				  off64_t	offset)
			throw(std::runtime_error);

		size_t Send_large(comm_handle_t handle, 
				  const unsigned char*   completion,
				  size_t	comp_size,
				  const void*   buffer,
				  size_t 	count,
				  off64_t	offset)
			throw(std::runtime_error);


		CBB_error start_uri_exchange_server(const std::string& 	my_uri,
						    int 		port)
			throw(std::runtime_error);

		CBB_error register_mem( void*		ptr,
				 	size_t 		size,
				 	comm_handle_t	handle,
				 	int		flag);

		CBB_error deregister_mem(comm_handle_t handle);
		CBB_error deregister_mem(cci_rma_handle_t* handle);

		static void*
			socket_thread_fun(void *obj);
		
		cci_endpoint_t* get_endpoint();

		virtual const char*
			get_my_uri()const;
		void dump_remote_key()const;

		private:

		static size_t 
			send_by_tcp(int   	sockfd,
				    const char* buf,
				    size_t  	len);

		static size_t 
			recv_by_tcp(int   	sockfd,
				    char*  	buf,
				    size_t  	len);

		virtual size_t
			Do_recv(comm_handle_t sockfd, 
				void* 	      buffer,
				size_t 	      count,
				int 	      flag)
			throw(std::runtime_error)override final;

		virtual size_t 
			Do_send(comm_handle_t 	sockfd,
				const void* 	buffer, 
				size_t 		count, 
				int 		flag)
			throw(std::runtime_error)override final;

		CBB_error
			get_uri_from_server(const char* server_addr,
					    int 	server_port,
					    char*	uri_buf)
			throw(std::runtime_error);

		static void* 
			uri_exchange_thread_fun(void* args);

		protected:
			cci_endpoint_t*   endpoint;
			const char*	  uri;
			pthread_t	  exchange_thread;
			struct exchange_args args;

		};

		inline CBB_error CBB_cci::
			register_mem(void*	    ptr,
				     size_t 	    size,
				     comm_handle_t  handle,
				     int	    flag)
		{
			int ret;
			_DEBUG("register mem %p size=%ld\n",
					ptr, size);
			if (CCI_SUCCESS != (ret=cci_rma_register(endpoint,
					ptr, size, flag, 
					const_cast<cci_rma_handle_t**>(&(handle->local_rma_handle)))))
			{
				_DEBUG("cci_register() failed with %s\n",
						cci_strerror(endpoint, (cci_status)ret));
				return FAILURE;
			}
			handle->dump_local_key();
			return SUCCESS;
		}

		inline CBB_error CBB_cci::
			deregister_mem(comm_handle_t handle)
		{
			int ret;

			_DEBUG("deregister mem\n");
			if (CCI_SUCCESS != (ret=cci_rma_deregister(endpoint,
					    handle->local_rma_handle)))
			{
				_DEBUG("cci_deregister() failed with %s\n",
						cci_strerror(endpoint, (cci_status)ret));
				return FAILURE;
			}
			return SUCCESS;
		}

		inline bool CBB_cci::
			compare_handle(comm_handle_t src,
				comm_handle_t des)
			{
				return src->cci_handle == des->cci_handle;
			}

		inline size_t CBB_cci:: 
			Recv_large(comm_handle_t handle, 
				   void*         buffer,
				   size_t 	 count)
			throw(std::runtime_error)
			{
				//forwarding
				return Recv_large(handle, buffer, count, 0);
			}


		inline size_t CBB_cci:: 
			Send_large(comm_handle_t handle, 
				   const void*   buffer,
				   size_t 	 count)
			throw(std::runtime_error)
			{
				//forwarding
				return Send_large(handle, buffer, count, 0);
			}

		inline size_t CBB_cci:: 
			Recv_large(comm_handle_t handle, 
				   void*         buffer,
				   size_t 	 count,
				   off64_t	 offset)
			throw(std::runtime_error)
			{
				return Recv_large(handle, nullptr, 0, buffer, count, offset);
			}

		inline size_t CBB_cci:: 
			Send_large(comm_handle_t handle, 
				   const void*   buffer,
				   size_t 	 count,
				   off64_t	 offset)
			throw(std::runtime_error)
			{
				return Send_large(handle, nullptr, 0, buffer, count, offset);
			}

		inline cci_endpoint_t* CBB_cci::
			get_endpoint()
		{
			return this->endpoint;
		}

		inline const char* CBB_cci::
			get_my_uri()const
		{
			return this->uri;
		}

		inline ref_comm_handle_t CBB_handle::
			 operator = (const_ref_comm_handle_t src)
		{
			memcpy(this, &src, sizeof(src));
			return *this;
		}

		inline void CBB_handle::
			dump_remote_key()const
		{
			_DEBUG("remote key=%lu %lu %lu %lu\n", 
					remote_rma_handle.stuff[0],
					remote_rma_handle.stuff[1],
					remote_rma_handle.stuff[2],
					remote_rma_handle.stuff[3]);
		}

		inline void CBB_handle::
			dump_local_key()const

		{
			_DEBUG("key=%lu %lu %lu %lu\n", 
					local_rma_handle->stuff[0],
					local_rma_handle->stuff[1],	
					local_rma_handle->stuff[2],	
					local_rma_handle->stuff[3]);
		}
	}
}
#endif
