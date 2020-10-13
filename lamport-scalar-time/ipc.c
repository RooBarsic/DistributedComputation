/**
 * @file     ipc.h
 * @Author   Michael Kosyakov and Evgeniy Ivanov (ifmo.distributedclass@gmail.com)
 * @date     March, 2014
 * @brief    A simple IPC library for programming assignments
 *
 * Students must not modify this file!
 */


#include <time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "pa2345.h"
#include "ipc.h"
//#include "common.h"
#include "process.h"

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

/** Send a message to the process specified by id.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param dst     ID of recepient
 * @param msg     Message to send
 *
 * @return 0 on success, any non-zero value on error
 */
int send(void * self, local_id dst, const Message * msg){
	//local_timer++;
	
	Process* provider_process = (Process* ) self;
	int target_process_writing_descriptor_id = provider_process->my_network[dst]->write;

/*
	if(fcntl(target_process_writing_descriptor_id, F_GETFD) == -1 || errno == EBADF){
        printf("Descriptor: %d is closed \n", target_process_writing_descriptor_id);
		exit(1);
		return 1;
    }
*/	

	provider_process->lampic_time++;
	size_t len = write(target_process_writing_descriptor_id, &msg->s_header, sizeof(MessageHeader));
	if(len < sizeof(MessageHeader)){
		printf(" Can't send message from process : %d  to process : %d \n", provider_process->my_id, dst);
		//exit(1);
		return 1;
	}
	
	len = write(target_process_writing_descriptor_id, &msg->s_payload, msg->s_header.s_payload_len);
	if(len < msg->s_header.s_payload_len){
		printf(" Can't send message from process : %d  to process : %d \n", provider_process->my_id, dst);
		//exit(1);
		return 1;
	}		
	//write(target_process_writing_descriptor_id, NULL, sizeof(NULL));

	return 0;
}

//------------------------------------------------------------------------------

/** Send multicast message.
 *
 * Send msg to all other processes including parrent.
 * Should stop on the first error.
 * 
 * @param self    Any data structure implemented by students to perform I/O
 * @param msg     Message to multicast.
 *
 * @return 0 on success, any non-zero value on error
 */
int send_multicast(void * self, const Message * msg){
	Process* provider_process = (Process*) self;
	int number_of_processes = provider_process->brothers_number;
	for(int i = 0; i <= number_of_processes; i++){
		if(i != provider_process->my_id){
			send(self, i, msg);
		}
	}
	return 0;
}

//------------------------------------------------------------------------------

int receive_header_till_size(int target_process_reading_descriptor_id, int target_process_id, int cur_process_id, int exepted_size, MessageHeader* message){
	//printf("   recive header  for %d    from %d \n", cur_process_id, target_process_id);
	int recived_message_size = 0;
	
	while(recived_message_size < exepted_size){
		int buff_size = exepted_size - recived_message_size;
		//printf("   reciving header  for %d    from %d   recived_message_size = %d   exepted_size = %d  \n", cur_process_id, target_process_id, recived_message_size, exepted_size);
		int returned_size = read(target_process_reading_descriptor_id, message + recived_message_size, buff_size);
		if(returned_size <= 0){
			//printf(" Can't receive header2 from process : %d  to process : %d \n", target_process_id, cur_process_id);
			return 1;
		}
		recived_message_size += returned_size;
	}
	return 0;
}

int receive_buff_till_size(int target_process_reading_descriptor_id, int target_process_id, int cur_process_id, size_t exepted_size, char *message){
	int recived_message_size = 0;
	
	while(recived_message_size < exepted_size){
		
		int buff_size = exepted_size - recived_message_size;
		int returned_size = read(target_process_reading_descriptor_id, message + recived_message_size, buff_size);
		//printf(" --------- +++++++++++++++cur_id = %d   target_id = %d      returned_size = %d \n", cur_process_id, target_process_id, returned_size);
		if(returned_size <= 0){
			//printf(" Can't receive cahr buff2 from process : %d  to process : %d \n", target_process_id, cur_process_id);
			return 1;
		}
		recived_message_size += returned_size;
		//printf(" --------- ++++++++++++++++++cur_id = %d   target_id = %d sz = %d \n", cur_process_id, target_process_id, recived_message_size);
	}
	return 0;
}

/** Receive a message from the process specified by id.
 *
 * Might block depending on IPC settings.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param from    ID of the process to receive message from
 * @param msg     Message structure allocated by the caller
 *
 * @return 0 on success, any non-zero value on error
 */
int receive(void * self, local_id from, Message * msg){
	 
	 
	Process* provider_process = (Process* ) self;
	//printf("    recive for %d   from %d  \n", provider_process->my_id, from);
	int target_process_reading_descriptor_id = provider_process->my_network[from]->read;
	
	MessageHeader* recived_header = (MessageHeader*) malloc(sizeof(recived_header));	

	int reply = receive_header_till_size(target_process_reading_descriptor_id, from, provider_process->my_id, sizeof(MessageHeader), recived_header);
	//printf("    process_id = %d    read header  from_id = %d \n", provider_process->my_id, from);
	
	if(reply == 1){
		//printf(" Can't receive header from process : %d  to process : %d \n", from, provider_process->my_id);
		//exit(1);
		return 1;
	}

	//printf("    process_id = %d    try to read char buff  from_id = %d \n", provider_process->my_id, from);	
	char* buffer = (char* ) malloc(recived_header->s_payload_len);
	reply = receive_buff_till_size(target_process_reading_descriptor_id, from, provider_process->my_id, recived_header->s_payload_len, buffer);
	while(reply == 1){
		//printf(" ERROR. Got header but can't receive body :(     -----------------------------------  Can't receive char buff from process : %d  to process : %d \n", from, provider_process->my_id);
		//exit(1);
		reply = receive_buff_till_size(target_process_reading_descriptor_id, from, provider_process->my_id, recived_header->s_payload_len, buffer);
		//return 1;
	}
	
	msg->s_header = *recived_header;	
	memcpy(&(msg->s_header), recived_header, sizeof(MessageHeader));
	memcpy(msg->s_payload, buffer, recived_header->s_payload_len);
	
	return 0;
}

//------------------------------------------------------------------------------

/** Receive a message from any process.
 *
 * Receive a message from any process, in case of blocking I/O should be used
 * with extra care to avoid deadlocks.
 *
 * @param self    Any data structure implemented by students to perform I/O
 * @param msg     Message structure allocated by the caller
 *
 * @return 0 on success, any non-zero value on error
 */
int receive_any(void * self, Message * msg){
	// если падает на боте - добавь в receive - sleep(1) - небольшой сон
	Process* provider_process = (Process* ) self;
	int number_of_processes = provider_process->brothers_number;
	for(int i = 0; i <= number_of_processes; i++){
		if(provider_process->my_network[i]->read != -1 && i != provider_process->my_id){
			//printf("  ~~~~~~~~~~~ process id=%d   try to read from i = %d \n", provider_process->my_id, i);
			if(receive(self, i, msg) == 0){
				return 0;
			}
			//printf("  ~~~~~~~~~~~ process id=%d   Non message from  from i = %d \n", provider_process->my_id, i);
		}

	}
	return 1;	
}

//------------------------------------------------------------------------------

