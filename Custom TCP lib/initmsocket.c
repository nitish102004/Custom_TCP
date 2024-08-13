#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "msocket.h"

#define SEM1_SIGNAL 1
#define SEM2_SIGNAL 2

sem_t *Socket_sem;
sem_t *Sem1;
sem_t *Sem2;
sem_t *sem3;
sem_t *Var_up;
int atempt_count =0;
SharedMemorySegment* sm;
// Assume SOCK_INFO structure is available globally
SOCK_INFO* sock_info;

int drop_message();

void *G_thread(void* arg) {

    while (1) {
        // Acquire semaphore before accessing shared memory
        sleep(T);
        sem_wait(Socket_sem);

        for (int i = 0; i < MX_SOCKETS; i++) {
            if (sm->sockets[i].is_allocated && sm->sockets[i].process_id != 0) {
                int result = kill(sm->sockets[i].process_id, 0);
                if (result == -1 && errno == ESRCH) {
                    sm->sockets[i].is_allocated = 0;
                    close(sm->sockets[i].udp_socket_id);
                    sm->sockets[i].udp_socket_id = -1;
                    printf("G_THREAD: Garbage collector cleaned up socket %d\n", i);
                }
            }
        }
        
        // Release semaphore after accessing shared memory
        sem_post(Socket_sem);
        
    }

    return NULL;
}

void *R_thread(void *arg){
    fd_set read_fds;
    struct timeval timer;
    int max_fd = 0;
    
    char buffer[1024];
    //Dont knwo
    ////////

    while(1){
        FD_ZERO(&read_fds);
        max_fd =0 ;

        struct sockaddr_in sender_addr;
        socklen_t snd_addr_len = sizeof(sender_addr);
        
        sem_wait(Socket_sem);
        for(int i=0;i< MX_SOCKETS;i++){
            if(sm->sockets[i].is_allocated){
                printf("R_THREAD: Listening on Socket %d\n",i);
                FD_SET(sm->sockets[i].udp_socket_id,&read_fds);
                if(sm->sockets[i].udp_socket_id > max_fd){
                    max_fd = sm->sockets[i].udp_socket_id;
                }
            }
        }
        sem_post(Socket_sem);

        timer.tv_sec = T; // confirm once whether it is 5 or not
        timer.tv_usec = 0;

        int retval = select(max_fd + 1, &read_fds, NULL, NULL,&timer);

        if(retval == 0)continue;
        else{
            for(int i=0;i<MX_SOCKETS;i++){
                if(sm->sockets[i].is_allocated && FD_ISSET(sm->sockets[i].udp_socket_id,&read_fds)){
                    MTPPacket packet;
                    int udp_socket = sm->sockets[i].udp_socket_id;
                    ssize_t recv_len = recvfrom(udp_socket,&packet,sizeof(packet),0,(struct sockaddr*)&sender_addr,&snd_addr_len);
                    if(recv_len < 0){
                        perror("recvfrom");
                        continue;
                    }
                    atempt_count++;
                    if(drop_message() == 1){printf("R_THREAD: Message is dropped of Type: %d {0:ACK, 1:MSG}, sequence number = %d \n",packet.type,packet.seq_num);continue;}
                    if(packet.type == MSG) {
                        printf("R_THREAD: Message: %s,  Message seq_num: %d, Expected Seq_num= %d\n",packet.data, packet.seq_num,sm->sockets[i].rwnd.expect_seq_num);
                        if(packet.seq_num == sm->sockets[i].rwnd.expect_seq_num) {
                            printf("R_THREAD: Received in-order message: seq_num %d\n", packet.seq_num);
                            sem_wait(Socket_sem);
                            strncpy(sm->sockets[i].recv_buffer[sm->sockets[i].rwnd.next_ind], packet.data, 1024);
                            // printf("index : %d, data : %s\n",sm->sockets[i].rwnd.next_ind ,sm->sockets[i].recv_buffer[sm->sockets[i].rwnd.next_ind]);
                            // Update rwnd to reflect the receipt
                            sm->sockets[i].rwnd.received[sm->sockets[i].rwnd.next_ind] = 1;
                            sm->sockets[i].rwnd.next_ind++;
                            sm->sockets[i].rwnd.size++;
                            /**************UPDATE***************************/
                            // int ct=1;
                            // for(int k=0;k<4;k++){
                            //     if(sm->sockets[i].rwnd.received[(sm->sockets[i].rwnd.next_ind + ct)% MX_RECV_BUFFER] == 0){
                            //         break;
                            //     }
                            //     ct++;
                            // }ct %= MX_RECV_BUFFER;
                            // sm->sockets[i].rwnd.next_ind = (sm->sockets[i].rwnd.next_ind + (ct))%MX_RECV_BUFFER; // Updated   
                            sm->sockets[i].rwnd.expect_seq_num = (sm->sockets[i].rwnd.expect_seq_num + 1) % MX_MSGS; // UPDATED 1 ---> ct.
                            sem_post(Socket_sem);

                            // Send ACK back
                            MTPPacket ack_pack;
                            ack_pack.type = ACK;
                            ack_pack.r_buff_size = MX_RECV_BUFFER - sm->sockets[i].rwnd.size;
                            ack_pack.seq_num = (sm->sockets[i].rwnd.expect_seq_num - 1 + MX_MSGS)%MX_MSGS;
                            // printf("R: curr_index= %d, expected_seq_num=%d, Buff_size =%d\n",sm->sockets[i].rwnd.next_ind,sm->sockets[i].rwnd.expect_seq_num, MX_RECV_BUFFER -  sm->sockets[i].rwnd.size);
                            sendto(sm->sockets[i].udp_socket_id, &ack_pack, sizeof(ack_pack), 0, (struct sockaddr*)&sender_addr, snd_addr_len);

                        } else {
                            printf("R_THREAD: Received out-of-order or duplicate message.\n");
                            int seq_num = packet.seq_num;
                            int is_duplicate = 0;
                            sem_wait(Socket_sem);
                            int window_end = (sm->sockets[i].rwnd.expect_seq_num + 5 - 1) % MX_MSGS; // Adjust for window size

                            // Check if seq_num falls within the expected range
                            if (sm->sockets[i].rwnd.expect_seq_num <= window_end) {
                                // No wrap-around
                                if (seq_num < sm->sockets[i].rwnd.expect_seq_num || seq_num > window_end) {
                                    is_duplicate = 1;
                                }
                            } else {
                                // With wrap-around
                                if (seq_num < sm->sockets[i].rwnd.expect_seq_num && seq_num > window_end) {
                                    is_duplicate = 1;
                                }
                            }
                            sem_post(Socket_sem);
                            if(is_duplicate == 1){
                                printf("R_THREAD: Dublicate Message(%s) ,sequence number: %d Received\n",packet.data, seq_num);
                            }else{
                                //How to store the message in the correct index of the recv buffer
                                sem_wait(Socket_sem);

                                int offset = (seq_num - sm->sockets[i].rwnd.expect_seq_num + MX_MSGS) % MX_MSGS;
                                int store_index = (sm->sockets[i].rwnd.next_ind + offset) % MX_RECV_BUFFER; 

                                if(!sm->sockets[i].rwnd.received[store_index]){
                                    strncpy(sm->sockets[i].recv_buffer[store_index], packet.data, sizeof(sm->sockets[i].recv_buffer[store_index]));
                                    sm->sockets[i].rwnd.received[store_index] = 1;

                                    printf("R_THREAD: Stored out of order message(%s), sequence number: %d at index=%d\n",packet.data, packet.seq_num,store_index);
                                }else{
                                    printf("R_THREAD: Buffer size full or Message is already at that index\n");
                                }
                                sem_post(Socket_sem);

                            }
                            /************UPDATE*************************/
                            MTPPacket ack_pack;
                            ack_pack.type = ACK;
                            ack_pack.r_buff_size = MX_RECV_BUFFER - sm->sockets[i].rwnd.size;
                            ack_pack.seq_num = (sm->sockets[i].rwnd.expect_seq_num - 1 + MX_MSGS)%MX_MSGS;
                            sendto(sm->sockets[i].udp_socket_id, &ack_pack, sizeof(ack_pack), 0, (struct sockaddr*)&sender_addr, snd_addr_len);
                            
                        }
                    }
                    else{
                        int ack_seq_num = packet.seq_num;
                        sem_wait(Socket_sem);
                        printf("R_THREAD: Received ACK seq_num: %d, Reciver buffer size = %d\n", ack_seq_num,packet.r_buff_size);

                        int found =0;
                        int curr_seq_num = sm->sockets[i].swnd.start_seq_num;
                        for(int j = 0;j < MX_MSGS;j++){
                            int seq_ind = (curr_seq_num + j )%MX_MSGS;
                            if(seq_ind == sm->sockets[i].swnd.next_seq_num){
                                break;
                            }
                            if(seq_ind == ack_seq_num){
                                found = 1;
                                break;
                            }
                        }
                        sm->sockets[i].swnd.size = packet.r_buff_size;
                        if(found == 1){
                            int tt = sm->sockets[i].swnd.w_e;
                            if(tt < sm->sockets[i].swnd.start_seq_num){
                                tt += MX_MSGS;
                            }
                            for(int j = sm->sockets[i].swnd.start_seq_num;j < tt; j = (j + 1) % MX_MSGS) {
                                sm->sockets[i].swnd.acked[j] = 1;
                                sm->sockets[i].swnd.start_seq_num = (sm->sockets[i].swnd.start_seq_num + 1) % MX_MSGS;
                                sm->sockets[i].swnd.w_s = (sm->sockets[i].swnd.w_s + 1) % MX_MSGS;
                                printf("R_THREAD: Message Seq Num: %d marked as received\n",j);
                                if(j == ack_seq_num) {
                                    break;
                                }
                            }
                        }
                        sem_post(Socket_sem);
                    }

                }
            }
        }

    }

}

void *S_thread(void *arg) {
    while (1) {
        // Sleep for a time less than T/2
        sleep(T/2); 
        
        // Acquire semaphore before accessing shared memory
        sem_wait(Socket_sem);
        
        for (int i = 0; i < MX_SOCKETS; i++) {
            MTPSocketInfo *socket_info = &sm->sockets[i];

            if (sm->sockets[i].is_allocated) {
                time_t current_time;
                time(&current_time);
             

                for (int j = socket_info->swnd.w_s; j != socket_info->swnd.w_e; j = (j + 1) % MX_SEND_BUFFER) {
                    if (socket_info->swnd.acked[j] == 0) { // Check if the message was not acknowledged
                        double time_since_last_sent = difftime(current_time, socket_info->last_sent_time[j]);
                        if (time_since_last_sent >= T) {
                            // Prepare and retransmit the packet
                            MTPPacket packet = { .type = MSG, .seq_num = j };
                            strncpy(packet.data, socket_info->send_buffer[j % MX_MSGS], sizeof(packet.data));
                            printf("S_THREAD: Message(%s) Sequence_num: %d send after timeout\n",packet.data,packet.seq_num);
                            sendto(socket_info->udp_socket_id, &packet, sizeof(packet), 0,
                                   (struct sockaddr*)&(socket_info->other_end_addr), sizeof(struct sockaddr_in));
                            // Update the last sent time for this packet
                            time(&(socket_info->last_sent_time[j]));
                        }
                    }
                }



                // Check if there is a pending message from the sender-side message buffer that can be sent
                int window_space = (socket_info->swnd.w_e + MX_SEND_BUFFER - socket_info->swnd.w_s) % MX_SEND_BUFFER;
                if (window_space < socket_info->swnd.size) { // If there is space in the window
                    int new_messages_to_send = socket_info->swnd.size - window_space;
                    for (int k = 0; k < new_messages_to_send; k++) {
                        if (socket_info->swnd.next_seq_num != (socket_info->swnd.w_e + MX_SEND_BUFFER) % MX_SEND_BUFFER) {
                            
                            // Assuming the next message to send is correctly set up in the send_buffer[next_seq_num]
                            MTPPacket packet = { .type = MSG, .seq_num = socket_info->swnd.w_e};
                            strncpy(packet.data, socket_info->send_buffer[socket_info->swnd.w_e], sizeof(packet.data));
                            printf("S_THREAD: Message send with seq_num=%d, message = %s\n",packet.seq_num,packet.data);
                            sendto(socket_info->udp_socket_id, &packet, sizeof(packet), 0,
                                   (struct sockaddr*)&(socket_info->other_end_addr), sizeof(struct sockaddr_in));
                            // Update control variables
                            time(&(socket_info->last_sent_time[socket_info->swnd.w_e]));
                            socket_info->swnd.w_e = (socket_info->swnd.w_e + 1) % MX_SEND_BUFFER;
                            // socket_info->swnd.next_seq_num = (socket_info->swnd.next_seq_num + 1) % MX_SEND_BUFFER;
                        }else{
                            break;
                        }
                    }
                }
            }
        }
        
        // Release semaphore after accessing shared memory
        sem_post(Socket_sem);
    }
    return NULL;
}

void init_shared_memory_and_variable() {
    // Initialize MTP sockets
    Sem1 = sem_open("/sem1", O_CREAT | O_EXCL, 0777, 0);
    if (Sem1 == SEM_FAILED) {
        perror("sem_open Sem1 failed");
        sem_unlink("/sem1");
        Sem1 = sem_open("/sem1", O_CREAT | O_EXCL, 0777, 0);
    }

    Sem2 = sem_open("/sem2",O_CREAT | O_EXCL,0777, 0);
    if(Sem2 == SEM_FAILED){
        perror("sem_open Sem2 failed");
        sem_unlink("/sem2");
        Sem2 = sem_open("/sem2",O_CREAT | O_EXCL,0777, 0);
    }

    Socket_sem= sem_open("/semS",O_CREAT | O_EXCL,0777, 1);
    if(Socket_sem == SEM_FAILED){
        perror("sem_open Socket_sem failed");
        sem_unlink("/semS");
        Socket_sem = sem_open("/semS",O_CREAT | O_EXCL,0777, 1);
    }

    Var_up = sem_open("/semV",O_CREAT | O_EXCL,0777, 0);
    if(Var_up == SEM_FAILED){
        perror("sem_open Var_up failed");
        sem_unlink("/semV");
        Var_up = sem_open("/semV",O_CREAT | O_EXCL,0777, 0);
    }

    // memset(sock_info, 0, sizeof(SOCK_INFO));

    key_t id1 = ftok("/home/", 'A');
    int shmid = shmget(id1,sizeof(SOCK_INFO),IPC_CREAT | 0666);
    sock_info = shmat(shmid,NULL,0);
    sock_info->sock_id = 0;

    // memset(sm, 0, sizeof(SharedMemorySegment));

    key_t id2 = ftok("/home/", 'B');
    int shmid2 = shmget(id2,sizeof(SharedMemorySegment),IPC_CREAT | 0666);
    sm = shmat(shmid2,NULL,0);

    for (int i = 0; i < MX_SOCKETS; i++) {
        sm->sockets[i].is_allocated = 0;
        sm->sockets[i].udp_socket_id = -1;
        sm->sockets[i].process_id = 0;

        sm->sockets[i].swnd.start_seq_num =0;
        sm->sockets[i].swnd.next_seq_num = 0;

        sm->sockets[i].rwnd.read_seq_num = 0;
        sm->sockets[i].rwnd.expect_seq_num =0;
        for(int j=0;j<5;j++)sm->sockets[i].rwnd.received[j] = 0;// i= 16->5
        // sm->sockets[i].rwnd.
    }
}

int drop_message(){
    float randomf = (float)(rand())/(float)RAND_MAX;
    return randomf < P;
}

void cleanup_resources() {
    // Detach shared memory segments
    printf("Total attempt to send the message is %d\n",atempt_count);
    if (sock_info != (void*)-1) {
        shmdt(sock_info);
    }
    if (sm != (void*)-1) {
        shmdt(sm);
    }

    // Close and unlink semaphores
    if (Sem1 != SEM_FAILED) {
        sem_close(Sem1);
        sem_unlink("/sem1");
    }
    if (Sem2 != SEM_FAILED) {
        sem_close(Sem2);
        sem_unlink("/sem2");
    }
    if (Socket_sem != SEM_FAILED) {
        sem_close(Socket_sem);
        sem_unlink("/semS");
    }
    if (Var_up != SEM_FAILED) {
        sem_close(Var_up);
        sem_unlink("/semV");
    }

    exit(0);
}


int main() {
    srand(time(NULL));
    signal(SIGINT,cleanup_resources);
    // Initialize shared memory segment
    // SOCK_INFO *sock_info = (SOCK_INFO *)malloc(2*sizeof(SOCK_INFO));
    init_shared_memory_and_variable();


    pthread_t thread_R, thread_S,thread_G;
    pthread_create(&thread_R, NULL, R_thread, (void *)sm);
    pthread_create(&thread_S, NULL, S_thread, (void *)sm);
    pthread_create(&thread_G, NULL, G_thread, (void *)sm);


    while (1) {
        printf("WAITING\n");
        sem_wait(Sem1);
        // If all fields of SOCK_INFO are 0, it is a m_socket call
        printf("READY\n");
        if (sock_info->sock_id == 0 ) {
            sock_info->sock_id = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock_info->sock_id == -1) {
                sock_info->errno_val = errno;
            }
        } 
        // If sock_id, IP, and port are non-zero, it is a m_bind call
        else if (sock_info->sock_id != 0) {
            printf("BINDING\n");
            struct  sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons((sock_info->port));
            addr.sin_addr.s_addr = INADDR_ANY;
            int bind_result = bind(sock_info->sock_id,(struct sockaddr*)&addr,sizeof(addr));
            if (bind_result == -1) {
                printf("bind problem\n");
                sock_info->errno_val = errno;
                sock_info->sock_id = -1; // Reset sock_id
            }
        }

        // Signal on Sem2
        sem_post(Sem2);
        sem_wait(Var_up);

        // Go back to wait on Sem1
    }

    pthread_join(thread_R, NULL);
    pthread_join(thread_S, NULL);

    free(sm);
    free(sock_info);
    // Destroy semaphores
    sem_destroy(Socket_sem);
    sem_destroy(Sem1);
    sem_destroy(Sem2);

    return 0;
}
