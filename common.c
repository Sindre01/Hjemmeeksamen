#include "common.h"

char* extract_num_from_string(char* recv_buf, char* num_string){

    int nummer_len = 0;
    for (int i = 4; i < MSG_BUFSIZE; i++) {
        if (isspace(recv_buf[i])) {
            break;
        }
        num_string[i-4] = recv_buf[i];
        nummer_len++;
    }
    num_string[nummer_len] = '\0';
    
    return num_string;
}

int search_for_client(size_t* clients_cache, char* nick, int ant_indexes_init){ //Søker etter klient basert på nick
    for (int i = 0; i < ant_indexes_init; i++) {
        struct client* client = (struct client *) clients_cache[i];
        // printf("Sjekker klient: \n");
        // print_client(client);
        if (client->port == 0) { //hvis slettet
            continue;
        }
        if (strcmp(client->nick, nick) == 0){
            //printf("Fant clienten i register/cache: %s\n", client->nick);
            return i; //Returnere indeksen hvor client ble funnet
        }
    }
    return -1; //Ingen klient funnet
}
int search_for_client_IP_PORT(size_t* clients_cache, char* ip, int port, int ant_indexes_init){ //Søker etter klient basert på ip adresse og port
    for (int i = 0; i < ant_indexes_init; i++) {
        struct client* client = (struct client *) clients_cache[i];
        // printf("Sjekker klient: \n");
        // print_client(client);
        // printf("Sammenlign med ip: %s og port: %d\n", ip, port);
        if (client->port == 0) { //hvis slettet
            continue;
        }
        if ( (strcmp((char*)client->ip, (char*)ip) == 0) && (client->port == port ) ){
            // printf("Fant clienten i register/cache: %s\n", client->nick);
            return i; //Returnere indeksen hvor client ble funnet
        }
    }
    return -1;
}

char* generate_string(char** buffer, char* argument1, char* argument2, char* argument3, char* argument4, char* argument5, char* argument6, char* argument7, char* argument8){ //Argument1 er en streng på 3 bokstaver
    char string[MSG_BUFSIZE];
    strcpy(string, argument1);

    if (strcmp(argument2, "") != 0) {
        strcat(string, " ");
        strcat(string, argument2);

    }
    if (strcmp(argument3, "") != 0) {
        strcat(string, " ");
        strcat(string, argument3);

    }
    if (strcmp(argument4, "") != 0) {
        strcat(string, " ");
        strcat(string, argument4);

    }
    if (strcmp(argument5, "") != 0) {
        strcat(string, " ");
        strcat(string, argument5);

    }
    if (strcmp(argument6, "") != 0) {
        strcat(string, " ");
        strcat(string, argument6);

    }
    if (strcmp(argument7, "") != 0) {
        strcat(string, " ");
        strcat(string, argument7);
    }

    if (strcmp(argument8, "") != 0) {
        strcat(string, " ");
        strcat(string, argument8);
    }
    memcpy(*buffer, string, strlen(string)+1);
    return *buffer;
}

//Blir bare brukt i klient: (men er her fordi den brukes i free_client)
void free_melding(struct melding* msg){
    free(msg->tekst);
    free(msg->format_msg);
    free(msg);
}

void free_meldinger(struct melding* msg_root){
    struct melding *tmp, *tmp2;
    tmp = msg_root;
    //Frigjør roten og setter roten til å være den roten pekte på. Frigjør bortover til alle er frigjort
    while (tmp != NULL){
        tmp2 = tmp->neste;
        //printf("Free melding: %s \n", tmp->tekst);
        free_melding(tmp);
        tmp = tmp2;
    }
}
////////////////////

void free_client(struct client* client) {
    free(client->nick);
    free(client->ip);
    free_meldinger(client->meldinger_sent);
    free(client);
}

void free_clients(size_t* REG_clients, int ant_indexes_init){
    //Free clients
    for (int i = 0; i < ant_indexes_init; i++){
        struct client* client = (struct client *) REG_clients[i];
        free_client(client);
    }
}  

int set_in_client(size_t* REG_clients, struct client* client, int ant_indexes_init) {

    for (int i = 0; i < ant_indexes_init; i++) {
        struct client* compare_client = (struct client *) REG_clients[i];

        if (compare_client->port == 0) { //hvis slettet
            printf("Klient lagt til i listen og satt inn der hvor en tidligere klient var slettet\n");
            //Setter inn den nye klienten her
            free_client(compare_client);
            REG_clients[i] = (size_t) client;
            return 0;

        } else if (strcmp(client->nick, compare_client->nick) == 0) { //Erstatte klient med likt navn
            printf("Klient lagt til i listen og erstattet tidligere klient med likt navn\n");
            free_client(compare_client);
            REG_clients[i] = (size_t) client;
            return -1;
        }
    }
    //Legger til på slutten:
    REG_clients[ant_indexes_init] = (size_t) client;
    //printf("Klient lag til på slutten av listen\n");
    return 1;
}


void print_client(struct client* client){
    printf("nick: %s ip: %s port: %d\n", client->nick, client->ip, client->port);
}