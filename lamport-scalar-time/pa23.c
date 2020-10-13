#include <time.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>  //Для puts, printf, fprintf, fdopen
#include <fcntl.h>  //Для open
#include <fcntl.h>  //Для определения константы O_RDWR


#include "pa2345.h"
#include "common.h"
#include "process.h"
#include "banking.h"
#include "ipc.h"

//#include "ipc.c"
//#include "bank_robbery.c"


#define Im_Childe 123
#define Im_Parent 0
#define Im_Error -1

int brothers_number = 0;
int id_counter = 0;
int status;
int my_inner_id;
pid_t my_pid;
int pipes[20][20][2];
int balances[20];

timestamp_t local_timer = 0;
Process process;


FILE *event_log_file;
FILE *pipe_log_file;



pid_t forik(){
	pid_t ret=fork();
	switch(ret)
	{
	  case -1: 
		status = Im_Error;
		//printf(" some error! \n");
		break; // {*}при вызове fork() возникла ошибка{*}/
	  case 0 :
		status = Im_Childe;
		//printf(" it's cod potomka. pid == 0 \n");
		break; // {*}это код потомка{*}/
	  default :
	    status = Im_Parent;
		//printf(" it's parent code! childe-pid = %d \n", ret); 
		// {*}это код родительского процесса{*}/
	}
	return ret;
}

int create_pipes(){
	///---printf("  Parent :: started creating pipes \n ");
	for(int i = 0; i <= brothers_number; i++){
		for(int j = i + 1; j <= brothers_number; j++){
			if(pipe(pipes[i][j])){
				printf("  somthing wrong in creating pipes ");
				return -1;
			}
			if(pipe(pipes[j][i])){
				printf("  somthing wrong in creating pipes ");
				return -1;
			}
			///---printf(" pipes : [%d, %d] = [%d, %d] \n", i, j, pipes[i][j][0], pipes[i][j][1]); 
			///---printf(" pipes : [%d, %d] = [%d, %d] \n", j, i, pipes[j][i][0], pipes[j][i][1]); 
			
			// делаем пайпы - неблокируюшими --------------------------------------------------------
			int flags = fcntl(pipes[i][j][0], F_GETFL, 0);
			fcntl(pipes[i][j][0], F_SETFL, flags | O_NONBLOCK);

			flags = fcntl(pipes[i][j][1], F_GETFL, 0);
			fcntl(pipes[i][j][1], F_SETFL, flags | O_NONBLOCK);

			flags = fcntl(pipes[j][i][0], F_GETFL, 0);
			fcntl(pipes[j][i][0], F_SETFL, flags | O_NONBLOCK);

			flags = fcntl(pipes[j][i][1], F_GETFL, 0);
			fcntl(pipes[j][i][1], F_SETFL, flags | O_NONBLOCK);

		}
	}
	return 0;
}


int my_receive(void * self, local_id from, Message * msg){
	int status_x = receive(self, from, msg);
	/*if(status_x == 0){
		if(local_timer < msg->s_header.s_local_time){
			local_timer = msg->s_header.s_local_time;
		}
		local_timer++;
	}*/
	return status_x;
}

int my_receive_any(void * self, Message * msg){
	int status_x = receive_any(self, msg);
	/*if(status_x == 0){
		if(local_timer < msg->s_header.s_local_time){
			local_timer = msg->s_header.s_local_time;
		}
		local_timer++;
	}*/
	return status_x;	
}

//*
/// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Waves <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
void push_lamper_time(timestamp_t new_time) {
    //timestamp_t cur_time = process.lampic_time;
    if (new_time > process.lampic_time){
        process.lampic_time = new_time;
    }
    process.lampic_time++;
    
}
void wave_send_starteds(){
	// Sending STARTED message to every one
	process.lampic_time++;
	Message hello_message;
	sprintf(hello_message.s_payload, log_started_fmt, get_lamport_time(), process.my_id, process.my_pid, process.parent_pid, balances[process.my_id]);
  
	MessageHeader header;
	header.s_local_time = get_lamport_time();
	header.s_payload_len = strlen(hello_message.s_payload);
	header.s_type = STARTED;
	header.s_magic = MESSAGE_MAGIC;
	hello_message.s_header = header;
	
	int x = send_multicast(&process, &hello_message);
	if(x == -1){
		printf("err\n");
	}
}

void wave_receive_starteds() {
    for(int i = 1; i <= process.brothers_number; i++){
		if(i != process.my_id){
			Message recived_hello_mesage;
			while(receive(&process, i, &recived_hello_mesage) != 0);
			push_lamper_time(recived_hello_mesage.s_header.s_local_time);
			///---printf(" Child : %d   got from process : %d    message :  %s \n", process.my_id, i, recived_hello_mesage.s_payload);
		}
	}
}

void wave_done_to_everyone(){
	// Sending DONE to Parent --------------------------------------
	process.lampic_time++;
	Message done_message;
	sprintf(done_message.s_payload, log_done_fmt, get_lamport_time(), process.my_id, process.local_history.s_history[MAX_T].s_balance);
  
	MessageHeader done_header;
	done_header.s_local_time = get_lamport_time();
	done_header.s_payload_len = strlen(done_message.s_payload);
	done_header.s_type = DONE;
	done_header.s_magic = MESSAGE_MAGIC;
	done_message.s_header = done_header;

	send(&process, 0, &done_message);
}

void init_hisltory(){
	process.local_history.s_id = process.my_id;
	process.local_history.s_history_len = 1;
	for(timestamp_t t = 0; t <= MAX_T; t++){
		process.local_history.s_history[t] = (BalanceState){
			.s_balance_pending_in = 0,
			.s_time = t,
			.s_balance = balances[process.my_id]
		};
	}
}

void kill_wave(){
	// Sending KILL message ( STOP ) to everyone
	process.lampic_time++;
	
	Message stop_message;
	MessageHeader* stop_message_header = (MessageHeader* ) &stop_message.s_header;
	stop_message_header->s_payload_len = 0;
	stop_message_header->s_local_time = get_lamport_time();
	stop_message_header->s_magic = MESSAGE_MAGIC;
	stop_message_header->s_type = STOP;

	send_multicast(&process, &stop_message);

}

void done_wave_receive(){
	// Start receiving STARTED from everyone
	for(int i = 1; i <= process.brothers_number; i++){
		if(i != process.my_id){
			Message received_bye_message;
			while(receive(&process, i, &received_bye_message) != 0);
			push_lamper_time(received_bye_message.s_header.s_local_time);
			///---printf(" Parent : %d   got from Childe : %d    message : %s \n", process.my_id, i, received_bye_message.s_payload);
		}
	}
	///---printf("   Parent got KILL from everyone \n");
}
/// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Waves >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
/*
void print_history(const AllHistory * history)
{
    if (history == NULL) {
        fprintf(stderr, "print_history: history is NULL!\n");
        exit(1);
    }

    typedef struct {
        int balance;
        int pending;
    } Pair;

    int max_time = 0;
    int has_pending = 0;
    int nrows = history->s_history_len + 2; // 0 row (parent) is not used + Total row
    Pair table[nrows][MAX_T];
    memset(table, 0, sizeof(table));

    for (int i = 0; i < history->s_history_len; ++i) {
        for (int j = 0; j < history->s_history[i].s_history_len; ++j) {
            const BalanceState * change = &history->s_history[i].s_history[j];
            int id = history->s_history[i].s_id;
            table[id][change->s_time].balance = change->s_balance;
            table[id][change->s_time].pending = change->s_balance_pending_in;
            if (max_time < change->s_time) {
                max_time = change->s_time;
            }
            if (change->s_balance_pending_in > 0) {
                has_pending = 1;
            }
        }
    }

    if (max_time > MAX_T) {
        fprintf(stderr, "print_history: max value of s_time: %d, expected s_time < %d!\n",
                max_time, MAX_T);
        return;
    }

    // Calculate total sum
    for (int j = 0; j <= max_time; ++j) {
        int sum = 0;
        for (int i = 1; i <= history->s_history_len; ++i) {
            sum += table[i][j].balance + table[i][j].pending;
        }
        table[nrows-1][j].balance = sum;
        table[nrows-1][j].pending = 0;
    }
    
    // pretty print
    fflush(stderr);
    fflush(stdout);

    const char * cell_format_pending = " %d (%d) ";
    const char * cell_format = " %d ";

    char buf[128];
    int max_cell_width = 0;
    for (int i = 1; i <= history->s_history_len; ++i) {
        for (int j = 0; j <= max_time; ++j) {
            if (has_pending) {
                sprintf(buf, cell_format_pending, table[i][j].balance, table[i][j].pending);
            } else {
                sprintf(buf, cell_format, table[i][j].balance);
            }
            int width = strlen(buf);
            if (max_cell_width < width) {
                max_cell_width = width;
            }
        }
    }

    const char * const first_column_header = "Proc \\ time |";
    const int first_column_width = strlen(first_column_header);
    const int underscrores = (first_column_width + 1) + (max_cell_width + 1) * (max_time + 1);

    char hline[underscrores + 2];
    for (int i = 0; i < underscrores; ++i) {
        hline[i] = '-';
    }
    hline[underscrores] = '\n';
    hline[underscrores + 1] = '\0';

    if (has_pending) {
        printf("\nFull balance history for time range [0;%d], $balance ($pending):\n", max_time);
    } else {
        printf("\nFull balance history for time range [0;%d], $balance:\n", max_time);
    }
    printf("%s \n", hline);
    
    printf("%s ", first_column_header);
    for (int j = 0; j <= max_time; ++j) {
        printf("%*d |", max_cell_width - 1, j);
    }
    printf("\n");
    printf("%s \n", hline);

    for (int i = 1; i <= history->s_history_len; ++i) {
        printf("%11d | ", i);
        for (int j = 0; j <= max_time; ++j) {
            if (has_pending) {
                sprintf(buf, cell_format_pending, table[i][j].balance, table[i][j].pending);
            } else {
                sprintf(buf, cell_format, table[i][j].balance);
            }
            printf("%*s|", max_cell_width, buf);
        }
        printf("\n");
        printf("%s \n", hline);
    }

    printf("      Total | ");
    for (int j = 0; j <= max_time; ++j) {
        printf("%*d |", max_cell_width - 1, table[nrows-1][j].balance);
    }
    printf("\n");
    printf("%s \n", hline);
}
//*/

timestamp_t get_lamport_time(){
  return process.lampic_time;
}

void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount)
{
    // student, please implement me
    Process* provider_process = (Process* ) parent_data;
    //local_timer++;
    process.lampic_time++;

	Message sending_message;

	TransferOrder* order = (TransferOrder*) &sending_message.s_payload;
	order->s_amount = amount;
	order->s_dst = dst;
	order->s_src = src;

	MessageHeader* message_header = (MessageHeader* ) &sending_message.s_header;
	message_header->s_payload_len = sizeof(TransferOrder);
	message_header->s_local_time = get_lamport_time();
	message_header->s_magic = MESSAGE_MAGIC;
	message_header->s_type = TRANSFER;

	send(provider_process, src, &sending_message);

	Message calli_back;

	while(my_receive(provider_process, dst, &calli_back) != 0){
		
	}
	push_lamper_time(calli_back.s_header.s_local_time);
	///---printf("Parent got a callback_2 from %d.\n", dst);

}


void child_job_2(){
	local_timer = 0;

	init_hisltory();
	// Create BalanceHistory Object
    BalanceHistory balance_history;
    balance_history.s_history_len = 0;
    balance_history.s_id = process.my_id;
    
    BalanceState balance_start_state;
    balance_start_state.s_time = get_lamport_time();
    balance_start_state.s_balance = balances[process.my_id];
    balance_start_state.s_balance_pending_in = 0;
    
    // Add state to balance history
    memcpy(&balance_history.s_history[balance_history.s_history_len], &balance_start_state, sizeof(BalanceState));
    balance_history.s_history_len++;

    	
	// Sending STARTED message to every one
	wave_send_starteds();
	///---printf(" Child : %d   send STARTED to everyone \n", process.my_id);
	
	// Recived STARTED from everyone 
	wave_receive_starteds();
	///---printf(" Child : %d   got STARTED from everyone \n", process.my_id);
	//sleep(1);f
	
	///*
	
	///---printf(" -------- Child : %d    start wainting KILL_Message ( STOP ) \n", process.my_id);
	int kill_flag = process.brothers_number;
	while(kill_flag > 1){
		Message command_message;
		if(my_receive_any(&process, &command_message) == 0){
			///---printf(" Child : %d   got  message :  %s \n", process.my_id, command_message.s_payload);
			push_lamper_time(command_message.s_header.s_local_time);
			if(command_message.s_header.s_type == STOP){
				///---printf(" Child : %d   got KILL_Message message \n", process.my_id);
				if(process.lampic_time > command_message.s_header.s_local_time){
					process.lampic_time = command_message.s_header.s_local_time;
				}
				wave_done_to_everyone();
				break;
			}
			else if(command_message.s_header.s_type == DONE){
				if(process.lampic_time > command_message.s_header.s_local_time){
					process.lampic_time = command_message.s_header.s_local_time;
				}				
				kill_flag--;
			}
			else if(command_message.s_header.s_type == TRANSFER){
				TransferOrder *transfer = (TransferOrder *) &command_message.s_payload;
				printf(" Child : %d   got TRANSFERRRRRRRRRRRRRRRRRRRR message.  src = %d  dst = %d  amount = %d   cur_balance = %d \n", process.my_id, transfer->s_src, transfer->s_dst, transfer->s_amount, balances[process.my_id]);
				BalanceState cur_balance_state;
				cur_balance_state.s_balance_pending_in = 0;
				timestamp_t sender_time = command_message.s_header.s_local_time;
				if(transfer->s_src == process.my_id){
					printf(" Child : %d   got TRANSFER message.  src = %d  dst = %d  amount = %d   cur_balance = %d \n", process.my_id, transfer->s_src, transfer->s_dst, transfer->s_amount, balances[process.my_id]);
					push_lamper_time(command_message.s_header.s_local_time);
					
					for(timestamp_t t = get_lamport_time(); t <= MAX_T; t++){
						process.local_history.s_history[t].s_balance -= transfer->s_amount;
					}
					
					balances[process.my_id] -= transfer->s_amount;
					
					cur_balance_state.s_balance_pending_in = transfer->s_amount;
					
					int x = transfer->s_dst;
					
					command_message.s_header.s_local_time = get_lamport_time();					
					send(&process, x, &command_message);

					printf(" Child : %d   sendt TRANSFER message to DST src = %d  dst = %d  amount = %d \n", process.my_id, x, transfer->s_dst, transfer->s_amount);
				}
				else {
					printf(" -------------Child : %d   got TRANSFER message.  src = %d  dst = %d  amount = %d   cur_balance = %d \n", process.my_id, transfer->s_src, transfer->s_dst, transfer->s_amount, balances[process.my_id]);
					push_lamper_time(sender_time);
					
					for(timestamp_t t = sender_time; t < get_lamport_time(); t++){
						process.local_history
							.s_history[t]
							.s_balance_pending_in += transfer->s_amount;
					}

					balances[process.my_id] += transfer->s_amount;
					
					for(timestamp_t t = get_lamport_time(); t <= MAX_T; t++){
						process.local_history
							.s_history[t]
							.s_balance += transfer->s_amount;
					}
					
					Message ack_message;
					MessageHeader* ack_message_header = (MessageHeader* ) &ack_message.s_header;
					ack_message_header->s_payload_len = sizeof(TransferOrder);
					ack_message_header->s_local_time = get_lamport_time();
					ack_message_header->s_magic = MESSAGE_MAGIC;
					ack_message_header->s_type = ACK;
					cur_balance_state.s_balance_pending_in = 0;

					send(&process, 0, &ack_message);
					
					printf(" Child : %d   sendt TRANSFER finish message to Parent \n", process.my_id);
				}
				
				// Add balance state
				//local_timer++;
				//cur_balance_state.s_balance_pending_in = 0;
				cur_balance_state.s_time = get_lamport_time();
				cur_balance_state.s_balance = balances[process.my_id];
				
				for(timestamp_t i = balance_history.s_history[balance_history.s_history_len - 1].s_time; i < get_lamport_time() - 1; i++){
					memcpy(&balance_history.s_history[balance_history.s_history_len], 
							&balance_history.s_history[balance_history.s_history_len - 1], 
							sizeof(BalanceState));
					balance_history.s_history[balance_history.s_history_len].s_time = 
										balance_history.s_history[balance_history.s_history_len - 1].s_time + 1;
					///---printf(" <<<<<<<<<<<<<<<<<<<<<<<<<< ##### BALANCE  Process_id = %d   len-1.pending = %d   len.pending = %d \n", 
					///---	process.my_id, 
					///---	balance_history.s_history[balance_history.s_history_len - 1].s_balance_pending_in, 
					///---	balance_history.s_history[balance_history.s_history_len].s_balance_pending_in);
					balance_history.s_history[balance_history.s_history_len].s_balance_pending_in = 0;
					balance_history.s_history_len++;
				}
				
				memcpy(&balance_history.s_history[balance_history.s_history_len], &cur_balance_state, sizeof(BalanceState));
				///---printf(" <<<<<<<<<<<<<<<<<<<<<<<<<< ------ finish BALANCE  Process_id = %d   len-1.pending = %d   len.pending = %d \n", 
				///---	process.my_id, 
				///---	balance_history.s_history[balance_history.s_history_len - 1].s_balance_pending_in, 
				///---	balance_history.s_history[balance_history.s_history_len].s_balance_pending_in);
				balance_history.s_history_len++;
				
				///---printf(" >>>>>>>>>>>>>>>>>>>>>>>>>> ##### BALANCE  Process_id = %d   --- CUR_BALANVE STATE ---- balance = %d      time = %d \n", process.my_id,
				///---	cur_balance_state.s_balance,
				///---	cur_balance_state.s_time);
				///---printf(" >>>>>>>>>>>>>>>>>>>>>>>>>> ##### BALANCE  Process_id = %d --- balance = %d      time = %d \n", process.my_id, 
				///---	balance_history.s_history[balance_history.s_history_len - 1].s_balance, 
				///---	balance_history.s_history[balance_history.s_history_len - 1].s_time);

			}
		}		
	}
	///---printf(" -------- Child : %d    got  KILL mesaage \n", process.my_id);

	//Sending BALANCE_HISTORY to Parent -------------------------------
	process.lampic_time++;
	process.local_history.s_history_len = get_lamport_time() + 1;
	int len = process.local_history.s_history_len * sizeof(BalanceState) 
				+ sizeof(process.local_history.s_id) 
				+ sizeof(process.local_history.s_history_len);
	Message report_balance_history;
	MessageHeader report_balance_history_header;
	report_balance_history_header.s_local_time = get_lamport_time();
	report_balance_history_header.s_payload_len = len;
	report_balance_history_header.s_type = BALANCE_HISTORY;
	report_balance_history_header.s_magic = MESSAGE_MAGIC;
	
	report_balance_history.s_header = report_balance_history_header;
	memcpy(&report_balance_history.s_payload, &process.local_history, len);
	//  report_balance_history_debug_print(p, balanceHistory);
	send(&process, 0, &report_balance_history);	

	printf(" -------- Child : %d    send HISTORY REPORT message to Parent \n", process.my_id);


	//*/
}

void parent_job_2(){
	// Start sending STARTED to everyone
	
	// Start receiving STARTED from everyone
	wave_receive_starteds();
	///---printf("   Parent got STARTED from everyone \n");


	//transfer(&process, 1, 2, 5);
	bank_robbery(&process, process.brothers_number);
	//bank_robbery(&process, process.brothers_number);
	//sleep(1);
	///---printf(" ------------ Parent    TRANSFER  done ------------------------ \n");
	
	// Sending KILL message ( STOP ) to everyone
	kill_wave();
	
	// Start receiving STARTED from everyone
	done_wave_receive();
	//sleep(1);

	// Collect All History
	///---printf("   Parent start collecting HISTORY REPORT \n");
	AllHistory total_history;
	total_history.s_history_len = process.brothers_number;
	for(int i = 1; i <= process.brothers_number; i++){
		///---printf(" -------- Parent : waiting HISTORY REPORT from Child=%d \n", i);

		Message history_report;
		while(my_receive(&process, i, &history_report) != 0);

		///---printf(" -------- Parent : got HISTORY REPORT from Child=%d \n", i);
		BalanceHistory *child_history = (BalanceHistory*) &history_report.s_payload;
		total_history.s_history[i - 1] = *child_history;
		push_lamper_time(history_report.s_header.s_local_time);
	}
	///---printf(" Parent got HISTORY REPORT from ALL Childs \n");
	
	for(int i = 1; i <= brothers_number; i++){
		int stati;
		//printf("Parent %d :: check Childe %d  \n", process.my_id, process.my_network[i]->pid);
		while(waitpid(process.my_network[i]->pid, &stati, WNOHANG) <= 0){
			//printf("Parent %d :: Childe %d is alive \n", process.my_id, process.my_network[i]->pid);
		}
	}
	printf("  Parent saying : ALL Childs sleptd \n");

	
	print_history(&total_history);

	

}

void create_childes_2(int n){
	process.my_id = 0;
	process.my_pid = getpid();
	
	for(int i = 1; i <= n; i++){
		if(status == Im_Parent){
			pid_t id = forik();
			if(status == Im_Parent){
				///---printf("  Parent :: --> I created child pid = %d inner_id = %d \n\n ", id, i);
				process.my_network[i]->pid = id;
			} 
			else if(status == Im_Childe){
				process.my_id = i;
				process.my_pid = getpid();
				process.my_status = status;
				///---printf("  Childe %d :: + I'm new childe. My pid = %d inner_id = %d \n\n", process.my_id, process.my_pid, process.my_id);
				break;
			}
			else if(status == Im_Error){
				printf("  Parent :: ?? Error in childe creating");
				return ;
			}
		}
	}
	
	for(int i = 0; i <= n; i++){
		if(i != process.my_id){
			process.my_network[i]->read = pipes[i][process.my_id][0];
			process.my_network[i]->write = pipes[process.my_id][i][1];
			
			pipes[i][process.my_id][0] = -1;
			pipes[process.my_id][i][1] = -1;
			//printf("  Process %d :: comunication with process = %d reading = %d writing = %d \n", process.my_id, i, process.my_network[i]->read, process.my_network[i]->write);
		}
	}
	for(int i = 0; i <= n; i++){
		for(int j = 0; j <= n; j++){
			if(i != j){
				if(pipes[i][j][0] > 0){
					close(pipes[i][j][0]);
				}
				if(pipes[i][j][1] > 0){
					close(pipes[i][j][1]);
				}
			}
		}
	}
	printf("  Process %d :: Finish network initing \n", process.my_id);
}

int main(int argc, char *argv[]){
	
	//event_log_file = open(events_log, O_WRONLY | O_APPEND); 
    //pipe_log_file = open(pipes_log, O_WRONLY | O_APPEND);
    event_log_file = fopen(events_log, "a");
    pipe_log_file = fopen(pipes_log, "w");
		
	brothers_number = 0;
	if((argc >= 4) && (strcmp("-p", argv[1])) == 0){
		brothers_number = atoi(argv[2]);
	}
	printf( "  number of brothers +++++++++++++++++++++++++++++++++++++++ %d \n", brothers_number);
	fprintf(event_log_file, "  number of brothers +++++++++++++++++++++++++++++++++++++++ %d \n", brothers_number);
	for(int i = 1; i <= brothers_number; i++){
		balances[i] = atoi(argv[2 + i]);
		printf("   balance i = %d    == %d \n", i, balances[i]);
	}
	process.brothers_number = brothers_number;
	process.lampic_time = 0;
	printf("%d \n", brothers_number);
	my_inner_id = 0;
	my_pid = getpid();
	if(create_pipes() == -1){
		return 0;
	} else {
		printf("Parent :: I created pipes for conwersation \n ");
		//char mess[] = "Parent :: I created pipes for conwersation \n";
		//write(event_log_file, mess, strlen(mess));
	}
	
	printf("Parent :: I'm going to create some childs. My pid = %d id = %d \n", my_pid, my_inner_id);
	status = Im_Parent;
	
	create_childes_2(brothers_number);
	
	if(process.my_status == Im_Parent){
		parent_job_2();
		printf(" Parent done \n");
	} else {
		child_job_2();
		printf(" Child %d done \n", process.my_id);
	}

	
	my_pid = getpid();
	printf(" Go bying id = %d\n", process.my_id);
	if(status == Im_Parent){
		printf("Parent %d :: Bye %d \n", process.my_id, process.my_pid);
	}
	else if(status == Im_Childe){
		printf("Child %d :: Bye %d \n", process.my_id, process.my_pid);
	}
	return EXIT_SUCCESS;
}




/*



export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:~/itmo/Rasp/lab-2/work_spacer";
### пустая строка
LD_PRELOAD=~/itmo/Rasp/lab-2/work_space/libruntime.so ./pa2 –p 2 10 20


*/
