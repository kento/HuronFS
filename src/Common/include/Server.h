
#ifndef SERVER_H_
#define SERVER_H_

#include <netinet/in.h>
#include <stdexcept>

#include "CBB_task_parallel.h"
#include "CBB_communication_thread.h"
#include "CBB_request_handler.h"
#include "CBB_remote_task.h"

//implement later
//#include "Thread_pool.h"

namespace CBB
{
	namespace Common
	{
		class Server:public CBB_communication_thread, public CBB_request_handler, public CBB_remote_task
		{
			public:
				void stop_server(); 
				int start_server();

			protected:
				Server(int port)throw(std::runtime_error); 
				virtual ~Server(); 
				//int _add_socket(int socket, int op);
				//int _add_socket(int socket);
				virtual int input_from_socket(int socket,
						CBB::Common::task_parallel_queue<CBB::Common::extended_IO_task>* output);
				virtual int input_from_producer(CBB::Common::task_parallel_queue<CBB::Common::extended_IO_task>*output);
				void _init_server()throw(std::runtime_error); 
				virtual int _parse_request(CBB::Common::extended_IO_task* new_task, CBB::Common::task_parallel_queue<CBB::Common::extended_IO_task>* output_queue)=0;
				virtual std::string _get_real_path(const char* path)const=0;
				virtual std::string _get_real_path(const std::string& path)const=0;
				virtual int remote_task_handler(remote_task* new_task)=0;

				int _recv_real_path(extended_IO_task* new_task, std::string& real_path);
				int _recv_real_relative_path(extended_IO_task* new_task, std::string& real_path, std::string &relative_path);

			protected:
				task_parallel_queue<extended_IO_task> _communication_input_queue;
				task_parallel_queue<extended_IO_task> _communication_output_queue;
				struct sockaddr_in _server_addr;
				int _port;
				int _server_socket;
		}; 

		/*inline int Server::_add_socket(int socket, int op)
		{
			return CBB_communication_thread::_add_socket(socket, op);
		}

		inline int Server::_add_socket(int socket)
		{
			return CBB_communication_thread::_add_socket(socket);
		}*/
	}
}

#endif
