#include "common.h"

struct blocked_nick{
    char* nick;
    struct blocked_nick *neste;
};

void cpy_melding_linkedlist(struct melding* msg, struct melding** copied_list);
int add_melding(struct melding* msg, struct melding** msg_root); //Legger til melding bakerst i send_meldinger-lenkelisten
void print_meldinger(struct melding* msg);
struct melding* slett_melding(struct melding* msg_root, int nummer);
void free_blocked(struct blocked_nick** blocked_root);
int unblock_nick(char* nick, struct blocked_nick** blocked_root);
int check_if_blocked(char* nick, struct blocked_nick** blocked_root);
int timeout_recvfrom(int sock, char* buf, int time, struct addrinfo* server);
int check_if_PKT_MSG(char* recv_buf, char* nick, char** from_nick, char** tekst); //Returverdier: -1 er feil format, 0 er feil mottaker, 1 er rikitg format og mottaker
int send_format_MSG(struct client *to_client, int client_init_bool, int sock, int PKT_nummer, char* nick, char* send_buf, struct addrinfo** client_friend, struct addrinfo hints);
int send_lookup(int sock, char* send_buf, char* recv_buf, struct addrinfo* server, int timeout, int *lookup_num);
int check_lookup_message_recv(char* recv_buf, char* compare_buf, int lookup_nummer);
struct client* get_info_from_lookup_msg(struct client** client, char* msg, char* to_nick);