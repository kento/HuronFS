#include <sys/epoll.h>
#include <sys/eventfd.h>

#include "CBB_communication_thread.h"
#include "CBB_const.h"
#include "CBB_internal.h"

using namespace CBB::Common;


basic_IO_task::basic_IO_task():
	basic_task(),
	socket(-1),
	message_size(0),
	basic_message(),
	current_point(0)
{}

basic_IO_task::~basic_IO_task()
{}

extended_IO_task::extended_IO_task():
	basic_IO_task(),
	extended_size(0),
	send_buffer(NULL),
	receive_buffer(NULL)
{}

extended_IO_task::~extended_IO_task()
{}

CBB_communication_thread::CBB_communication_thread()throw(std::runtime_error):
	keepAlive(KEEP_ALIVE),
	epollfd(epoll_create(LENGTH_OF_LISTEN_QUEUE+1)),
	_socket_pool(),
	thread_started(UNSTARTED),
	input_queue(),
	output_queue(),
	communication_thread(),
	queue_event_fd(eventfd(0,0))
{
	if(-1 == queue_event_fd)
	{
		perror("eventfd");
		throw std::runtime_error("CBB_communication_thread"); 
	}

	if(-1  ==  epollfd)
	{
		perror("epoll_creation"); 
		throw std::runtime_error("CBB_communication_thread"); 
	}
}

CBB_communication_thread::~CBB_communication_thread()
{
	stop_communication_server();
	close(epollfd);
	for(socket_pool_t::iterator it=_socket_pool.begin(); 
			it!=_socket_pool.end(); ++it)
	{
		//inform each client that server is shutdown
		Send(*it, sizeof(int));
		Send_flush(*it, I_AM_SHUT_DOWN); 
		//close socket
		close(*it); 
	}
	close(queue_event_fd);
}

int CBB_communication_thread::start_communication_server()
{
	int ret=SUCCESS;

	if(NULL == input_queue || NULL == output_queue)
	{
		return FAILURE;
	}
	input_queue->set_queue_event_fd(queue_event_fd);
	struct epoll_event event; 
	event.data.fd=queue_event_fd; 
	event.events=EPOLLIN|EPOLLPRI; 
	epoll_ctl(epollfd, EPOLL_CTL_ADD, queue_event_fd, &event); 

	if(0 == (ret=pthread_create(&communication_thread, NULL, thread_function, this)))
	{
		thread_started=STARTED;
	}
	else
	{
		perror("pthread_create");
	}
	return ret;
}

int CBB_communication_thread::stop_communication_server()
{
	keepAlive=NOT_KEEP_ALIVE;
	void* ret=NULL;
	if(STARTED == thread_started)
	{
		pthread_join(communication_thread, &ret);
		//pthread_join(queue_event_wait_thread, &ret);
		thread_started = UNSTARTED;
	}
	return SUCCESS;
}

int CBB_communication_thread::_add_socket(int socketfd)
{
	struct epoll_event event; 
	event.data.fd=socketfd; 
	event.events=EPOLLIN; 
	_socket_pool.insert(socketfd); 
	return epoll_ctl(epollfd, EPOLL_CTL_ADD, socketfd, &event); 
}

int CBB_communication_thread::_add_socket(int socketfd, int op)
{
	struct epoll_event event; 
	event.data.fd=socketfd; 
	event.events=EPOLLIN|op; 
	_socket_pool.insert(socketfd); 
	return epoll_ctl(epollfd, EPOLL_CTL_ADD, socketfd, &event); 
}

int CBB_communication_thread::_delete_socket(int socketfd)
{
	struct epoll_event event; 
	event.data.fd=socketfd; 
	event.events=EPOLLIN;
	_socket_pool.erase(socketfd); 
	return epoll_ctl(epollfd, EPOLL_CTL_DEL, socketfd, &event); 
}

void* CBB_communication_thread::thread_function(void* args)
{
	CBB_communication_thread* this_obj=static_cast<CBB_communication_thread*>(args);
	struct epoll_event events[LENGTH_OF_LISTEN_QUEUE]; 
	memset(events, 0, sizeof(struct epoll_event)*(LENGTH_OF_LISTEN_QUEUE)); 
	uint64_t queue_notification;
	while(KEEP_ALIVE == this_obj->keepAlive)
	{
		int nfds=epoll_wait(this_obj->epollfd, events, LENGTH_OF_LISTEN_QUEUE, -1); 
		for(int i=0; i<nfds; ++i)
		{
			int socket=events[i].data.fd;
			if(this_obj->queue_event_fd == socket)
			{
				_DEBUG("task from handler received\n");
				read(socket, &queue_notification, sizeof(uint64_t));
				this_obj->input_from_producer(this_obj->input_queue);
			}
			else
			{
				_DEBUG("task from socket received\n");
				this_obj->input_from_socket(socket, this_obj->output_queue);
			}
		}
	}
	return NULL;
}

size_t CBB_communication_thread::send(extended_IO_task* new_task)
{
	size_t ret=0;
	int socket=new_task->get_socket();
	//_DEBUG("send message socket=%d, size=%ld\n", socket, new_task->get_message_size());
	Send(socket, new_task->get_message_size());
	Send(socket, new_task->get_extended_data_size());
	if(0 != new_task->get_extended_data_size())
	{
		ret+=Sendv(socket, new_task->get_message(), new_task->get_message_size());
		//_DEBUG("send extended message size=%ld\n", new_task->get_extended_data_size());
		ret+=Sendv_flush(socket, new_task->get_send_buffer(), new_task->get_extended_data_size());
	}
	else
	{
		ret+=Sendv_flush(socket, new_task->get_message(), new_task->get_message_size());
	}
	return ret;
}

size_t CBB_communication_thread::receive_message(int socket, extended_IO_task* new_task)
{
	size_t basic_size=0, extended_size=0;
	int ret=0;
	Recv(socket, basic_size);
	Recv(socket, extended_size);
	//_DEBUG("receive basic message size=%ld\n", basic_size);
	new_task->set_socket(socket);
	new_task->set_message_size(basic_size);
	new_task->set_extended_data_size(extended_size);
	ret=Recvv_pre_alloc(socket, new_task->get_message(), basic_size); 
	if(0 != extended_size)
	{
		//_DEBUG("receive extended message size=%ld\n", extended_size);
		ret+=Recvv_pre_alloc(socket, new_task->get_receive_buffer(BLOCK_SIZE), extended_size);
	}
	return ret;
}

void CBB_communication_thread::set_queue(task_parallel_queue<extended_IO_task>* input_queue, task_parallel_queue<extended_IO_task>* output_queue)
{
	this->input_queue=input_queue;
	this->output_queue=output_queue;
}

int CBB::Common::Send_attr(extended_IO_task* output_task, const struct stat* file_stat)
{
	output_task->push_back(file_stat->st_mode);    /* protection */
	output_task->push_back(file_stat->st_uid);     /* user ID of owner */
	output_task->push_back(file_stat->st_gid);     /* group ID of owner */
	output_task->push_back(file_stat->st_size);    /* total size, in bytes */
	output_task->push_back(file_stat->st_atime);   /* time of last access */
	output_task->push_back(file_stat->st_mtime);   /* time of last modification */
	output_task->push_back(file_stat->st_ctime);   /* time of last status change */
	return SUCCESS;
}

int CBB::Common::Recv_attr(extended_IO_task* new_task, struct stat* file_stat)
{
	new_task->pop(file_stat->st_mode);    /* protection */
	new_task->pop(file_stat->st_uid);     /* user ID of owner */
	new_task->pop(file_stat->st_gid);     /* group ID of owner */
	new_task->pop(file_stat->st_size);    /* total size, in bytes */
	new_task->pop(file_stat->st_atime);   /* time of last access */
	new_task->pop(file_stat->st_mtime);   /* time of last modification */
	new_task->pop(file_stat->st_ctime);   /* time of last status change */
	return SUCCESS;
}
