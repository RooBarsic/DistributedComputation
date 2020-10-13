#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>  //Для puts, printf, fprintf, fdopen
#include <fcntl.h>  //Для open
#include <fcntl.h>  //Для определения константы O_RDWR
#include "pa1.h"
#include "ipc.h"
#include "common.h"

#define Im_Childe 123
#define Im_Parent 0
#define Im_Error -1

int brothers_number = 0;
int id_counter = 0;
int status;
int my_inner_id;
pid_t my_pid;
int pipes[10][10][2];

int event_log_file;
int pipe_log_file;

struct Network{
	pid_t pid;
	int read;
	int write;
};

struct Process{
	int id;
	int status;
	pid_t pid;
	struct Network network[10][2];
} process;

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
	printf("  Parent :: started creating pipes \n ");
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
			printf(" pipes : [%d, %d] = [%d, %d] \n", i, j, pipes[i][j][0], pipes[i][j][1]); 
			printf(" pipes : [%d, %d] = [%d, %d] \n", j, i, pipes[j][i][0], pipes[j][i][1]); 
		}
	}
	return 0;
}

int read_from_pipe(int file){	
	char buffer[BUFSIZ + 1];
	memset(buffer, '.', sizeof(buffer));
	read(file, buffer, BUFSIZ);
	//close(file);
	
	//printf(" read data ::: %s \n", buffer);
	//sleep(1);
	return atoi(buffer);
}

void reverse(char s[]){
     int i, j;
     char c;
 
     for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
 }

void itoa(int n, char s[])
 {
     int i, sign;
 
     if ((sign = n) < 0)  /* записываем знак */
         n = -n;          /* делаем n положительным числом */
     i = 0;
     do {       /* генерируем цифры в обратном порядке */
         s[i++] = n % 10 + '0';   /* берем следующую цифру */
     } while ((n /= 10) > 0);     /* удаляем */
     if (sign < 0)
         s[i++] = '-';
     s[i] = '\0';
     reverse(s);
 }

void write_to_pipe(int file, int message){
	//int num_length = getNumberLength(message);
	char adcString[BUFSIZ];  // буфер, в которую запишем число
	itoa(message, adcString);
	write(file, &adcString, sizeof(adcString));
	//close(file);
	//sleep(1);
}

void do_childe_job(){
	printf(" Childe %d ::  starting doing childe job \n\n ", process.id);
	//printf(" Childe %d :: step 1 writing to %d \n ", my_inner_id, pipes[my_inner_id][0][1]);
	for(int i = 0; i <= brothers_number; i++){
		if(i != process.id){
			write_to_pipe(process.network[i]->write, process.id * 1000 + 11);
		}
	}
	printf(" Childe %d :: send all accept messages to brothers  \n ", process.id);

	for(int i = 0; i <= brothers_number; i++){
		if(i != process.id){
			//printf(" Childe %d :: trying reseive message from = %d \n\n ", my_inner_id, i);
			int message_fom_process_i = read_from_pipe(process.network[i]->read);
			printf(" Childe %d :: resived message-1 = %d from worker = %d \n\n ", process.id, message_fom_process_i, i);	
		}
	}
	printf(" Childe %d :: synchronization done  \n\n ", process.id);
	printf(" Childe %d :: start preporation for saying goodbye.  \n\n ", process.id);

	for(int i = 0; i <= brothers_number; i++){
		if(i != process.id){
			write_to_pipe(process.network[i]->write, process.id * 1000 + 66);
		}
	}
	printf(" Childe %d :: send all accept messages to brothers  \n ", process.id);

	for(int i = 0; i <= brothers_number; i++){
		if(i != process.id){
			int message_fom_process_i = read_from_pipe(process.network[i]->read);
			printf(" Childe %d :: resived message-2 = %d from worker = %d \n\n ", process.id, message_fom_process_i, i);	
		}
	}
	
	printf(" Childe %d :: access for goodbye are given  \n\n ", process.id);
}

int create_childes(){
	printf("  Parent :: started creating childrens \n ");
	for(int i = 1; i <= brothers_number; i++){
		if(status == Im_Parent){
			id_counter++;
			pid_t id = forik();
			if(status == Im_Parent){
				printf("  Parent :: --> I created child pid = %d inner_id = %d \n\n ", id, id_counter);
			} 
			else if(status == Im_Childe){
				my_inner_id = id_counter;
				my_pid = getpid();
				printf("  Childe %d :: + I'm new childe. My pid = %d inner_id = %d \n\n", my_inner_id, my_pid, my_inner_id);
				break;
			}
			else if(status == Im_Error){
				printf("  Parent :: ?? Error in childe creating");
				return -1;
			}
		}
	}
	return 0;
}


void create_childes_2(int n){
	process.id = 0;
	process.pid = getpid();
	
	for(int i = 1; i <= n; i++){
		if(status == Im_Parent){
			pid_t id = forik();
			if(status == Im_Parent){
				printf("  Parent :: --> I created child pid = %d inner_id = %d \n\n ", id, i);
				process.network[i]->pid = id;
			} 
			else if(status == Im_Childe){
				process.id = i;
				process.pid = getpid();
				process.status = status;
				printf("  Childe %d :: + I'm new childe. My pid = %d inner_id = %d \n\n", process.id, process.pid, process.id);
				break;
			}
			else if(status == Im_Error){
				printf("  Parent :: ?? Error in childe creating");
				return ;
			}
		}
	}
	
	for(int i = 0; i <= n; i++){
		if(i != process.id){
			process.network[i]->read = pipes[i][process.id][0];
			process.network[i]->write = pipes[process.id][i][1];
			
			pipes[i][process.id][0] = -1;
			pipes[process.id][i][1] = -1;
			printf("  Process %d :: comunication with process = %d reading = %d writing = %d \n", process.id, i, process.network[i]->read, process.network[i]->write);
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
	printf("  Process %d :: Finish network initing \n", process.id);
}

int main(int argc, char *argv[]){
	event_log_file = open(events_log, O_WRONLY | O_APPEND);
    pipe_log_file = open(pipes_log, O_WRONLY | O_APPEND);
	
	brothers_number = 0;
	if((argc == 3) && (strcmp("-p", argv[1])) == 0){
		brothers_number = atoi(argv[2]);
	}
	printf("%d \n", brothers_number);
	my_inner_id = 0;
	my_pid = getpid();
	if(create_pipes() == -1){
		return 0;
	} else {
		printf("Parent :: I created pipes for conwersation \n ");
		char mess[] = "Parent :: I created pipes for conwersation \n";
		write(event_log_file, mess, strlen(mess));
	}
	
	printf("Parent :: I'm going to create some childs. My pid = %d id = %d \n", my_pid, my_inner_id);
	status = Im_Parent;
	
	create_childes_2(brothers_number);
	
	if(status == Im_Parent){
		printf("Parent :: I created %d childs \n\n ", brothers_number);
		 //printf("Parent :: start messaging. writing to %d \n ", pipes[0][1][1]);
     	 //printf(" Parent :: step 1 . reading from = %d \n", pipes[1][0][0]);
		for(int i = 1; i <= brothers_number; i++){
			//open(process.network[i]->read);
			int message_from_child = read_from_pipe(process.network[i]->read);
			printf(" Parent :: resived message-1 = %d from childe %d \n\n ", message_from_child, i);
		}
		for(int i = 1; i <= brothers_number; i++){
 			 //open(process.network[i]->write);
			 write_to_pipe(process.network[i]->write, 111);
		}
		
		for(int i = 1; i <= brothers_number; i++){
			//open(process.network[i]->read);
			int message_from_child = read_from_pipe(process.network[i]->read);
			printf(" Parent :: resived message-2 = %d from childe %d \n\n ", message_from_child, i);
		}

		for(int i = 1; i <= brothers_number; i++){
 			//open(process.network[i]->write);
			write_to_pipe(process.network[i]->write, 555);
		}

				
		for(int i = 1; i <= brothers_number; i++){
			//open(process.network[i]->read);
			int message_from_child = read_from_pipe(process.network[i]->read);
			printf(" Parent :: resived message-3 = %d from childe %d \n\n ", message_from_child, i);
		}

	}
	
	if(status == Im_Childe){
		do_childe_job();
	}
	
	my_pid = getpid();
	if(status == Im_Parent){
		for(int i = 1; i <= brothers_number; i++){
			int stati;
			printf("Parent %d :: check Childe %d  \n", process.id, process.network[i]->pid);
			while(waitpid(process.network[i]->pid, &stati, WNOHANG) <= 0){
				printf("Parent %d :: Childe %d is alive \n", process.id, process.network[i]->pid);
			}
		}
		printf("Parent %d :: Bye %d \n", process.id, process.pid);
	}
	else if(status == Im_Childe){
		printf("Child %d :: Bye %d \n", process.id, process.pid);
		write_to_pipe(process.network[0]->write, process.id * 1000 + 99);
	}
	return EXIT_SUCCESS;
}




/*

  1 2 3
1
2
3

*/
