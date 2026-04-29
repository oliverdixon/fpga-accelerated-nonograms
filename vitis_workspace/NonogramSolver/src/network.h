// clang-format Language: C

#ifndef NETWORK_H
#define NETWORK_H

#include <FreeRTOS.h>
#include <semphr.h>

#include <lwip/sockets.h>
#include <lwip/sys.h>

struct NetworkState
{
    int sock;
    struct sockaddr_in local_addr;
    struct sockaddr_in dst_addr;
    SemaphoreHandle_t mutex;
};

void network_initialise(
    struct NetworkState * network,
    lwip_thread_fn app
);

int network_bind_socket(
    struct sockaddr_in * const local_addr,
    in_port_t local_port
);

void network_prepare_dst_addr(struct sockaddr_in * const dst_addr);

#endif // NETWORK_H
