#include "server_funcs.h"


struct client* make_client(int nick_start, char* recv_buf, struct sockaddr_in recv_addr, size_t* REG_clients, struct addrinfo* res, int sock, int ant_indexes_init) {

        char pkt_nummer[SMALL_BUFSIZE];
        int nummer_len = 0;
        for (int i = 4; i < MSG_BUFSIZE; i++) {
            if (isspace(recv_buf[i])) {
                break;
            }
            pkt_nummer[i-4] = recv_buf[i];
            nummer_len++;
        }
        pkt_nummer[nummer_len] = '\0';
        int pkt_num = atoi(pkt_nummer);

        //Finner navn:
        int nick_len = 0;
        for (int i = nick_start; i < MSG_BUFSIZE; i++) {
            if (isspace(recv_buf[i]) || (recv_buf[i] == '\0')) {
                break;
            }
            nick_len++;
        }

        if (nick_len > NICK_SIZE) {
            fprintf(stderr, "NICK er for langt!\n");
            freeaddrinfo(res);
            close(sock);
            free_clients(REG_clients, ant_indexes_init);
            free(REG_clients);
            exit(EXIT_FAILURE);
        }

        char nick[nick_len];
        //printf("Type_len: %d Nick_len: %d  Buf: %s\n", type_len, nick_len, recv_buf);

        for (int i = 0; i < nick_len; i++){
            char bokstav = recv_buf[nick_start+i];
            if (isspace(bokstav)) {
                printf("Ugyldig tegn i NICK\n");
                freeaddrinfo(res);
                close(sock);
                free_clients(REG_clients, ant_indexes_init);
                free(REG_clients);
                exit(EXIT_FAILURE);
            } 
            if (isascii(bokstav)) {
                nick[i] = bokstav;
            }
        }
        nick[nick_len] = '\0';
        
        struct client* newClient = malloc(sizeof(struct client));
        if (newClient == NULL) {
            fprintf(stderr, "malloc failed. possibly out of memory\n");
            exit(EXIT_FAILURE);
        }
        newClient->nick = strdup(nick);
        newClient->ip = (unsigned char*) strdup(inet_ntoa(recv_addr.sin_addr));
        newClient->port = ntohs(recv_addr.sin_port);
        newClient->last_PKT_num = pkt_num;
        newClient->meldinger_sent = NULL;
        return newClient;
}