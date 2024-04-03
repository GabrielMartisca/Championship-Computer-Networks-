#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8081
#define MAX_MESSAGE_SIZE 1024

int main()
{
    int clientSocket;
    struct sockaddr_in serverAddr;

    // Create a new socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server addr
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    // Connect to sv
    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Welcome message
    printf("Welcome! Please type 'login' if you have an account or 'register' otherwise!\n");

    char message[MAX_MESSAGE_SIZE];
    while (1)
    {
        // Get user input
        printf(">");
        if (fgets(message, sizeof(message), stdin) == NULL)
        {
            perror("Error in reading input");
            break;
        }

        // Remove the newline character
        size_t length = strlen(message);
        if (length > 0 && message[length - 1] == '\n')
        {
            message[length - 1] = '\0';
        }

        // Check if the user wants to exit
        if (strncmp(message, "exit", 4) == 0)
        {
            close(clientSocket);
            break; 
        }

        // Send the message to the server
        ssize_t bytesSent = send(clientSocket, message, strlen(message), 0);
        if (bytesSent == -1)
        {
            perror("Error in send");
            close(clientSocket);
            break;
        }

        // Clear buffer
        memset(message, 0, sizeof(message));

        // Receive the server's response and print it on terminal
        ssize_t bytesReceived = recv(clientSocket, message, sizeof(message) - 1, 0);
        if (bytesReceived <= 0)
        {
            perror("Error in recv");
            close(clientSocket);
            break;
        }
        message[bytesReceived] = '\0'; // Add null 
        printf("%s\n", message);
    }

    return 0;
}
