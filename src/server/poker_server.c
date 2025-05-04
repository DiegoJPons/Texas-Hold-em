#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "poker_client.h"
#include "client_action_handler.h"
#include "game_logic.h"

#define BASE_PORT 2201
#define NUM_PORTS 6
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    struct sockaddr_in address;
} player_t;

game_state_t game; //global variable to store our game state info (this is a huge hint for you)

int main(int argc, char **argv) {
    int server_fds[NUM_PORTS], client_socket, player_count = 0;
    int opt = 1;
    struct sockaddr_in server_address;
    player_t players[MAX_PLAYERS];
    memset(players, 0, sizeof(players));
    char buffer[BUFFER_SIZE] = {0};
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int port_nums[NUM_PORTS];

    for (int i = 0; i < NUM_PORTS; i++) {
        port_nums[i] = BASE_PORT + i;
    }


    int rand_seed = argc == 2 ? atoi(argv[1]) : 0;
    init_game_state(&game, 100, rand_seed);


    for(int i = 0; i < NUM_PORTS; i++) {
        if ((server_fds[i] = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        if (setsockopt(server_fds[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
            perror("setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))");
            exit(EXIT_FAILURE);
        }

        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(port_nums[i]);

        if (bind(server_fds[i], (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
            perror("[Server] bind() failed.");
            exit(EXIT_FAILURE);
        }

        if (listen(server_fds[i], 3) < 0) {
            perror("[Server] listen() failed.");
            exit(EXIT_FAILURE);
        }

        printf("[Server] Listening on port %d\n", port_nums[i]);
    }

    fd_set read_fds;
    int max_fd = 0;

    while (1) {
        FD_ZERO(&read_fds);
        max_fd = 0;

        for (int i = 0; i < NUM_PORTS; i++) {
            FD_SET(server_fds[i], &read_fds);
            if (server_fds[i] > max_fd) {
                max_fd = server_fds[i];
            }
        }

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select error");
            continue;
        }

        for (int i = 0; i < NUM_PORTS; i++) {
            if (FD_ISSET(server_fds[i], &read_fds)) {
                if (players[i].socket != 0) {
                    int temp = accept(server_fds[i], NULL, NULL);
                    close(temp);  // Reject extra connection
                    printf("[Server] Rejected extra connection on port %d (player already assigned).\n", port_nums[i]);
                    continue;
                }

                client_socket = accept(server_fds[i], (struct sockaddr *)&players[i].address, &addrlen);
                if (client_socket < 0) {
                    perror("accept error");
                    continue;
                }

                players[i].socket = client_socket;
                game.sockets[i] = players[i].socket;
                player_count++;
                printf("[Server] Player %d connected on port %d (socket %d)\n", i + 1, port_nums[i], client_socket);
            }
        }

        // Optional: break loop once all players are connected
        if (player_count == NUM_PORTS) {
            printf("[Server] All players connected.\n");
            break;
        }
    }

    printf("[Server] Current game sockets:\n");
    for (int i = 0; i < MAX_PLAYERS; i++) {
        printf("  Player %d: socket %d\n", i + 1, game.sockets[i]);
    }

    //Setup the server infrastructre and accept the 6 players on ports 2201, 2202, 2203, 2204, 2205, 2206

    //Join state?

    server_join(&game);

    print_game_state(&game);
    while (1) {
       
        printf("Inside while loop\n");
        // READY
        if (!server_ready(&game)) {
            printf("[Server] Not enough players for this round.\n");
            break;
        }

        // PREFLOP
        printf("PREFLOP ROUND\n");
        game.round_stage = ROUND_PREFLOP;
        server_deal(&game);

        // FLOP
        printf("FLOP ROUND\n");
        game.round_stage = ROUND_FLOP;
        server_deal(&game);

        // TURN
        printf("TURN ROUND\n");
        game.round_stage = ROUND_TURN;
        server_deal(&game);

        // RIVER
        printf("RIVER ROUND\n");
        game.round_stage = ROUND_RIVER;
        server_deal(&game);

        // SHOWDOWN
        printf("SHOWDOWN ROUND\n");
        game.round_stage = ROUND_SHOWDOWN;
        server_end(&game);


        reset_game_state(&game); 
    }

    printf("[Server] Shutting down.\n");

    // Close all fds (you're welcome)
    for (int i = 0; i < MAX_PLAYERS; i++) {
        close(server_fds[i]);
        if (game.player_status[i] != PLAYER_LEFT) {
            close(game.sockets[i]);
        }
    }

    return 0;
}

