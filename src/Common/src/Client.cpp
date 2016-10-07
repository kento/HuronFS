#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iterator>

#include "Client.h"
#include "CBB_const.h"
#include "CBB_internal.h"

using namespace CBB::Common;
using namespace std;

Client::Client(int communication_thread_number):
	CBB_communication_thread(),
	_communication_input_queue(communication_thread_number),
	_communication_output_queue(communication_thread_number),
	_threads_handle_map(communication_thread_number)
{
	for(int i=0; i<communication_thread_number; ++i)
	{
		_communication_output_queue[i].set_queue_id(i);
		_communication_input_queue[i].set_queue_id(i);
	}
	CBB_communication_thread::setup(&_communication_output_queue,
			&_communication_input_queue);
}

Client::~Client()
{
	stop_client();
}

int Client::input_from_network(comm_handle_t handle,
		communication_queue_array_t* output_queue_array)
{
	int to_id=0;
	communication_queue_t* output_queue=nullptr;
	extended_IO_task* new_task=nullptr;
	try
	{
		Recv(handle, to_id);
		output_queue=&output_queue_array->at(to_id);
		new_task=output_queue->allocate_tmp_node();
		new_task->set_handle(handle);
		CBB_communication_thread::receive_message(handle, new_task);
	}
	catch(std::runtime_error &e)
	{
		//handle is killed before getting to_id;
		//we return an error code;
		if(nullptr == output_queue)
		{
			output_queue=get_communication_queue_from_handle(handle);
			if(nullptr == output_queue)
			{
				//if there is no one waiting for this handle ignore it
				return SUCCESS;
			}
			new_task=output_queue->allocate_tmp_node();
			new_task->set_handle(handle);
		}
		new_task->set_error(SOCKET_KILLED);
		node_failure_handler(handle);
	}
	output_queue->task_enqueue_signal_notification();
	return SUCCESS;
}

int Client::input_from_producer(communication_queue_t* input_queue)
{
	while(!input_queue->is_empty())
	{
		extended_IO_task* new_task=input_queue->get_task();
		_DEBUG("send request from producer\n");
		if(new_task->is_new_connection())
		{
			new_task->clear_new_conection();
			connect_to_server(new_task->get_uri(),
					  new_task->get_port(),
					  new_task);
		}
		else
		{
			try
			{
				send(new_task);
			}
			catch(std::runtime_error& e)
			{
				//for client if server died before data is sent;
				//we return an error code to client;
				reply_with_handle_error(new_task);
				node_failure_handler(new_task->get_handle());
			}
		}
		input_queue->task_dequeue();
	}
	return SUCCESS;
}

void Client::stop_client()
{
	CBB_communication_thread::stop_communication_server();
}

int Client::start_client()
{
	return CBB_communication_thread::start_communication_server();
}

communication_queue_t* Client::get_new_communication_queue()
{
	auto begin=std::begin(_communication_output_queue), 
	     queue_ptr=begin,
	     end=std::end(_communication_output_queue);

	return &(*queue_ptr);
	/*while(true)
	{
		if(0 == queue_ptr->try_lock_queue())
		{
			//return pointer
			return &(*queue_ptr);
		}
		else
		{
			++queue_ptr;
			if(end == queue_ptr)
			{
				queue_ptr=begin;
			}
		}
	}
	return nullptr;*/
}

communication_queue_t* Client::get_communication_queue_from_handle(comm_handle_t handle)
{
	for(threads_handle_map_t::iterator it=begin(_threads_handle_map);
			end(_threads_handle_map) != it; ++it)
	{
		if(handle == *it)
		{
			return &_communication_input_queue.at(distance(begin(_threads_handle_map), it));
		}
	}
	_DEBUG("return nullptr:handle=%p\n", handle);
	return nullptr;
	//return &_communication_input_queue.at(0);
}

int Client::reply_with_handle_error(extended_IO_task* input_task)
{
	int 			index		=input_task->get_id();
	communication_queue_t& 	input_queue	=_communication_input_queue.at(index);
	extended_IO_task* 	new_task	=input_queue.allocate_tmp_node();

	new_task->set_error(SOCKET_KILLED);
	input_queue.task_enqueue_signal_notification();
	return SUCCESS;
}

int Client::node_failure_handler(comm_handle_t node_handle)
{
	//dummy code
	_DEBUG("IOnode failed handle=%p\n", node_handle);
	return SUCCESS;
}

extended_IO_task* Client::get_query_response(extended_IO_task* query)throw(std::runtime_error)
{
	extended_IO_task* ret=nullptr;
	_threads_handle_map[query->get_id()]=query->get_handle();
	_DEBUG("wait on query address %p\n", get_input_queue_from_query(query));

	do
	{
		ret=get_input_queue_from_query(query)->get_task();
	}while((!compare_handle(query->get_handle(), ret->get_handle()))
			&& (SUCCESS == print_handle_error(ret))
			&& (SUCCESS == dequeue(ret)));	

	//handle killed
	if(SOCKET_KILLED == ret->get_error())
	{
		response_dequeue(ret);
		release_communication_queue(query);
		_DEBUG("handle killed, entry=%p\n", ret);

		_report_IOnode_failure(query->get_handle());
		throw std::runtime_error("handle killed\n");
	}
	_threads_handle_map[query->get_id()]=nullptr;
	_DEBUG("end of get query response\n");
	return ret;
}

int Client::
connect_to_server(const char* 	   uri,
		  int 	           port,
		  extended_IO_task* new_task)
{

	comm_handle handle;

	Connect(uri, port,
		handle, 
		new_task->get_message(),
		reinterpret_cast<size_t*>(
			new_task->get_message()
		+MESSAGE_SIZE_OFF));
	new_task->set_handle(&handle);
	CBB_communication_thread::_add_handle(&handle);

	return SUCCESS;
}
