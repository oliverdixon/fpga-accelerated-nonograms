#ifndef NETWORK_H
#define NETWORK_H

#include <lwip/sys.h>

struct sockaddr_in;

void network_init(unsigned char* mac_address, lwip_thread_fn app);
int network_bind_socket(struct sockaddr_in * const local_addr);
void network_prepare_dst_addr(struct sockaddr_in * const dst_addr);

#endif // NETWORK_H
