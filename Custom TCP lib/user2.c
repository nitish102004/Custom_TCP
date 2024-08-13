#include "msocket.h"

int main(){
    initialize_all_variables();

    int socket = m_socket(1, 2, 3);
    printf("Socket: %d\n", socket);
    m_bind(socket,"127.0.0.1", 8085, "127.0.0.1", 8084);

    FILE *file_w = fopen("recv.txt", "w");
    if(file_w == NULL){
        printf("Error in opening file\n");
        return 0;
    }

    char buffer[1022];
    int to_break =0;
    while(1){
        int n;
        while(1){
            n = m_recvfrom(socket, buffer, 1022);
            if(n != -1){
                buffer[n] = '\0'; 
                printf("%s",buffer);
                if(buffer[n-1] == '%'){
                    to_break = 1;
                    break;
                }
                fprintf(file_w, "%s", buffer);
                break;
            }
        }
        if(to_break == 1)break;
        
    }

    // m_sendto(socket, "Hello", 5);
    // m_sendto(socket, "I am Gautam,", 12);
    // m_sendto(socket, "and this is test message", 25);
    // m_sendto(socket, "123456",6);
    // m_sendto(socket,"67891011",8);
    // m_sendto(socket,"Want more test, here you go", 28);
    // m_sendto(socket,"123456",6);
    // int count = 2;
    // char buff[10];

    // while(count--){
    //     sprintf(buff,"NUM: %d",count);
    //     m_sendto(socket, buff, 10);
    // }


    // char buffer[100];
    // while(1){
    //     int n = m_recvfrom(socket, buffer, 100);
    //     if(n != -1){
    //         printf("Received: %s\n", buffer);
    //         break;
    //     }
    // }
    // while(1){
    //     int n = m_recvfrom(socket, buffer, 100);
    //     if(n != -1){
    //         printf("Received: %s\n", buffer);
    //         break;
    //     }
    // }
    // while(1){
    //     int n = m_recvfrom(socket, buffer, 100);
    //     if(n != -1){
    //         printf("Received: %s\n", buffer);
    //         break;
    //     }
    // }while(1){
    //     int n = m_recvfrom(socket, buffer, 100);
    //     if(n != -1){
    //         printf("Received: %s\n", buffer);
    //         break;
    //     }
    // }


    // while(1){
    //     int n = m_recvfrom(socket, buffer, 100);
    //     if(n != -1){
    //         printf("Received: %s\n", buffer);
    //         break;
    //     }
    // }

    int dum;
    printf("To eixt the program press any character (Don't exit before reciving or sending all message because garbage collector will delte the socket\n");
    scanf("%d",&dum);
    return 0;
}