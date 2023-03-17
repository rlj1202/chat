#include <iostream>
#include <string>
#include <atomic>
#include <thread>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

void input_handler(std::atomic<bool> &run, int socket_fd) {
    while (run) {
        std::string msg;
        std::getline(std::cin, msg);

        if (msg.compare(".exit") == 0) {
            run = false;
            close(socket_fd);
            break;
        }

        if (send(socket_fd, msg.c_str(), msg.length(), 0) < 0) {
            std::cerr << "send failed." << std::endl;
            return;
        }
    }
}

int main() {
    // Create socket
    // AF_INET means IPv4 internet protocols
    // SOCK_STREAM means sequenced, reliable, two-way, connection-based byte streams,
    //     which means TCP rather than UDP.
    int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "socket failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    char server_ip[] = "127.0.0.1"; // TODO:
    unsigned short server_port = 8080;

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    // htons function converts host byte order to network byte order.
    // 's' stands for 'short'.

    // Convert IPv4 and IPv6 addresses from text to binary form.
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address: Address not supported." << std::endl;
        exit(EXIT_FAILURE);
    }

    int status;
    if ((status = connect(socket_fd, (struct sockaddr*) &server_addr, sizeof(server_addr))) < 0) {
        std::cerr << "Connection failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::atomic<bool> run(true);

    std::thread input_thread(input_handler, std::ref(run), socket_fd);

    char buffer[1024];
    int readn;
    while (run) {
        readn = read(socket_fd, buffer, sizeof(buffer) - 1);
        if (readn < 0) {
            std::cerr << "Read failed." << std::endl;
            exit(EXIT_FAILURE);
        } else if (readn == 0) {
            break;
        }

        buffer[readn] = '\0';
        std::cout << buffer << std::endl;
    }

    run = false;
    input_thread.join();

    close(socket_fd);

    return 0;
}