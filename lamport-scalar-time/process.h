/**
 * @file     ipc.h
 * @Author   Michael Kosyakov and Evgeniy Ivanov (ifmo.distributedclass@gmail.com)
 * @date     March, 2014
 * @brief    A simple IPC library for programming assignments
 *
 * Students must not modify this file!
 */

//#ifndef __IFMO_DISTRIBUTED_CLASS_IPC__H

#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "banking.h"

#include "ipc.h"

//------------------------------------------------------------------------------

typedef struct{
	pid_t pid;
	int read;
	int write;
} __attribute__((packed)) Network;

typedef struct{
	int brothers_number;
	int parent_pid;
	int my_id;
	int my_status;
	pid_t my_pid;
	Network my_network[20][2];
	timestamp_t lampic_time;
	BalanceHistory local_history;
} __attribute__((packed)) Process;


//------------------------------------------------------------------------------

//#endif // __IFMO_DISTRIBUTED_CLASS_IPC__H

