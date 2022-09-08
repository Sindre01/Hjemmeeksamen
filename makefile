CFLAGS = -g -std=gnu11 -Wall -Wextra
VFLAGS = --track-origins=yes --malloc-fill=0x40 --free-fill=0x23 --leak-check=full --show-leak-kinds=all
BIN = upush_client upush_server

all: $(BIN)

upush_client: upush_client.o send_packet.o common.o client_funcs.o
	gcc $(CFLAGS) $^ -o $@

upush_server: upush_server.o send_packet.o common.o server_funcs.o
	gcc $(CFLAGS) $^ -o $@

testClient: upush_client
	valgrind $(VFLAGS) ./upush_client Sindre 127.0.0.1 2015 2 20

testServer: upush_server
	valgrind $(VFLAGS) ./upush_server 2015 20

upush_client.o: upush_client.c
	gcc $(CFLAGS) -c $^ -o $@

upush_server.o: upush_server.c
	gcc $(CFLAGS) -c $^ -o $@

%.o: %.c
	gcc -g -c $^

clean:
	rm -f $(BIN) *.o