#include "server_funcs.h"

int main(int argc, char const *argv[]){
    struct addrinfo hints;
    struct addrinfo* res;
    char* port;
    
    char* send_buf = malloc(SMALL_BUFSIZE);
    if (send_buf == NULL){
        fprintf(stderr, "malloc failed. possibly out of memory\n");
        return EXIT_FAILURE;
    }
    memset(send_buf, 0, SMALL_BUFSIZE);

    int ant_clients = 0;  //antall tilgjengelige klienter

    int ant_indexes_init = 0;  //For dynamisk allokering. Antall indexes som er initilaisert med en struct peker i size_t
    int max_clients = 10;  //For dynamisk allokering. Starter plass til 10 klienter
    size_t* REG_clients = malloc(sizeof(struct client*)*max_clients);
    if (REG_clients == NULL) {
        fprintf(stderr, "malloc failed. possibly out of memory\n");
        return EXIT_FAILURE; 
    }
    
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <port> <tapsannsynlighet (0-100)> \n", argv[0]);
        free(REG_clients);
        free(send_buf);
        return EXIT_SUCCESS; //Ingen teknisk feil
    }

    port = (char*)argv[1];
    if (1024 > atoi(port) || atoi(port) > 65535) {
        fprintf(stderr, "Port must be in range 1024 to 65535\n");
        free(REG_clients);
        free(send_buf);
        return EXIT_SUCCESS; //ingen teknisk feil
    }

    float probability = ((float)atof(argv[2]))/100.0;
    if ( (probability > 1) || (probability < 0) ) { //Må være et tall mellom 0 og 1, ETTEr delt på 100
        fprintf(stderr, "Tapssannsynlighet must be in range 0-100 \n");
        free(REG_clients);
        free(send_buf);
        return EXIT_SUCCESS; //Ingen teknisk feil
    }

    //Bruker srand48 fordi drand48() ikke fungerer på WSL (Lært fra gruppetime)
    time_t sekunder;
    sekunder = time(NULL);
    srand48(sekunder);
    set_loss_probability(probability);

    //setter alle til 0
	memset(&hints, 0, sizeof(struct addrinfo));
    //Fyller inn de jeg trenger
	hints.ai_family = AF_INET; 
	hints.ai_socktype = SOCK_DGRAM; //Bruker UDP
	hints.ai_flags = AI_PASSIVE; //Fyller inn ip for meg

    //Lagrer adrresen/port informasjon i res
	int status = getaddrinfo(NULL, port, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        free(REG_clients);
        free(send_buf);
        return EXIT_FAILURE;
    }
    
    //oppretter socket hvor det skal mottas medlinger fra klienter
    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock == -1){
        perror("socket");
        freeaddrinfo(res);
        free(REG_clients);
        free(send_buf);
        return EXIT_FAILURE;
    }
	
    //Binder adressen til socket
	status = bind(sock, res->ai_addr, res->ai_addrlen);
    if (status == -1){
        perror("bind");
        freeaddrinfo(res);
        close(sock);
        free(REG_clients);
        free(send_buf);
        return EXIT_FAILURE;
    }

    //Brukers til å lagre informasjon om avsender av meldingen
    struct sockaddr_in recv_addr;
    socklen_t recv_len = sizeof(struct sockaddr_in);
    char recv_buf[MSG_BUFSIZE];

    fd_set readfds;
    int rc;
    int slutt_status = EXIT_SUCCESS; //For lagre slutt-statusn til programmet som returnes til slutt. Dette gjør at mye lik kode blir fjernet
    while (1) {
        //Nullstiller
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds); //Lytter til socket
        FD_SET(STDIN_FILENO, &readfds); //Lytter til tastatur
        
        //sjekker om noe har kommet inn på socket eller STDIN
        rc = select(FD_SETSIZE, &readfds, NULL, NULL, NULL); //Venter tiden som er gitt
        if (rc == -1){
            perror("recvfrom");
            slutt_status = EXIT_FAILURE;
            break;
        }
        //Hvis melding fra nett inn på socket
        if (FD_ISSET(sock, &readfds)){
            rc = recvfrom(sock, recv_buf, MSG_BUFSIZE-1, 0, (struct sockaddr*)&recv_addr, &recv_len);
            if (rc == -1){
                perror("recvfrom");
                slutt_status = EXIT_FAILURE;
                break;
            }
            recv_buf[rc] = '\0';
           
            //Finner PKT nummer:
            char number_string[SMALL_BUFSIZE];
            char* number = extract_num_from_string(recv_buf, number_string);

            //Finner type melding (Kunne brukt strstr() her, men velger istedenfor å gjøre dette for bedre forståelse)
            int number_len = strlen(number);
            int type_len = 0;
            int type_start = 4+number_len+1; 
            char type[type_len];
            for (int i = type_start; i < MSG_BUFSIZE; i++){
                if ((recv_buf[i] == '\0') || isspace(recv_buf[i])) {
                    type[type_len] = '\0';
                    break;
                }
                type[i-type_start] = recv_buf[i];
                type_len++;
            }

            int nick_start = type_start+type_len+1;
            
            //Lager klient og sjekker om den er i REG_clients
            struct client* new_Client = make_client(nick_start, recv_buf, recv_addr, REG_clients, res, sock, ant_indexes_init);
            int value = search_for_client(REG_clients, new_Client->nick, ant_indexes_init); //returner indeksen hvor klient ble funnet eller -1 hvis ingen klient ble funnet

            if (strcmp(type, "REG") == 0) { //Registrer klienten
                int allready_in_list = 0;
                //Legger inn tidspunktet klienten ble registrert på
                time_t seconds = time(NULL);
                new_Client->last_REG = seconds;
                
                if (value == -1) { //Fant ikke noen klient med dette navnet og må sjekke om mer minne trengs å settes av før innseting i liste
                    printf("\nMotatt: %s\n", recv_buf);
                    printf("Registrerer ny klient med nick: %s, ip: %s og port: %d\n", new_Client->nick, new_Client->ip, new_Client->port);
                    if (ant_indexes_init >= max_clients-1) { //Hvis mer minne trengs
                        printf("setter av mer minne! ");
                        max_clients = max_clients*2;
                        REG_clients = realloc(REG_clients, sizeof(struct client*) *(max_clients));
                        printf("Nytt minne: %d og gammelt minne: %d\n", max_clients, max_clients/2 );
                    }

                } else { //Fant klient i REG_clients
                    struct client *stored_client = (struct client*) REG_clients[value];
                    if (new_Client->last_PKT_num != stored_client->last_PKT_num) {
                        printf("\nMotatt: %s\n", recv_buf);
                        printf("Heartbeat fra klient med nick: %s, ip: %s og port: %d\n", new_Client->nick, new_Client->ip, new_Client->port);

                    } else { //Hvis denne pakken duplikat
                        allready_in_list = 1;
                        stored_client->last_REG = seconds;
                        free_client(new_Client);
                    }
    
                } 
                if (!allready_in_list) { //Hvis ikke allerede i liste
                    //Metode som leter igjennom listen og finner første ledige plass, kan være at tidligere registrerte klienter har blitt slettet. 
                    //Hvis ikke funnet blir den satt inn på slutten av listen
                    //Hvis funnet så blir klienten erstattet med den nye registreringen
                    status = set_in_client(REG_clients, new_Client, ant_indexes_init);
                    if (status == 1) { //Hvis satt inn på slutten av listen
                        ant_indexes_init++;
                        ant_clients++;
                    } else if (status == 0) { //Satt inn der hvor tidligere klient var slettet (hvor porten var 0)
                        ant_clients++;
                    }
                }
                
                char* send = generate_string(&send_buf, "ACK\0", number, "OK\0", "", "", "", "", "");
                int wc = send_packet(sock, send, strlen(send), 0, (struct sockaddr*)&recv_addr, sizeof(struct sockaddr_in)); //send_packet(soc, send_buf, SMALL_BUFSIZE, 0, (struct sockaddr*)&recv_addr, sizeof(struct sockaddr_in));
                if (wc == -1) {
                    perror("send_packet");
                    slutt_status = EXIT_FAILURE;
                    break;
                }

            } else if (strcmp(type, "LOOKUP") == 0) { //Oppslag-melding
                printf("\nMotatt: %s\n", recv_buf);
                
                int remove_client = 0;
                free_client(new_Client);

                if (value == -1){ //Fant ingen med dette navnet
                    generate_string(&send_buf, "ACK\0", number, "NOT FOUND\0", "", "", "", "", "");

                } else { //Fant en med dette navnet
                    struct client* client = (struct client *) REG_clients[value];

                    //Sjekker om clienten har blir registrert i løpet av de siste 30 sekundene
                    time_t seconds2 = time(NULL);
                    time_t time_gone = (seconds2 - client->last_REG);
                    printf("Time since last REG: %ld seconds\n", time_gone);
                    time_t delete_after_this_time = 30;
                    if (time_gone > delete_after_this_time) { //HVis det har gått over 30 seunder siden siste registrering
                        generate_string(&send_buf, "ACK\0", number, "NOT FOUND\0", "", "", "", "", "");
                        remove_client = 1; //signaliserer at klienten skal fjernes etter at den er sendt

                    } else { //Hvis server hat motatt heartbeat innefor 30 sek
                        char port_int[SMALL_BUFSIZE];
                        sprintf(port_int, "%d", client->port); //Gjør om int til char*

                        generate_string(&send_buf, "ACK\0", number, "NICK\0", client->nick, (char*)client->ip, "PORT\0", port_int, ""); //lager strengern: "N"
                    }
                }

                printf("Sender tilbake: %s\n", send_buf);
                int wc = send_packet(sock, send_buf, SMALL_BUFSIZE, 0, (struct sockaddr*)&recv_addr, sizeof(struct sockaddr_in)); //send_packet(soc, send_buf, SMALL_BUFSIZE, 0, (struct sockaddr*)&recv_addr, sizeof(struct sockaddr_in));
                if (wc == -1) {
                    perror("send_packet");
                    slutt_status = EXIT_FAILURE;
                    break;
                }
                if (remove_client == 1) { //Sletter klienten fra REG_clients
                    struct client* client = (struct client *) REG_clients[value];
                    client->port = 0; //setter porten til 0 for å markere at klienten er slettet
                    ant_clients--;
                    printf("slettet %s fra REG_clients\n", client->nick);
                }

            } else {
                free(new_Client);
            }

        } else if (FD_ISSET(STDIN_FILENO, &readfds)){ //Ser om det har kommet input fra STDIN
            char stdin_buf[MSG_BUFSIZE];
            fgets(stdin_buf, MSG_BUFSIZE, stdin);
            if (stdin_buf[strlen(stdin_buf) - 1] == '\n') {
                stdin_buf[strlen(stdin_buf) - 1] = '\0'; //Fjerner newline
            }

            char end_msg[5] = "QUIT";

            if (strcmp(stdin_buf, end_msg) == 0){ //Avslutter programmet
                printf("Avslutter..\n");
                break;
            }
        }
    }

    //Frigjør minne fra heapen
    free_clients(REG_clients, ant_indexes_init);
    free(REG_clients);
    freeaddrinfo(res);
    free(send_buf);
    close(sock);

    //Informasjon om hvor mange av hver pakketype som ikke fikk en ACK tilbake: (Det vil si at pakken kan også ha gått tapt fra server og til klient)
    printf("|-----------------------------------------------------------\n");
    printf("|   Info: \n");
    printf("|       > Antall klienter i minne ved slutt: %d \n", ant_clients);
    printf("|-----------------------------------------------------------\n");
    return slutt_status;
}