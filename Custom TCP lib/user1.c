#include "msocket.h"

int main(){
    initialize_all_variables();

    int socket = m_socket(1, 2, 3);
    printf("Socket: %d\n", socket);
    m_bind(socket,"127.0.0.1", 8084, "127.0.0.1", 8085);

    FILE *file = fopen("send.txt", "r");
    char buffer[1000];
    while(fgets(buffer, sizeof(buffer), file) != NULL) {
        buffer[sizeof(buffer)-1] = '\0'; // remove the newline character
        while(1) {
            int n = m_sendto(socket, buffer, strlen(buffer));
            if(n != -1) {
                break;
            }
        }
    }
    strcpy(buffer,"%");
    while(1) {
        int n = m_sendto(socket, buffer, strlen(buffer));
        if(n != -1) {
            break;
        }
    }



    // char buffer[10];
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
    // }
    
    // // printf("Received: %s\n", buffer);

    // char buff[10];
    // int count = 4;
    // while(count--){
    //     sprintf(buff,"NUM: %d",count);
    //     m_sendto(socket, buff, 10);
    // }

    /***********************************Multi socket processing**************************/
    // int sock2 = m_socket(1,2,3);
    // m_bind(sock2,"127.0.0.1", 8084, "127.0.0.1", 8085);
    // char buff2[30];
    // sprintf(buff2,"Sending over another socket");
    // m_sendto(sock2,buff,30);


    int dum;
    printf("To eixt the program press any character (Don't exit before reciving or sending all message because garbage collector will delte the socket\n");
    scanf("%d",&dum);
    return 0;
}