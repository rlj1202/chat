#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <set>
#include <functional>
#include <mutex>
#include <sstream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

void send_to(const std::string &msg, int socket_fd) {
    if (send(socket_fd, msg.c_str(), msg.length(), 0) < 0) {
        std::cerr << "send failed at send_to_all" << std::endl;
    }
}

void send_to_all(const std::string &msg, int sender_fd, const std::set<int> &client_fds, std::mutex &m) {
    for (int client_fd : client_fds) {
        if (client_fd == sender_fd) continue;

        send_to(msg, client_fd);
    }
}

void client_handler(const std::atomic<bool> &run, const int socket_fd, std::set<int> &client_fds, std::mutex &m) {
    char buffer[1024];
    int readn;

    while (run) {
        readn = read(socket_fd, buffer, sizeof(buffer) - 1);

        if (readn == 0) {
            break;
        } else if (readn < 0) {
            break;
        }

        std::stringstream msg_builder;
        msg_builder << "Client" << socket_fd << ": " << std::string(buffer, readn);
        std::string msg = msg_builder.str();

        std::cout << msg << std::endl;
        send_to_all(msg, socket_fd, client_fds, m);
    }
    
    const std::lock_guard<std::mutex> lock_guard(m);
    client_fds.erase(socket_fd);

    std::stringstream msg_builder;
    msg_builder << "Client" << socket_fd << " has left the chat";
    std::cout << msg_builder.str() << std::endl;
    send_to_all(msg_builder.str(), -1, client_fds, m);
}

void input_handler(std::atomic<bool> &run, const int server_fd, std::set<int> &client_fds, std::mutex &m) {
    while (run) {
        std::string cmd;
        std::getline(std::cin, cmd);

        if (cmd.compare(".exit") == 0) {
            run = false;

            close(server_fd);

            m.lock();
            for (int client_fd : client_fds) {
                close(client_fd);
            }
            client_fds.clear();
            m.unlock();

            break;
        }

        cmd = "Server: " + cmd;
        send_to_all(cmd, -1, client_fds, m);
    }

    std::cout << "input_handler exited." << std::endl;
}

int main() {
    std::vector<std::thread> workers;
    std::atomic<bool> run(true);
    std::set<int> client_fds;
    std::mutex client_fds_lock;

    // Create a socket for server
    int server_fd;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "socket failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDR - reuse already used address when to bind
    //      When a program exits unexpectedly, it is impossible bind to previously used address because it holds previous bind information.
    // SO_REUSEPORT - same as above.
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt" << std::endl;
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    unsigned short port = 8080;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*) &address, addrlen) < 0) {
        std::cerr << "bind failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Backlog queue to 5
    if (listen(server_fd, 3) < 0) {
        std::cerr << "listen" << std::endl;
        exit(EXIT_FAILURE);
    }

    workers.push_back(std::thread(input_handler, std::ref(run), server_fd, std::ref(client_fds), std::ref(client_fds_lock)));

    while (run) {
        int new_socket_fd;
        if ((new_socket_fd = accept(server_fd, (struct sockaddr*) &address, (socklen_t*) &addrlen)) < 0) {
            std::cerr << "Accept failed." << std::endl;
            continue;
        }

        client_fds_lock.lock();
        client_fds.insert(new_socket_fd);
        client_fds_lock.unlock();

        workers.push_back(std::thread(client_handler, std::ref(run), new_socket_fd, std::ref(client_fds), std::ref(client_fds_lock)));

        char msg[] = "Welcome message";
        send_to(msg, new_socket_fd);

        std::stringstream msg_builder;
        msg_builder << "Client" << new_socket_fd << " has joined the chat";
        std::cout << msg_builder.str() << std::endl;
        send_to_all(msg_builder.str(), new_socket_fd, client_fds, client_fds_lock);
    }

    std::cout << "Joining all threads..." << std::endl;
    run = false;
    for (std::thread& worker : workers) {
        worker.join();
    }

    // Shutdown the listening socket
    // shutdown(server_fd, SHUT_RDWR);

    std::cout << "Close server socket." << std::endl;
    // TODO: is it okay to call close multiple times?
    close(server_fd);

    return 0;
}
