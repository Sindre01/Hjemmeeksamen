#include "common.h"
#include "send_packet.h"

struct client* make_client(int nick_start, char* recv_buf, struct sockaddr_in recv_addr, size_t* REG_clients, struct addrinfo* res, int sock, int ant_indexes_init);