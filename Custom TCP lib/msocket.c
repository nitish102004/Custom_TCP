#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include "msocket.h"

#define SEM1_SIGNAL 1
#define SEM2_SIGNAL 2

sem_t *Socket_sem;
sem_t *Sem1;
sem_t *Sem2;
sem_t *Var_up;
SOCK_INFO* sock_info;
SharedMemorySegment* sm;
sem_t* safe_sem_open(const char* name, unsigned int initial_value) {
    sem_t* sem = sem_open(name, O_CREAT, 0666, initial_value);
    if (sem == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }
    return sem;
}

void initialize_all_variables(){
    Sem1 = safe_sem_open("/sem1", 0);
    Sem2 = safe_sem_open("/sem2", 0);
    Socket_sem = safe_sem_open("/semS", 1);
    Var_up = safe_sem_open("/semV", 0);

    key_t id1 = ftok("/home/", 'A');
    int shmid = shmget(id1,sizeof(SOCK_INFO),IPC_CREAT | 0666);
    sock_info = shmat(shmid,NULL,0);

    key_t id2 = ftok("/home/", 'B');
    int shmid2 = shmget(id2,sizeof(SharedMemorySegment),IPC_CREAT | 0666);
    sm = shmat(shmid2,NULL,0);
}

int m_socket(int domain, int type, int flags) {

/*********************************************************************************************************/
    int dum;
    printf("Press any character to proceddd for socket creation. So that two process don't create socket at the same time\n");
    scanf("%d",&dum);


    sem_post(Sem1);
    sem_wait(Sem2);
    int udp_socket = sock_info->sock_id;
    sock_info->sock_id =0 ;
    sem_post(Var_up);

    printf("UDP_SOCKET ID is %d\n",udp_socket);

/*********************************************************************************************************/
    int slot_found = -1;
    // Acquire semaphore before accessing shared memory
    sem_wait(Socket_sem);

    for (int i = 0; i < MX_SOCKETS; i++) {
        if (!sm->sockets[i].is_allocated) {
            slot_found = i;
            break;
        }
    }

    // Release semaphore after accessing shared memory
    sem_post(Socket_sem);

    if (slot_found == -1) {
        errno = ENOBUFS;
        return -1;
    }

    sem_wait(Socket_sem);

    sm->sockets[slot_found].is_allocated = 1;
    sm->sockets[slot_found].udp_socket_id = udp_socket;
    sm->sockets[slot_found].process_id = getpid();
    sm->sockets[slot_found].swnd.size = 5;
    sm->sockets[slot_found].swnd.start_seq_num = 0;
    sm->sockets[slot_found].swnd.next_seq_num = 0;
    sm->sockets[slot_found].swnd.w_s = 0;
    sm->sockets[slot_found].swnd.w_e = 0; // update

    sem_post(Socket_sem);


    return slot_found; 
}



//?????????????????????????????????????????????????????????????????????????????????????????????????????????????????
int m_bind(int sock_des, char *src_ip, int src_port, char *des_ip, int des_port) {
    int dum;
    printf("Press any character to proceddd for socket binding. So that two socket don't bind at the same time\n");
    scanf("%d",&dum);

    int udp_socket = sm->sockets[sock_des].udp_socket_id;
    sock_info->sock_id = udp_socket;
    sock_info->IP = src_ip;
    sock_info->port = src_port;
    sem_post(Sem1);
    sem_wait(Sem2);
    sock_info->sock_id = 0;
    sem_post(Var_up);
    

    struct sockaddr_in *des_addr = &sm->sockets[sock_des].other_end_addr;
    des_addr->sin_addr.s_addr = inet_addr(des_ip);
    des_addr->sin_port = htons(des_port);
    des_addr->sin_family = AF_INET;

    // Reset SOCK_INFO fields

   return 0;
   }

/*************************************************************************************************************/
//Needs to non-blockgin because we are actually not sending the data.
int m_sendto(int mtp_sock, char* message, size_t message_len){
    // Handle error for invalid arguments...
    if(mtp_sock < 0 || mtp_sock >= MX_SOCKETS || message == NULL || message_len <= 0 || message_len > sizeof(((MTPPacket*)0)->data)) {
        errno = EINVAL;
        return -1;
    }

    MTPSocketInfo *socketinfo;

    // Acquire semaphore before accessing shared memory
    sem_wait(Socket_sem);

    socketinfo = &sm->sockets[mtp_sock];
    if(socketinfo->is_allocated == 0){
        errno = EBADF;
        sem_post(Socket_sem);
        return -1;
    }

    // Release semaphore after accessing shared memory
    /**************************************************UPDATED********************************************************(included other condition)*/
    if((socketinfo->swnd.next_seq_num + 1)%MX_MSGS == socketinfo->swnd.start_seq_num){
        // printf("Message: %s,Next_Seq_Num=%d, Start_Seq_Num=%d\nERR: NO buffer space\n",message, socketinfo->swnd.next_seq_num,socketinfo->swnd.start_seq_num);
        sem_post(Socket_sem);
        return -1;
    }

    sem_post(Socket_sem);


    /*Have to check whether the buffer is full or not   */    


    

    sem_wait(Socket_sem);

    int index = socketinfo->swnd.next_seq_num;

    // Copy the message to the send buffer at the calculated index
    int ct=0;
    while(message_len > ct){
        if(socketinfo->swnd.next_seq_num + 1 == socketinfo->swnd.start_seq_num){
            // perror("NO buffer space\n");
            return -1;
        }

        if(message_len - ct> 1022){
            strncpy(socketinfo->send_buffer[index%MX_MSGS], message + ct, 1022);
            socketinfo->send_buffer[index%MX_MSGS][1022] = '\0'; // Ensure null-termination
            ct += 1022;
        }else{
            strncpy(socketinfo->send_buffer[index%MX_MSGS], message + ct, message_len - ct);
            socketinfo->send_buffer[index%MX_MSGS][message_len - ct] = '\0';
            ct = message_len;
        }
        index = (index + 1) % MX_MSGS;
        socketinfo->swnd.acked[index] = 0; 
        socketinfo->swnd.next_seq_num = index;
    }
    
    // printf("Message: %s (Stored in send Buffer) \n",message);
    sem_post(Socket_sem);
    
    return 1;
}
/*************************************************************************************************************/

int m_recvfrom(int mtp_sock, char* buffer, size_t buffer_len) {
    if(mtp_sock < 0 || mtp_sock >= MX_SOCKETS || buffer == NULL || buffer_len == 0) {
        errno = EINVAL;
        return -1;
    }

    MTPSocketInfo *socketinfo;

    // Acquire semaphore before accessing shared memory
    sem_wait(Socket_sem);

    socketinfo = &sm->sockets[mtp_sock];
    if(socketinfo->is_allocated == 0){
        errno = EBADF;
        return -1;
    }
    // printf("is allcoated\n");
    sem_post(Socket_sem);

    sem_wait(Socket_sem);
    if(socketinfo->rwnd.received[0] == 0) {
        errno = EAGAIN; // No in-order message available
        sem_post(Socket_sem);
        return -1;
    }

    
    strncpy(buffer, socketinfo->recv_buffer[0], buffer_len);
    buffer[buffer_len-1] = '\0'; // Ensure null-termination
    sem_post(Socket_sem);

    // Mark the message as processed
    // socketinfo->rwnd.received[socketinfo->rwnd.read_seq_num] = 0;
    // socketinfo->rwnd.read_seq_num = (socketinfo->rwnd.read_seq_num + 1) % MX_MSGS;
    sem_wait(Socket_sem);
    /******************************UPDATE********************8*/
    for(int i = 1; i < 5; i++) {
        strncpy(socketinfo->recv_buffer[i - 1], socketinfo->recv_buffer[i], sizeof(socketinfo->recv_buffer[i - 1]) - 1);
        socketinfo->recv_buffer[i - 1][1023] = '\0'; // Ensure null-termination for each message
        socketinfo->rwnd.received[i - 1] = socketinfo->rwnd.received[i];
    }

    // Clear the last message slot and its received status
    socketinfo->recv_buffer[socketinfo->rwnd.size - 1][0] = '\0';
    socketinfo->rwnd.received[socketinfo->rwnd.size - 1] = 0;
    socketinfo->rwnd.next_ind--;
    if(socketinfo->rwnd.size > 0) {
        socketinfo->rwnd.size--;
    }
    
    
    // Release semaphore after accessing shared memory
    
    sem_post(Socket_sem);


    return strlen(buffer);
}


/*************************************************************************************************************/
int m_close(int mtpsocket){
    MTPSocketInfo * socketinfo;

    // Acquire semaphore before accessing shared memory
    sem_wait(Socket_sem);

    socketinfo = &sm->sockets[mtpsocket];
    socketinfo->is_allocated = 0;

    // int udp_socket_id = socketinfo->udp_socket_id;

    sem_post(Socket_sem);

    return 0;
}
/******************************************************************************************************************************/
