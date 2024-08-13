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
// #include <cstdlib>
#include <time.h>
#include <semaphore.h>


#define ACK 0   // Message type
#define MSG 1
#define MX_SEND_BUFFER 10 // Sender buffer size
#define MX_RECV_BUFFER 5  // Receiver buffer size
#define MX_SOCKETS 25 // MAX number of sockets
#define T 5 // Timeout in seconds
#define P 0.0 // Drop probability
#define MX_MSGS 10 

typedef struct {
    int is_allocated; // 0 for not allocated, 1 for allocated
    pid_t process_id; // Id of the Process which requested the socket
    int udp_socket_id;  // UDP socket id
    struct sockaddr_in other_end_addr; // Address of the other end
    char send_buffer[MX_SEND_BUFFER][1024]; // Adjusted for sender buffer size
    char recv_buffer[MX_RECV_BUFFER][1024]; // Adjusted for receiver buffer size
    time_t last_sent_time[MX_SEND_BUFFER]; // Time when each packet was last sent

    struct {
        int size;
        int sequence_num[5]; // update
        int start_seq_num;//buffer index
        int next_seq_num; // Next sequence number(index) to be used
        int w_s; //index of window start
        int w_e; //index of window end
        int acked[MX_SEND_BUFFER]; // Acknowledgment status for each packet in the sender buffer
    } swnd;

    struct {
        int size; // size of buffer filled.
        int next_ind; //Next Free index in the receiver buffer
        int expect_seq_num; // Next expected sequence number for in-order receipt
        int read_seq_num; // Sequence number of the next message to be read by application
        int received[MX_RECV_BUFFER]; // Receipt status for each packet in the receiver buffer
        int recv_seq_num[MX_RECV_BUFFER]; //NOt needed but can be used to store the sequence number or receivec packets
    } rwnd;

} MTPSocketInfo;

typedef struct {
    MTPSocketInfo sockets[MX_SOCKETS];
} SharedMemorySegment;



typedef struct {
    int type; // 0 for message, 1 for ACK
    int seq_num; // seq_num of the message
    int r_buff_size; // Buffer size of receiver (used only in case of ACK)
    char data[1024 - sizeof(int) * 2]; 
} MTPPacket;

typedef struct{
    int sock_id;
    char *IP;
    int port;
    int errno_val;
}SOCK_INFO;                                                                         

int m_socket(int,int,int);
void initialize_all_variables();

int m_bind(int,char*, int,char*, int);
int m_sendto(int, char*, size_t);
int m_recvfrom(int, char*, size_t);