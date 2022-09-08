#include "client_funcs.h"

//Gobal variabel som inneholder en lenkeliste av nicker som er blokkerte
struct blocked_nick *blocked_nicks = NULL;

int main(int argc, char const *argv[]) {
    struct addrinfo hints;
    struct addrinfo* server;
    struct addrinfo* client_friend;
    int client_init_bool = 0;
    char end_msg[5] = "QUIT"; //melding for å avlsutte program
    int slutt_status = EXIT_SUCCESS; //variabel som holder statusen til programmet ved slutt. bruker break i while-løkken for å hindre mye dublicate kode (free(), close() osv..)

    //argumenter
    char* nick;
    unsigned char* ip;
    char* port;
    int timeout;
    float probability;

    //Brukes for informasjon til slutt og stop&wait
    int PKT_nummer = 0; //MSG nummer for pakker som blir sendt
    int ant_PKT_success = 0; //MSG-pakker som fikk ACK tilbake
    int PKT_Alle = 0; //Alle MSG-pakker som ble sendt

    int REG_nummer = 0; //Heartbeat/REG nummer. Alle som fikk ack
    int REG_Alle = 0; //Alle REG meldinger sendt

    int OPPSLAG_nummer = 0; //Vellykkede OPPSLAG/LOOKUP 
    int OPPSLAG_Alle = 0; //Alle oppslagsmeldinger sendt

    int ant_clients = 0;  

    int ant_indexes_init = 0;  //For dynamisk allokering. Antall indexes som er initilaisert med en struct peker i size_t
    int max_clients = 10;  //For dynamisk allokering. Starter plass til 10 klienter
    //Lagrer kliente i et cache. Jeg har valgt å bruke et array av pekere til structer med informasjon
    size_t* clients_cache = malloc(sizeof(struct REG_client*)*max_clients);
    if (clients_cache == NULL) {
        fprintf(stderr, "malloc failed. possibly out of memory\n");
        return EXIT_FAILURE;
    }

    char* send_buf = malloc(MSG_BUFSIZE); //Brukes til å sende meldinger
    if (send_buf == NULL) {
        fprintf(stderr, "malloc failed. possibly out of memory\n");
        free(clients_cache);
        return EXIT_FAILURE;
    }
    char* recv_buf = malloc(MSG_BUFSIZE); //Brukes til å motta meldinger
    if (recv_buf == NULL) {
        fprintf(stderr, "malloc failed. possibly out of memory\n");
        free(clients_cache);
        free(send_buf);
        return EXIT_FAILURE;
    }
    char *compare_buf = malloc(MSG_BUFSIZE); //Brukes til å sammenligne medlinger
    if (compare_buf == NULL) {
        fprintf(stderr, "malloc failed. possibly out of memory\n");
        free(clients_cache);
        free(send_buf);
        free(recv_buf);
        return EXIT_FAILURE;
    }


    if (argc < 6) {
        fprintf(stderr, "Usage: %s <nick> <adresse> <port> <timeout> <tapsannsynlighet (0-100)> \n", argv[0]);
        free(send_buf);
        free(recv_buf);
        free(compare_buf);
        free(clients_cache);
        return EXIT_SUCCESS;
    }
    nick = (char*)argv[1];
    int nick_length = (int)strlen(nick);
    nick[nick_length] = '\0';
    //Sjekker om nick inneholder lovlig tegn:

    for(int i = 0; i < nick_length; i++) {
        if (isalnum(nick[i]) == 0) {
            fprintf(stderr, "Nick kan bare inneholde tall i det engelske alfabetet og tall\n");
            free(send_buf);
            free(recv_buf);
            free(compare_buf);
            free(clients_cache);
            return EXIT_SUCCESS; //ingen teknisk feil

        }
    }
    
    ip = (unsigned char*)argv[2];

    port = (char*)argv[3];
    if (1024 > atoi(port) || atoi(port) > 65535) {
        fprintf(stderr, "Port must be in range 1024 to 65535\n");
        free(send_buf);
        free(recv_buf);
        free(compare_buf);
        free(clients_cache);
        return EXIT_SUCCESS; //ingen teknisk feil
    }
    timeout = atoi(argv[4]);

    probability = ((float)atof(argv[5]))/100.0;
    if ( (probability > 1) || (probability < 0) ) { //Må være et tall mellom 0 og 1, ETTEr delt på 100
        fprintf(stderr, "Tapssannsynlighet must be in range 0-100 \n");
        free(send_buf);
        free(recv_buf);
        free(compare_buf);
        free(clients_cache);
        return EXIT_SUCCESS; //ingen teknisk feil
    }

    //Bruker srand48 fordi drand48() ikke fungerer på WSL (Lært fra gruppetime)
    time_t sekunder;
    sekunder = time(NULL);
    srand48(sekunder);
    set_loss_probability(probability);

    //Setter hints sitt minne til 0 for at den skal være initialisert og setter feltene vi trenger
    memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

    //Henter info om servers adresse og port og lagrer det i en struct addrinfo
	int status = getaddrinfo((char*)ip, port, &hints, &server);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        free(send_buf);
        free(recv_buf);
        free(compare_buf);
        free(clients_cache);
        return EXIT_FAILURE;
    }
    //Socket for kommunikasjon
    int sock = socket(server->ai_family, server->ai_socktype, 0);
    if (sock == -1){
        perror("socket");
        free(send_buf);
        free(recv_buf);
        free(compare_buf);
        free(clients_cache);
        freeaddrinfo(server);
        return EXIT_FAILURE;
    }
    
    //Adressen meldinger kommer fra lagres i structen: recv_addr
    struct sockaddr_in recv_addr;
    socklen_t recv_len = sizeof(struct sockaddr_in);
    
    fd_set readfds;
    struct timeval tv;
    int rc;

    //For heartbeat/REG meldinger. Sender melding hver 10.sek, men hvis timeout er 
    time_t seconds_Before = time(NULL) - 10; //starter med å sende en REG-melding
    time_t seconds_Now;
    int seconds_since_REG_ACK; //sekunder siden siste ACK på REG/Heartbeat-melding

    time_t seconds_when_sent = time(NULL) - timeout; //Starter med å sende REG-melding
    int seconds_since_REG_sent; //Sekunder siden siste REG/Heartbeat sendt
    
    while (1) {

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds); //Lytter til socket
        FD_SET(STDIN_FILENO, &readfds); //Lytter til tastatur

        //Tiden som maskimalt ventes for å få et svar
        tv.tv_sec = 0; 
        tv.tv_usec = 10000; // 10 ms, Setter en liten ventetid i select for å få mest presisjon ved timeout blant sendte meldinger.


        //Sjekker om det er behov for å sende en ny melding:
        seconds_Now = time(NULL);
        seconds_since_REG_ACK = (int)(seconds_Now - seconds_Before); //tiden siden forrige "ACK num OK" fra server på REG melding

        //Sjekker om tiden siden forrige sending er over timeout:
        seconds_since_REG_sent = (int) (seconds_Now - seconds_when_sent);

        if ( (seconds_since_REG_ACK >= 10) && (seconds_since_REG_sent >= timeout) ) { //Sender registreringsmelding hvert 10 sekund og når timeout har gått ut

            fprintf(stderr, "\nSENDER REG/Heartbeat MELDING\n");
            if (seconds_since_REG_ACK > (30+timeout) ) { //Hvis ingen kontakt med server etter 30 sekunder + timeout så avslutter programmet (uten + timeout så ville programmet avsluttes hvis timeout er større enn 30 sek.)
                fprintf(stderr, "Ingen kontakt med server\n");
                break;
            }
            char REG_string[SMALL_BUFSIZE];
            sprintf(REG_string, "%d", REG_nummer); //Gjør om int til char*
            generate_string(&send_buf, "PKT\0", REG_string, "REG\0", nick, "", "", "", "");

            //printf("Sender REG melding: %s\n", send_buf);
            int wc = send_packet(sock, send_buf, strlen(send_buf), 0, server->ai_addr, server->ai_addrlen);
            if (wc == -1){
                perror("send_packet");
                break;
            }
            
            seconds_when_sent = time(NULL); //Lagrer tiden når REG/Heartbeat er sendt
            REG_Alle++;
        } 
        


        rc = select(sock+1, &readfds, NULL, NULL, &tv); //Venter maksimalt timeout som er gitt
        if (rc == -1){
            perror("select");
            slutt_status = EXIT_FAILURE;
            break;
        }

        //Hvis melding fra nettet inn på socket
        if (FD_ISSET(sock, &readfds)){
            //En melding fra nettet
            rc = recvfrom(sock, recv_buf, MSG_BUFSIZE-1, 0, (struct sockaddr*)&recv_addr, &recv_len);
            if (rc == -1){
                perror("recvfrom");
                slutt_status = EXIT_FAILURE;
                break;
            }
            recv_buf[rc] = 0;
            //printf("Motokk: %s\n", recv_buf);

            //Lagrer nummeret til pakken som ble mottatt
            char recv_num_string[SMALL_BUFSIZE];
            char* recv_num_str = extract_num_from_string(recv_buf, recv_num_string);
            int recv_num = atoi(recv_num_str);

            char* recv_ip = inet_ntoa(recv_addr.sin_addr);
            int recv_port = ntohs(recv_addr.sin_port);
           
            if (strncmp(recv_buf, "PKT\0", 3) == 0) { //Hvis meldingen kommer fra en annen klient
                //Sjekker om meldingen er på formatet: "PKT nummer FROM fra_nick TO nick MSG tekst"
                printf("\nMotokk: %s\n", recv_buf);
                char* from_nick = malloc(NICK_SIZE);
                if (from_nick == NULL) {
                    fprintf(stderr, "malloc failed. possibly out of memory\n");
                    slutt_status = EXIT_FAILURE;
                    break;
                }
                char* tekst = malloc(TEXT_SIZE);
                if (tekst == NULL) {
                    fprintf(stderr, "malloc failed. possibly out of memory\n");
                    slutt_status = EXIT_FAILURE;
                    break;
                }

                status = check_if_PKT_MSG(recv_buf, nick, &from_nick, &tekst); //Sjekker om meldingen har fortmatet til en pakke-melding fra en annen klient
                int blocked = check_if_blocked(from_nick, &blocked_nicks); //sjekker om nick er blokkert

               
                if (status == -1){ //Feil format på melding
                    generate_string(&send_buf, "ACK\0", recv_num_str, "WRONG FORMAT\0", "", "", "", "", "");

                } else if (status == 0) { //Feil mottaker på melding
                    generate_string(&send_buf, "ACK\0", recv_num_str, "WRONG NAME\0", "", "", "", "", "");

                } else { //Rikitg mottaker og format på melding
                    generate_string(&send_buf, "ACK\0", recv_num_str, "OK\0", "", "", "", "", "");

                    if (!blocked) { //Hvis nick ikke er blokkert
                        
                        int index = search_for_client(clients_cache, nick, ant_indexes_init);
                        int index2 = search_for_client_IP_PORT(clients_cache, recv_ip, recv_port, ant_indexes_init); //Sjekker om det kan være en ny klient som er registrert med samme nick

                        if ( (index == -1) || (index2 == -1) ) { //Ingen melding motatt tidligere fordi klient ikke er kjent

                            //Lager klient og lagrer klient i client-cache:
                            struct client* new_client = malloc(sizeof(struct client));
                            if (new_client == NULL) {
                                fprintf(stderr, "malloc failed. possibly out of memory\n");
                                slutt_status = EXIT_FAILURE;
                                break;
                            }
                            new_client->nick = strdup(from_nick);
                            new_client->ip = strdup(inet_ntoa(recv_addr.sin_addr));
                            new_client->port = recv_port;
                            new_client->meldinger_sent = NULL;
                            new_client->last_PKT_num = recv_num;

                            status = set_in_client(clients_cache, new_client, ant_indexes_init);
                            if (status == 1) { //Lagt til på slutten
                                //printf("Oppdaterer indexes med 1.\n");
                                ant_indexes_init++;
                                ant_clients++;
                            } else if (status == 0 ) {
                                ant_clients++;
                            }
                            //printf("Ny klient: ");
                            printf("[%s] %s\n", from_nick, tekst); //Skriver ut meldingen @Ida hei @Sindre hello tilbake

                        } else { //En melding er motatt tidligere
                            struct client* stored_client = (struct client*) clients_cache[index];

                            if (stored_client->last_PKT_num != recv_num) { //Sjekker at det ikke er duplikat av forrige melding ELLER om det er fra en ny klient med likt navn
                                printf("[%s] %s\n", from_nick, tekst); //Skriver ut meldingen
                            }
                            stored_client->last_PKT_num = recv_num; //setter denne pakkens nummer til å være det nyligste pakke nummeret som er motatt

                        }
                    }

                }
                
                if (!blocked) { //Hvis nick ikke er blokkert

                    //printf("Sender: %s\n", send_buf);
                    int wc = send_packet(sock, send_buf, strlen(send_buf), 0, (struct sockaddr*)&recv_addr, sizeof(struct sockaddr_in));
                    if (wc == -1){
                        perror("send_packet");
                        slutt_status = EXIT_FAILURE;
                        break;
                    }
            
                }

                free(from_nick);
                free(tekst);

            } else if (strncmp(recv_buf, "ACK", 2) == 0) { //Hvis en svar melding 

                //Finner klienten som meldingen kom fra:
                int indeks = search_for_client_IP_PORT(clients_cache, recv_ip, recv_port, ant_indexes_init);

                if (indeks != -1) { //Klient funnet
                    fprintf(stderr, "Motokk ACK-melding: %s\n", recv_buf);
                    struct client* stored_client = (struct client*) clients_cache[indeks];
                    //Slett melding med samme nmmer
                    fprintf(stderr, "Sletter melding med PKT-nummer %d fra meldingslisten til %s\n", recv_num, stored_client->nick);
                    struct melding *updated_meldinger = slett_melding(stored_client->meldinger_sent, recv_num);
                    stored_client->meldinger_sent = updated_meldinger;
                    ant_PKT_success++;

                    fprintf(stderr, "Lenkeliste nå: ");
                    print_meldinger(stored_client->meldinger_sent);
                    printf("\n");
                    
                } else { //klient funnet

                    //sjekker om ACK kom fra server
                    char* server_ip = (char*) ip;
                    if ( (strcmp(recv_ip, server_ip) == 0) && (recv_port ==  atoi(port)) ){
                        fprintf(stderr, "Motokk REG/Heartbeat svar fra server: %s\n", recv_buf);
                        // seconds_since_REG_ACK = 0;
                        seconds_Before = time(NULL); //Brukes for neste registrering
                        seconds_since_REG_sent = timeout; //Settes i tilfelle timeout er større enn 10 sekunder
                        REG_nummer++;

                    } else { //ACK kom ikke fra noen melding vi har sendt
                        fprintf(stderr, "Ukjent sender av ACK\n");
                    }

                }
                
            }
            
        } else if (FD_ISSET(STDIN_FILENO, &readfds)){ //Ser om det har kommet input fra STDIN og legger isåfall melding inn i send_meldinger-leneklisten

            //Leser fra stdin
            char stdin_buf[MSG_BUFSIZE];
            fgets(stdin_buf, MSG_BUFSIZE, stdin);
            if (stdin_buf[strlen(stdin_buf) - 1] == '\n') {
                stdin_buf[strlen(stdin_buf) - 1] = '\0'; //Fjerner newline
            }

            //Sjekker om meldingen signaliserer at program skal avsluttes
            if (strcmp(stdin_buf, end_msg) == 0){
                printf("Avslutter..\n");
                break;
            } 

            //Sjekker hvordan melding som nå skal analyseres
            if (strncmp(stdin_buf, "BLOCK", 5) == 0) { //Blokkerer en nick
                char *block_nick;
                char *start = stdin_buf + strlen("BLOCK")+1; //Starter på fra_nick
                block_nick = strdup(start);
                //printf("BLOCKING %s\n", block_nick);  

                //Setter den blokkerte inn på starten av listen
                struct blocked_nick *new_block = malloc(sizeof(struct blocked_nick));
                if (new_block == NULL) {
                    fprintf(stderr, "malloc failed. possibly out of memory\n");
                    slutt_status = EXIT_FAILURE;
                    break;
                }
                struct blocked_nick *tmp;
                tmp = blocked_nicks;
                new_block->nick = block_nick;
                new_block->neste = tmp;
                blocked_nicks = new_block;
                continue;

            } else if (strncmp(stdin_buf, "UNBLOCK", 7) == 0) { //Opphever blokkering av en nick
                char* block_nick = stdin_buf + strlen("UNBLOCK")+1; //Starter på fra_nick
                if (blocked_nicks != NULL) {
                    unblock_nick(block_nick, &blocked_nicks); //Unblocker nicken
                }
                continue;

            } else if (stdin_buf[0] != '@') {
                printf("** Feil format på input. Prøv '@til_nick: tekst'\n");
                continue;

            } else { //Melding starter på '@'

                int failed = 0;
                char c;
                char to_nick[NICK_SIZE+1];
                int k = 1;
                for (int i = 1; i < NICK_SIZE + 2; i++){
                    c = stdin_buf[i];
                    if (isspace(c)) {
                        k++;
                        to_nick[i-1] = '\0';
                        break;
                    }
                    else if ( (c == '\0') || (!isascii(c)) || ( (i == NICK_SIZE-1) && (!isspace(stdin_buf[i+1])) ) ) {
                        printf("** Meldingen må være på formatet: '@til_nick: tekst' og NICK kan ikke inneholde mellomrom, tabulator, retur eller være større enn 20 byte. Prøv igjen: \n");
                        failed = 1;
                        break;

                    } else {
                        to_nick[i-1] = c;
                        k++;
                    } 
                }

                if (check_if_blocked(to_nick, &blocked_nicks) == 1) { //nick er blokkert og kan ikke sendes melding til
                    printf("NICK: %s is BLOCKED\n", to_nick);
                    continue;
                }
                if (failed == 1) {
                    continue;
                }
                char tekst[TEXT_SIZE + 1]; //pluss nullbyte
                for (int i = k; i < TEXT_SIZE + k + 1; i++){
                    c = stdin_buf[i];
                    if ( c == '\0' || (i == TEXT_SIZE) ) {
                        tekst[i-k] = '\0';
                        break;
                    } else {
                        tekst[i-k] = c;
                    }
                }

                //fprintf(stderr, "Sender melding %s\n", tekst);
                printf("[ME] %s\n", tekst); 

                int indeks = search_for_client(clients_cache, to_nick, ant_indexes_init); //returner indeksen til der klienten ble funnet
                struct client* client = malloc(sizeof(struct client));
                if (client == NULL) {
                    fprintf(stderr, "malloc failed. possibly out of memory\n");
                    slutt_status = EXIT_FAILURE;
                    break;
                }
                //Initialiserer structen:
                char *init = "tmp";
                client->nick = strdup(init);
                client->ip = strdup(init);
                client->port = 0;
                client->last_PKT_num = -1;
                client->meldinger_sent = NULL;
            

                if (indeks == -1) { //Fant ikke klient i cache. Sender oppslagsmelding og spør om klientens informasjon
                    printf("Sender oppslagsmelding til server\n");
                    char lookup_string[SMALL_BUFSIZE];
                    sprintf(lookup_string, "%d", OPPSLAG_nummer); //Gjør om int til char*
                    generate_string(&send_buf, "PKT\0", lookup_string, "LOOKUP", to_nick, "", "", "", "");

                    status = send_lookup(sock, send_buf, recv_buf, server, timeout, &OPPSLAG_Alle); //Skriver ut feilmelding hvis melding ikke er motatt innen tidsavbruddsperioden 2 ganger
                    if (status == -1) { 
                        //send_lookup funksjonen kaller perror med rikitg feilmelding.
                        free_client(client);
                        break;

                    } else if (status == 0) {//Ingen melding motatt innen tidsperioden, prøvd 2 ganger
                        fprintf(stderr, "Ingen melding motatt innen tidsavbruddperioden under OPPSLAG av klient \n");
                        free_client(client);
                        printf("Avslutter..\n");
                        break;
                    }  

                    fprintf(stderr, "Motokk fra server: %s\n", recv_buf);
                    status = check_lookup_message_recv(recv_buf, compare_buf, OPPSLAG_nummer); //Sjekker om formatet på meldingen man fikk fra server
                    OPPSLAG_nummer++;

                    if ( status == -1) { //Meldingen samsvarer ingen kjent format

                        fprintf(stderr, "Ukjent ACK fra server\n");
                        free_client(client);
                        continue;

                    } else if (status == 0){ //NICK er ikke registrert
                        fprintf(stderr, "NICK %s NOT REGISTERED\n", to_nick);
                        free_client(client);
                        continue;

                    } else if (status == 1) { //fikk meldig tilbake med informasjon om klienten

                        get_info_from_lookup_msg(&client, recv_buf, to_nick); //Lagrer informasjon i structen: client

                        //Lagrer klienten i client-cache
                        if (ant_indexes_init >= max_clients-1) { //Hvis mer minne trengs
                            clients_cache = realloc(clients_cache, sizeof(struct client*) *(max_clients*2)); //Dobbler størrelsen på cache
                            max_clients = max_clients*2;
                            printf("Setter av mer minne! Nytt minne: %d og gammelt minne: %d\n", max_clients, max_clients/2 );
                        }

                        status = set_in_client(clients_cache, client, ant_indexes_init); //Enten på en plass som har blitt slettet eller på slutten hvor ant_indexes_init blir oppdatert
                        if (status == 1) { //Lagt til på slutten
                            ant_indexes_init++;
                            ant_clients++;
                        } else if (status == 0 ) {
                            ant_clients++;
                        }
                        
                    } 
                    
                } else { //Fant klient i cache
                    free_client(client);
                    client = (struct client *) clients_cache[indeks];
                }

                char PKT_string[SMALL_BUFSIZE];
                sprintf(PKT_string, "%d", PKT_nummer); //Gjør om int til char*
                send_buf = generate_string(&send_buf, "PKT\0", PKT_string, "FROM\0", nick, "TO\0", to_nick, "MSG\0", tekst);
                
                //Lagrer meldingen i meldinger_sent lenkelisten i client
                struct melding* ny_melding = malloc(sizeof(struct melding));
                if (ny_melding == NULL) {
                    fprintf(stderr, "malloc failed. possibly out of memory\n");
                    slutt_status = EXIT_FAILURE;
                    break;
                }
                ny_melding->tekst = strdup(tekst);
                ny_melding->format_msg = strdup(send_buf);
                ny_melding->time_sent = time(NULL);
                ny_melding->tries = 1;
                ny_melding->neste = NULL;
                ny_melding->nummer = PKT_nummer;

                add_melding(ny_melding, &(client->meldinger_sent));

                fprintf(stderr, "Satt inn melding. Ny lenkeliste: ");
                print_meldinger(client->meldinger_sent);

                //freeraddrinfo hvi tidligere initialisert
                if (client_init_bool) {
                    freeaddrinfo(client_friend);
                    client_init_bool = 0;
                }

                //sender melding på formen "PKT nummer FROM fra_nick TO to_nick MSG tekst"
                rc = send_format_MSG(client, client_init_bool, sock, PKT_nummer, nick, send_buf, &client_friend, hints);
                if (rc < 0) {
                    if (rc == -2) { //Feil ved sending etter initialisering av addrinfo
                        client_init_bool = 1;
                    }
                    slutt_status = EXIT_FAILURE;
                    break;
                }
                client_init_bool = 1; //Forteller at getaddrinfo må frigjøres
                PKT_Alle++;
                PKT_nummer++;
                fprintf(stderr, "Sendt melding\n\n");
                
            } 



        } else { //Hvis ingen påvirket select

            //Sjekker hvilke klienter som ikke har motatt ACK (tiden siden sist er lengre enn eller lik timeout). Altså time_since_sent >= timeout.
            int failed = 0;
            for (int i = 0; i < ant_indexes_init; i++){
                struct client* stored_client = (struct client*) clients_cache[i];
                struct melding* melding = stored_client->meldinger_sent;

                if (stored_client->meldinger_sent != NULL) {

                    time_t time_now = time(NULL); //Lagrer tiden nå
                    time_t time_since_sent = time_now - (melding->time_sent);

                    if ( time_since_sent >= (time_t)timeout ) {
                        //fprintf(stderr, "Melding '%s' har blitt forsøkt sendt %d ganger og har ikke mottatt svar på %d sekunder siden sist sending\n", melding->tekst, melding->tries, time_since_sent);
                    
                        if ( (melding->tries == 1) || (melding->tries == 3)) {//Sender melding en gang til ELLER sender siste gangen
                            fprintf(stderr, "Melding '%s' er på forsøk %d\n", melding->format_msg, melding->tries);

                            char port_streng[SMALL_BUFSIZE];
                            sprintf(port_streng, "%d", stored_client->port); //Gjør om int til char*
                            if (client_init_bool) {
                                freeaddrinfo(client_friend);
                                client_init_bool = 0;
                            }
                            int status = getaddrinfo(stored_client->ip, port_streng, &hints, &client_friend);
                            if (status != 0) {
                                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
                                failed = 1;
                                break;
                            }
                            client_init_bool = 1;
                            melding->time_sent = time(NULL);
    
                            int rc = send_packet(sock, melding->format_msg, strlen(melding->format_msg), 0, client_friend->ai_addr, client_friend->ai_addrlen);
                            if (rc == -1) {
                                perror("send_packet");
                                slutt_status = EXIT_FAILURE;
                                failed = 1;
                                break;
                            }
                            PKT_Alle++;
                            melding->tries += 1;
                            fprintf(stderr, "Sendt melding\n\n");


                        } else if (melding->tries == 2){ //Gjær et nytt oppslag av nick og sender
                            fprintf(stderr, "Melding '%s' er på forsøk %d og gjør nytt OPPSLAG og sender en gang til\n", melding->format_msg, melding->tries);

                            char lookup_string[SMALL_BUFSIZE];
                            sprintf(lookup_string, "%d", OPPSLAG_nummer); //Gjør om int til char*
                            generate_string(&send_buf, "PKT\0", lookup_string, "LOOKUP", stored_client->nick, "", "", "", "");

                            status = send_lookup(sock, send_buf, recv_buf, server, timeout, &OPPSLAG_Alle); //Skriver ut feilmelding hvis melding ikke er motatt innen tidsavbruddsperioden 2 ganger
                            
                            if (status == -1) {
                                //send_lookup funksjonen kaller perror med rikitg feilmelding.
                                failed = 1;
                                break;
                            
                            } else if (status == 0) {//Ingen melding motatt innen tidsperioden, prøvd 2 ganger
                                fprintf(stderr, "Ingen melding motatt innen tidsavbruddperioden under OPPSLAG av klient \n");
                                printf("Avslutter..\n");
                                failed = 1;
                                break;
                            }  
                            status = check_lookup_message_recv(recv_buf, compare_buf, OPPSLAG_nummer);
                            OPPSLAG_nummer++;
                            fprintf(stderr, "Motokk fra Server: %s\n", recv_buf);
                            if ( status == -1) { //Meldingen samsvarer ingen kjent format
                                fprintf(stderr, "Ukjent ACK fra server\n");
                                struct melding *updated_meldinger = slett_melding(stored_client->meldinger_sent, melding->nummer); //Sletter meldingen som ikke kom fram
                                stored_client->meldinger_sent = updated_meldinger; //Lenkelisten uten elementet som ble slettet
                                continue;

                            } else if (status == 0){ //NICK er ikke registrert
                                fprintf(stderr, "NICK %s NOT REGISTERED\n", stored_client->nick);
                                struct melding *updated_meldinger = slett_melding(stored_client->meldinger_sent, melding->nummer); //Sletter meldingen som ikke kom fram
                                stored_client->meldinger_sent = updated_meldinger; //Lenkelisten uten elementet som ble slettet
                                continue;

                            } else if (status == 1) { //Fikk meldig tilbake med informasjon om klienten i rikitg format

                                struct client* client = malloc(sizeof(struct client));
                                if (client == NULL) {
                                    fprintf(stderr, "malloc failed. possibly out of memory\n");
                                    slutt_status = EXIT_FAILURE;
                                    failed = 1;
                                    break;
                                }
                                //Initialiserer structen:
                                char *init = "tmp";
                                client->nick = strdup(init);
                                client->ip = strdup(init);
                                client->port = 0;
                                client->last_PKT_num = stored_client->last_PKT_num;
                                client->meldinger_sent = NULL;

                                get_info_from_lookup_msg(&client, recv_buf, stored_client->nick); //Lagrer informasjon i structen: client

                                struct melding* copied_list = malloc(sizeof(struct melding));
                                cpy_melding_linkedlist(stored_client->meldinger_sent, &copied_list); //Kopier leneklisten til den nye klienten eventuelt funnet.

                                client->meldinger_sent = copied_list;                            

                                //Lagrer klienten i client-cache
                                if (ant_indexes_init >= max_clients) { //Hvis mer minne trengs
                                    clients_cache = realloc(clients_cache, sizeof(struct client*) *(max_clients*2)); //Dobbler størrelsen på cache
                                    max_clients = max_clients*2;
                                }

                                status = set_in_client(clients_cache, client, ant_indexes_init); //Enten på en plass som har blitt slettet eller på slutten hvor ant_indexes_init blir oppdatert
                                if (status == 1) { //Lagt til på slutten
                                    ant_indexes_init++;
                                    ant_clients++;
                                } else if (status == 0 ) {
                                    ant_clients++;
                                }

                                //Sender medlingen til den nye klienten informasjonen som er motatt:
                                char port_streng[SMALL_BUFSIZE];
                                sprintf(port_streng, "%d", client->port); //Gjør om int til char*
                                
                                if (client_init_bool) {
                                    freeaddrinfo(client_friend);
                                    client_init_bool = 0;
                                }

                                int status = getaddrinfo(client->ip, port_streng, &hints, &client_friend);
                                if (status != 0) {
                                    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
                                    failed = 1;
                                    break;
                                }
                                client_init_bool = 1;

                                client->meldinger_sent->time_sent = time(NULL);

                                int rc = send_packet(sock, client->meldinger_sent->format_msg, strlen(client->meldinger_sent->format_msg), 0, client_friend->ai_addr, client_friend->ai_addrlen);
                                if (rc == -1) {
                                    perror("send_packet");
                                    slutt_status = EXIT_FAILURE;
                                    failed = 1;
                                    break;
                                }
                                client->meldinger_sent->tries += 1;
                                PKT_Alle++;
                                fprintf(stderr, "Sendt melding\n\n");

                            } 
                            
                        } else { //NICK er utilgjengelig
                            fprintf(stderr, "NICK %s UNREACHABLE\n", stored_client->nick);
                            struct melding *updated_meldinger = slett_melding(stored_client->meldinger_sent, melding->nummer); //Sletter meldingen som ikke kom fram
                            stored_client->meldinger_sent = updated_meldinger; //Lenkelisten uten elementet som ble slettet
                        }
                    }
                }
                    
            }
            
            if (failed) { //Hvis oppslag av en nick failet. Ikke kontakt med server
                break;
            }

	    }
    }  
    //Lukker og free'er alt fra heapen:
    free_clients(clients_cache, ant_indexes_init);
    free(recv_buf);
    free(send_buf);
    free(compare_buf);
    free(clients_cache);
    close(sock);
    freeaddrinfo(server);
    if (client_init_bool) {
        freeaddrinfo(client_friend);
    }
    free_blocked(&blocked_nicks);

    //Informasjon om hvor mange av hver pakketype som ikke fikk en ACK tilbake: (Det vil si at pakken kan også ha gått tapt fra server og til klient)
    printf("|-----------------------------------------------------------\n");
    printf("|   Pakketap: \n");
    printf("|       > %d av totalt %d MSG-pakker ble tapt. \n", (PKT_Alle - ant_PKT_success), PKT_Alle);
    printf("|       > %d av totalt %d OPPSLAG-pakker ble tapt. \n", (OPPSLAG_Alle - OPPSLAG_nummer), OPPSLAG_Alle);
    printf("|       > %d av totalt %d REG/HEARTBEAT-pakker ble tapt. \n", (REG_Alle - REG_nummer), REG_Alle);
    printf("|       > %d klienter i cache ved slutt.\n", ant_clients);
    printf("|-----------------------------------------------------------\n");

    return slutt_status;
}