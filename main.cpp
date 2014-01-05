#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <getopt.h>
#include <netdb.h>
#include <thread>
#include <new>

const int BUFLEN = 65536;

int socket_server(int _port) {
    int f_listen = ::socket(PF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(_port);

    if (::bind(f_listen, (sockaddr *) &addr, sizeof(addr)) != 0) {
        return -1;
    }
    if (::listen(f_listen, SOMAXCONN) != 0) {
        return -1;
    }
    return f_listen;
}

int socket_accept(int _sock) {
    int f_conn = ::accept(_sock, (sockaddr *) 0, 0);
    return f_conn;
}

int socket_client(const char *_domain, int _port) {
    int f_conn = ::socket(PF_INET, SOCK_STREAM, 0);
    
    sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    
    const hostent* host = gethostbyname(_domain);
    
    if (host->h_addrtype != AF_INET) {
        fprintf(stderr, "Unknown protocol.\n");
        return -2;
    }
    
    for (const char * const *
            p = host->h_addr_list;
            *p != 0; ++p) 
    {
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, *p, str, INET_ADDRSTRLEN);
        printf("Connecting to: %s ...\n", str);

        addr.sin_addr = *(in_addr *)*p;
        
        if (0 != ::connect(f_conn, (const sockaddr *) &addr, sizeof(addr))) {
            fprintf(stderr, "Failed.\n");
        } else {
            fprintf(stderr, "Connected.\n");
            return f_conn;
        }
    }
    
    fprintf(stderr, "All candidates of address failed. Aborting ...\n");
    
    return -1;
}

ssize_t copyfd(int i, int o) 
{
    ssize_t total_size = 0;
    char *buf = new char [BUFLEN];
    ssize_t size;
    
    while ((size = ::read(i, buf, BUFLEN)) > 0) {
        const char *p = buf;
        ssize_t wsize = 1;
        while (size > 0) {
            wsize = ::write(o, buf, size);
            if (wsize <= 0) {
                fprintf(stderr, "Write error\n");
                return -1;
            }
            total_size += wsize;
            size -= wsize;
        }
    }
    
    return total_size;
}

void send_recv(int sock) {
    std::thread th_send([=] {
        ssize_t ret = copyfd(0, sock);
        if (ret < 0) { fprintf(stderr, "Error in sending\n"); }
        else { shutdown(sock, SHUT_WR); }
    } );
    
    std::thread th_recv([=] {
        ssize_t ret = copyfd(sock, 1);
        if (ret < 0) { fprintf(stderr, "Error in receiving\n"); }
        else { shutdown(sock, SHUT_RD); }
    } );
    
    th_send.join(); th_recv.join();
    
    ::close(sock);
}

int server_mode(int _port) {
    int f_listen = socket_server(_port);
    int f_conn = socket_accept(f_listen);
    ::close(f_listen);
    
    send_recv(f_conn);
    
    return 0;
}

int client_mode(const char *_dom, int _port) {
    int sock = socket_client(_dom, _port);
    if (sock < 0) return -1;

    send_recv(sock);
    
    return 0;
}


int main(int argc, char *argv[])
{
    int reverse = 0;
    char domain[256];
    int port = 0;
    int is_server = 0;
    int is_client = 0;

    for (
        int opt = 0; 
        (opt = getopt(argc, argv, "c:s:")) != -1;
    ) {
        switch (opt) {
        case 's': {
            char tmp = '\0';
            if (sscanf(optarg, "%d%c", &port, &tmp) != 1) {
                fprintf(stderr, "Port number should be integer.\n");
                return 1;
            }
            is_server = 1;
            
            break;
        }
        case 'c': {
            char tmp = '\0';
            if (sscanf(optarg, "%256[^:]:%d%c", domain, &port, &tmp) != 2) {
                fprintf(stderr, "Address should be \033[4mhostname\033[0m:\033[4mport\033[0m.\n");
                return 1;
            }
            is_client = 1;
            
            break;
        }
        default: {
            fprintf(stderr, "Unknown argument: %c\n", opt);
            return 1;
        }
        }
    }

    if (!is_server && !is_client) {
        fprintf(
            stderr, 
            "Usage: \n"
            "  %s (-s \033[4mport\033[0m|-c \033[4mhostname\033[0m:\033[4mport\033[0m)\n"
            , argv[0]
        );
        return 1;
    }
    
    if (is_server && is_client) {
        fprintf(
            stderr,
            "Cannot use server-mode and client-mode at the same time.\n"
        );
        return 1;
    }
    
    if (is_server) {
        return server_mode(port);
    } else {
        return client_mode(domain, port);
    }
        
    return 0;
}

