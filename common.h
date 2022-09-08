#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <time.h>

#include "send_packet.h"

#define MSG_BUFSIZE 1500 //Nok til å inneholde den største mulige medlingen som er 2*NICK_SIZE + TEXT_SIZE + andre kommandoer
#define SMALL_BUFSIZE 50
#define NICK_SIZE 20
#define TEXT_SIZE 1400

struct client{
    char* nick;
    char* ip;
    int port;
    struct melding* meldinger_sent; //Brukes bare i klient
    time_t last_REG; //To betydninger:     - server: Siste tiden hvor pakken ble registrert       -Klient: Siste tiden hvor pakken ble sendt
    int last_PKT_num;
};

//Brukes bare i klient, men trengs i begge fordi jeg bruker en felles client-struct:
struct melding{
    int tries; //antall ganger meldingen er prøvd sent
    int nummer;
    char* tekst;
    char* format_msg;
    time_t time_sent;
    struct melding *neste;
};
//////////////////////

char* extract_num_from_string(char* recv_buf, char* num_string);
int search_for_client(size_t* clients_cache, char* nick, int max_clients);
int search_for_client_IP_PORT(size_t* clients_cache, char* ip, int port, int ant_indexes_init);
char* generate_string(char** buffer, char* argument1, char* argument2, char* argument3, char* argument4, char* argument5, char* argument6, char* argument7, char* argument8);
void free_clients(size_t* REG_clients, int ant_indexes_init);
void free_client(struct client* client);
int set_in_client(size_t* REG_clients, struct client* client, int ant_indexes_init);
void print_client(struct client* client);
void free_melding(struct melding* msg);
void free_meldinger(struct melding* msg_root);

