#include "client_funcs.h"


void cpy_melding_linkedlist(struct melding* msg, struct melding** copied_list) {

    if (msg == NULL) {
        *copied_list = NULL;
        return;
    }
    
    struct melding* tmp = msg;
    struct melding* start = (*copied_list); //Peker på første melding i listen

    while (tmp != NULL) {
        (*copied_list)->tekst = strdup(tmp->tekst);
        (*copied_list)->format_msg = strdup(tmp->format_msg);
        (*copied_list)->time_sent = tmp->time_sent;
        (*copied_list)->nummer = tmp->nummer;
        (*copied_list)->tries = tmp->tries;

        if (tmp->neste != NULL) {
            (*copied_list)->neste = malloc(sizeof(struct melding));
            (*copied_list) = (*copied_list)->neste;
        } else {
            (*copied_list)->neste = NULL;
        }

        tmp = tmp->neste;
    }
    (*copied_list) = start;


}

int add_melding(struct melding* msg, struct melding** msg_root){ //Legger til melding bakerst i send_meldinger-lenkelisten 
    //Send_meldinger er en global lenkeliste som innheolder medlinger som skal sendes-
    if (msg == NULL) {
        return -1;
    }
    if (*msg_root == NULL) {
        *msg_root = msg;
        (*msg_root)->neste = NULL;
        return 1;
    }
    struct melding* tmp;
    tmp = *msg_root;
    while (tmp->neste != NULL) {
        tmp = tmp->neste;
    }

    tmp->neste = msg;
    return 1;
}

void print_meldinger(struct melding* msg) {
    struct melding* tmp = msg;

    while (tmp != NULL){
        fprintf(stderr, "(%s, %s, tries: %d, nummer: %d) -> ", tmp->tekst, tmp->format_msg, tmp->tries, tmp->nummer);
        tmp = tmp->neste;
    }
    fprintf(stderr, "(NULL)\n");
}

struct melding* slett_melding(struct melding* msg_root, int nummer) { //Sletter meldingen med nummer som PKT nummer
    int found = 0;
    struct melding *tmp, *tmp2, *start;
    tmp = msg_root;
    start = msg_root;
    
    if (tmp == NULL) {
        return NULL;
    }

    if (tmp->nummer == nummer) { //Hvis dette er den vi leter etter
        start = tmp->neste;
        free_melding(msg_root);
        return start;
        
    } else {
        while (tmp->neste != NULL){
            if ( tmp->neste->nummer == nummer ) { //Hvis den neste er den vi leter etter
                //fant den blokkerte
                found = 1;
                break;
            }
            tmp = tmp->neste;
        }

        if (found == 1) {
            tmp2 = tmp->neste;
            tmp->neste = tmp->neste->neste; //Setter pekeren til å hoppe over den den pekte på
            free_melding(tmp2);
            return start;
        }
    }

    return NULL; //Fant ikke den blokkerte nicken
}


void free_blocked(struct blocked_nick **blocked_root){
    struct blocked_nick *tmp, *tmp2;
    tmp = (*blocked_root);
    //Frigjør roten og setter roten til å være den roten pekte på. Frigjør bortover til alle er frigjort
    while (tmp != NULL){
        tmp2 = tmp->neste;
        free(tmp->nick);
        free(tmp);
        tmp = tmp2;
    }
}
int unblock_nick(char* nick, struct blocked_nick **blocked_root){
    int found = 0;
    struct blocked_nick *tmp, *tmp2;
    tmp = (*blocked_root);
    if (strcmp(tmp->nick, nick) == 0) { //Hvis dette er den vi leter etter
        //fant den blokkerte
        free((*blocked_root)->nick);
        free((*blocked_root));
        (*blocked_root) = NULL;
        
    } else {
        while (tmp->neste != NULL){
            if (strcmp((tmp->neste)->nick, nick) == 0) { //Hvis den neste er den vi leter etter
                //fant den blokkerte
                found = 1;
                break;
            }
            tmp = tmp->neste;
        }
        if (found == 1) {
            tmp2 = tmp->neste;
            tmp->neste = tmp->neste->neste; //Setter pekeren til å hoppe over den den pekte på
            free(tmp2->nick);
            free(tmp2);
            return 1;
        }
    }

    return 0; //Fant ikke den blokkerte nicken
}

int check_if_blocked(char* nick, struct blocked_nick** blocked_root){
    struct blocked_nick *tmp;
    tmp = (*blocked_root);
    while (tmp != NULL){
        if (strcmp(tmp->nick, nick) == 0) { 
            //fant den blokkerte
            return 1;
        }
        tmp = tmp->neste;
    }
    return 0;
}

int timeout_recvfrom(int sock, char* buf, int time, struct addrinfo* server) {
	fd_set readfds;
	struct timeval tv;
	int rc;

	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);
	tv.tv_sec = time;
	tv.tv_usec = 0;

	rc = select(FD_SETSIZE, &readfds, NULL, NULL, &tv); //Venter tiden som er gitt
    if (rc == -1){
        perror("select");
        return -1;
    }
	

	if (FD_ISSET(sock, &readfds)) {
		//printf("før recvfrom\n");
		rc = recvfrom(sock, buf, MSG_BUFSIZE, 0, NULL, NULL);
        if (rc == -1){
            perror("recvfrom");
            return -1;
        }
        
		return rc;
	} 
	return 0; //ingen melding motatt
}

int check_if_PKT_MSG(char* recv_buf, char* nick, char** from_nick, char** tekst){ //Returverdier: -1 er feil format, 0 er feil mottaker, 1 er rikitg format og mottaker

    //Sjekker om medingen inneholder formatet vi er ute etter
    char* FROM_start = strstr(recv_buf, "FROM");
    if (FROM_start == NULL){
        return -1;
    }
    char* TO_start = strstr(recv_buf, "TO");
    if (TO_start == NULL){
        return -1;
    }
    char* MSG_start = strstr(recv_buf, "MSG");
    if (MSG_start == NULL){
        return -1;
    }

    char from_nick_tmp[NICK_SIZE] = "";
    char* start = FROM_start + strlen("FROM")+1; //Starter på fra_nick
    int len = strlen(start) - strlen(TO_start)-1;
    strncpy(from_nick_tmp, start, len);
    from_nick_tmp[len+1] = '\0';;
    //printf("to_nick: %s med lengde %d\n", from_nick_tmp, len);

    char to_nick[NICK_SIZE] = "";
    start = TO_start + strlen("TO")+1; //Starter på to_nick
    len = strlen(start) - strlen(MSG_start)-1;
    strncpy(to_nick, start, len);
    to_nick[len+1] = '\0';;
    //printf("to_nick: %s med lengde %d\n", to_nick, len);

    char tekst_tmp[TEXT_SIZE] = "";
    start = MSG_start + strlen("MSG")+1; 
    len = strlen(start);
    strncpy(tekst_tmp, start, len);
    tekst_tmp[len+1] = '\0'; 


    if (strcmp(to_nick, nick) != 0){ //ikke riktig mottaker
        return 0; //Signaliserer: send meding tilbake med "ACK nummer WRONG NAME"
    } 

    memcpy(*from_nick, from_nick_tmp, strlen(from_nick_tmp)+1);
    memcpy(*tekst, tekst_tmp, strlen(tekst_tmp)+1);
    
    return 1; //Signaliserer: Send melding tilbake med "ACK nummer OK"
    
}

int send_format_MSG(struct client *to_client, int client_init_bool, int sock, int PKT_nummer, char* nick, char* send_buf, struct addrinfo** client_friend, struct addrinfo hints){
    char port_streng[SMALL_BUFSIZE];
    sprintf(port_streng, "%d", to_client->port); //Gjør om int til char*
    // print_client(to_client);

    int status = getaddrinfo(to_client->ip, port_streng, &hints, client_friend);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }
    int rc = send_packet(sock, send_buf, strlen(send_buf), 0, (*client_friend)->ai_addr, (*client_friend)->ai_addrlen);
    if (status == -1) {
        perror("send_packet");
        return -2;
    }
}

int send_lookup(int sock, char* send_buf, char* recv_buf, struct addrinfo* server, int timeout, int *lookup_num) {

    //Prøver å sende en oppslagsmelding
    //printf("Sender oppslagelding\n");
    int wc = send_packet(sock, send_buf, strlen(send_buf), 0, server->ai_addr, server->ai_addrlen);
    if (wc == -1){
        perror("send_packet");
        return -1;
    } 
    (*lookup_num) += 1;

    int found = timeout_recvfrom(sock, recv_buf, timeout, server);
    if (found > 0) {
        recv_buf[found] = '\0';
        //Sjekker om dette er den medlingen som forventes tilbake ha (Fordi meldingen kan være en MSG-meldingen fra en annen klient).
        if (strncmp("ACK", recv_buf, 2) == 0) {
            char recv_num_string[SMALL_BUFSIZE];
            char* recv_num_str = extract_num_from_string(recv_buf, recv_num_string);

            char send_num_string[SMALL_BUFSIZE];
            char* send_num_str = extract_num_from_string(send_buf, send_num_string);

            if (strcmp(recv_num_str, send_num_str) == 0) {
                return found; //Returnerer antall bytes som ble lest
            }
        }
    } else if (found == -1) { //feil ved select() eller recvfrom()
        return -1;
    }
    //Hvis ingen motatt så prøves det to ganger til
    for (int i = 0; i < 2; i++){ //Oppgavetekst: "sender en oppslagsmelding og hvis ikke svar så 2 ganger til"
        //printf("Sender oppslagelding\n");
        wc = send_packet(sock, send_buf, strlen(send_buf), 0, server->ai_addr, server->ai_addrlen);
        if (wc == -1){
            return -1;
        } 
        (*lookup_num) += 1;
        found = timeout_recvfrom(sock, recv_buf, timeout, server);
        if (found > 0) {
            recv_buf[found] = '\0';
            if (strncmp("ACK", recv_buf, 2) == 0) {
                char recv_num_string[SMALL_BUFSIZE];
                char* recv_num_str = extract_num_from_string(recv_buf, recv_num_string);

                char send_num_string[SMALL_BUFSIZE];
                char* send_num_str = extract_num_from_string(send_buf, send_num_string);

                if (strcmp(recv_num_str, send_num_str) == 0) {
                    return found; //Returnerer antall bytes som ble lest
                }
            }
        }
    }
    return 0; //returner 0 hvis ingen melding motatt innen tidsavbruddsperioden
}

int check_lookup_message_recv(char* recv_buf, char* compare_buf, int lookup_nummer){

    //melding motatt. Sjekker hvordan melding vi fikk tilbake
    int FOUND = -1; //Hvis uendret så motokk vi en ACK som vi ikke kjenner til. Da ingnoreres denne ved fortsette hovedhendelsesløkken
    char lookup_string[SMALL_BUFSIZE];
    
    sprintf(lookup_string, "%d", lookup_nummer); //Gjør om int til char*
    generate_string(&compare_buf, "ACK\0", lookup_string, "NOT FOUND\0", "", "", "", "", "");
    if (strncmp(recv_buf, compare_buf, strlen(compare_buf)) == 0){ 
        FOUND = 0;
    }

    generate_string(&compare_buf, "ACK\0", lookup_string, "NICK\0", "", "", "", "", "");
    if (strncmp(recv_buf, compare_buf, strlen(compare_buf)) == 0){ //sammenligner bare de første ordene opptil NICK og antar at server sender rikitg format
        FOUND = 1;
    }

    return FOUND;
}

struct client* get_info_from_lookup_msg(struct client** client, char* msg, char* to_nick){

   //Finner ip-adressen
    char* NICK_start = strstr(msg, "NICK");
    char* IP_start = NICK_start + (strlen(to_nick)) + 6;
    unsigned char ip_msg[SMALL_BUFSIZE];

    int index_so_far = 0;

    for (int i = 0; i < strlen(IP_start); i++){
        char bokstav = IP_start[i];
        if (isspace(bokstav)){
            ip_msg[i] = '\0';
            index_so_far = i+1;
            break;
        } else {
            ip_msg[i] = bokstav;
        }
    }

    //Finner PORT nummeret:
    int port_index_start = index_so_far + 5;
    char port_msg [SMALL_BUFSIZE];
    for (int i = port_index_start; i < strlen(msg) + 1; i++){
        char bokstav = IP_start[i];
        if (bokstav == '\0') {
            port_msg[i-port_index_start] = '\0';
        } else {
            port_msg[i-port_index_start] = bokstav;
        }
    }
    free((*client)->nick);
    free((*client)->ip);

    (*client)->nick = strdup(to_nick); 
    (*client)->ip = strdup(ip_msg);
    (*client)->port = atoi(port_msg);


    return *client;

}

