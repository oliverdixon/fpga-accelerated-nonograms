// clang-format Language: C

/**
 * @file
 * @brief LwIP network management interface
 * @date 2026-05-17
 * @author Oliver Dixon <od641@york.ac.uk>
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <FreeRTOS.h>
#include <semphr.h>

#include <lwip/sockets.h>
#include <lwip/sys.h>

/**
 * @struct NetworkState
 * @brief The POSIX network management block controlled with a FreeRTOS mutex.
 */
struct NetworkState
{
    int sock; /**< @brief The socket to the UDP/IP server. */
    struct sockaddr_in local_addr; /**< @brief Our local binding address. */
    struct sockaddr_in dst_addr; /**< @brief The server's address. */
    SemaphoreHandle_t mutex; /**< @brief The mutex to protect the socket. */
};

/**
 * @brief Initialise the network runtime into the given NetworkState management block.
 * @param network The destination NetworkState management block.
 * @param app The task for LwIP runtime to release once the network is ready.
 */
void network_initialise(
    struct NetworkState *network,
    lwip_thread_fn app
);

/**
 * @brief Open a socket and bind it to the given local IPv4 address on the given local port.
 * @param local_addr The address to bind.
 * @param local_port The port on which the socket should be bound.
 * @return The POSIX socket number, or -1 on failure.
 */
int network_bind_socket(
    struct sockaddr_in * local_addr,
    in_port_t local_port
);

/**
 * @brief Populate the given address for the EMBS student network Nonogram server.
 * @param dst_addr The buffer for the destination address.
 */
void network_prepare_dst_addr(struct sockaddr_in * dst_addr);

#endif // NETWORK_H
