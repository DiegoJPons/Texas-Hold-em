#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "poker_client.h"
#include "client_action_handler.h"
#include "game_logic.h"
#define max(a,b) ((a) > (b) ? (a) : (b))

//Feel free to add your own code. I stripped out most of our solution functions but I left some "breadcrumbs" for anyone lost

// for debugging
void print_game_state( game_state_t *game){
    printf("\n--- Game State Debug ---\n");

    printf("Dealer: %d\n", game->dealer_player);
    printf("Current Player: %d\n", game->current_player);
    printf("Round Stage: %d\n", game->round_stage);
    printf("Next Card Index: %d\n", game->next_card);
    printf("Pot Size: %d\n", game->pot_size);
    printf("Highest Bet: %d\n", game->highest_bet);
    printf("Num Players: %d\n", game->num_players);

    int active_count = 0;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        printf("Player %d:\n", i);
        printf("Socket: %d\n", game->sockets[i]);
        printf("Status: %d\n", game->player_status[i]);
        printf("Stack: %d\n", game->player_stacks[i]);
        printf("Current Bet: %d\n", game->current_bets[i]);
        printf("Hand: [%d, %d]\n", game->player_hands[i][0], game->player_hands[i][1]);

        if (game->player_status[i] == PLAYER_ACTIVE) {
            active_count++;
        }
    }

    printf("Active Players: %d\n", active_count);

    printf("Community Cards: [");
    for (int i = 0; i < MAX_COMMUNITY_CARDS; i++) {
        printf("%d", game->community_cards[i]);
        if (i < MAX_COMMUNITY_CARDS - 1) printf(", ");
    }
    printf("]\n");

    printf("--- End of Game State ---\n\n");
}

void init_deck(card_t deck[DECK_SIZE], int seed){ //DO NOT TOUCH THIS FUNCTION
    srand(seed);
    int i = 0;
    for(int r = 0; r<13; r++){
        for(int s = 0; s<4; s++){
            deck[i++] = (r << SUITE_BITS) | s;
        }
    }
}

void shuffle_deck(card_t deck[DECK_SIZE]){ //DO NOT TOUCH THIS FUNCTION
    for(int i = 0; i<DECK_SIZE; i++){
        int j = rand() % DECK_SIZE;
        card_t temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

//You dont need to use this if you dont want, but we did.
void init_game_state(game_state_t *game, int starting_stack, int random_seed){
    memset(game, 0, sizeof(game_state_t));
    init_deck(game->deck, random_seed);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game->player_stacks[i] = starting_stack;
    }
}

void reset_game_state(game_state_t *game) {
    //Call this function between hands.
    //You should add your own code, I just wanted to make sure the deck got shuffled.
    shuffle_deck(game->deck);
    game->next_card = 0;
   
    for(int i = 0; i < MAX_PLAYERS; i++) {
        if(game->player_status[i] == PLAYER_FOLDED && game->sockets[i] != -1){
            game->player_status[i] = PLAYER_ACTIVE;
        }
        game->current_bets[i] = 0;
        if (game->player_status[i] == PLAYER_ACTIVE) {
            game->player_hands[i][0] = NOCARD; 
            game->player_hands[i][1] = NOCARD;
        }
    }

    for (int i = 0; i < MAX_COMMUNITY_CARDS; i++) {
        game->community_cards[i] = NOCARD;
    }

    game->pot_size = 0;
    game->highest_bet = 0;
    game->dealer_player = (game->dealer_player + 1) % MAX_PLAYERS;
    while (game->player_status[game->dealer_player] != PLAYER_ACTIVE) {
        game->dealer_player = (game->dealer_player + 1) % MAX_PLAYERS;
    }

    game->round_stage = ROUND_INIT;
}

void remove_player(game_state_t *game, player_id_t pid) {
    game->player_status[pid] = PLAYER_LEFT;
    close(game->sockets[pid]);
    game->sockets[pid] = -1;
    game->num_players--;
}

void server_join(game_state_t *game) {
    //This function was called to get the join packets from all players
    for(int i = 0; i < MAX_PLAYERS; i++) {
        client_packet_t p;

        int bytes_read = recv(game->sockets[i], &p, sizeof(client_packet_t), 0);
        if (bytes_read <= 0) {
            printf("Error reading bytes from player %d.\n", i);
            continue;
        }

        if(p.packet_type == JOIN) {
            game->player_status[i] = PLAYER_ACTIVE;
        }
    }
}

void set_first(game_state_t *game) {
    for (int i = 1; i <= MAX_PLAYERS; i++) {
        int player = (game->dealer_player + i) % MAX_PLAYERS;
        if (player != game->dealer_player && (game->player_status[player] == PLAYER_ACTIVE || game->player_status[player] == PLAYER_ALLIN)) {
            game->current_player = player;
            return;
        }
    }
    game->current_player = -1; 
}

void move_to_next_player(game_state_t *game) {
    for (int i = 1; i <= MAX_PLAYERS; i++) {
        int player = (game->current_player + i) % MAX_PLAYERS;
        if (game->player_status[player] == PLAYER_ACTIVE || game->player_status[player] == PLAYER_ALLIN) {
            game->current_player = player;
            return;
        }
    }
    game->current_player = -1;
}

int server_ready(game_state_t *game) {
    //This function updated the dealer and checked ready/leave status for all players
    int ready = 0;
    player_id_t last_ready = 0;
    for(int i = 0; i < MAX_PLAYERS; i++) {
        if (game->player_status[i] == PLAYER_LEFT) {
            continue;
        }

        client_packet_t p;
    
        int bytes_read = recv(game->sockets[i], &p, sizeof(client_packet_t), 0);

        if (bytes_read <= 0) {
            printf("Player %d disconnected unexpectedly.\n", i + 1);
            game->player_status[i] = PLAYER_LEFT;
            close(game->sockets[i]);
            game->sockets[i] = -1;
            continue;
        }

        if (p.packet_type == READY){
            game->player_status[i] = PLAYER_ACTIVE;
            ready++;
            last_ready = i;
        }
        else if (p.packet_type == LEAVE){
            remove_player(game, i);
        }
    }

    game->num_players = ready;
    
    if(ready < 2) {
        if(ready == 1) {
            server_packet_t out; 
            out.packet_type = HALT;
            send(game->sockets[last_ready], &out, sizeof(server_packet_t), 0);
        }
        return -1;
    }

    for(int i = game->dealer_player; i < MAX_PLAYERS; i++) {
        if(game->player_status[i] == PLAYER_ACTIVE) {
            game->dealer_player = i;
            game->current_player = i;
            move_to_next_player(game);
            return 1;
        }
    }
    
    for(int i = 0; i < MAX_PLAYERS; i++) {
        if(game->player_status[i] == PLAYER_ACTIVE) {
            game->dealer_player = i;
            game->current_player = i;
            move_to_next_player(game);
            return 1;
        }
    }

    move_to_next_player(game);
    return 1;
}

void send_info_packets(game_state_t *game) {
    for(int i = 0; i < MAX_PLAYERS; i++) {
        if (game->player_status[i] != PLAYER_LEFT) {
            server_packet_t out;
            build_info_packet(game, i, &out);
            if(send(game->sockets[i], &out, sizeof(server_packet_t), 0) < 0){
                printf("Error sending info packet to player %d\n", i);
            }
        }
    }
}

void reset_bets(game_state_t *game) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game->current_bets[i] = 0;
    }
    game->highest_bet = 0;
}

//This was our dealing function with some of the code removed (I left the dealing so we have the same logic)
void server_deal(game_state_t *game) {

    if(game->round_stage == ROUND_FLOP || game->round_stage == ROUND_TURN || game->round_stage == ROUND_RIVER) {
        set_first(game);
        reset_bets(game);
    }

    if(game->round_stage == ROUND_PREFLOP) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game->player_status[i] == PLAYER_ACTIVE) {
                game->player_hands[i][0] = game->deck[game->next_card++];
                game->player_hands[i][1] = game->deck[game->next_card++];
            }
        }
    }

    else {
        server_community(game);
    }

    int i = game->current_player;
    int num_played_players = 0;
    int last_to_raise = game->current_player;

    while(1){
        i = game->current_player;
       
        if(game->player_status[i] == PLAYER_ACTIVE){
            send_info_packets(game);

            server_packet_t out;
            client_packet_t in;
            
            int action = -1;
            while(action != 0) {
                int bytes_read = recv(game->sockets[i], &in, sizeof(client_packet_t), 0);
                if (bytes_read <= 0) {
                    printf("Player %d disconnected or read error.\n", i);
                    remove_player(game, i);
                    break; 
                }
                action = handle_client_action(game, i, &in, &out);
            }

            if (game->player_status[i] == PLAYER_ACTIVE) {
                num_played_players++;
                if (in.packet_type == RAISE) {
                    last_to_raise = i;
                    num_played_players = 1; 
                }
            }
        }

        move_to_next_player(game);

        int active_players = 0;
        for (int k = 0; k < MAX_PLAYERS; k++) {
            if (game->player_status[k] == PLAYER_ACTIVE) {
                active_players++;
            }
        }
        if (active_players <= 1) {
            game->round_stage = ROUND_SHOWDOWN;
            server_end(game);
            break;
        }

        if (check_betting_end(game) && game->current_player == last_to_raise) {
            break;
        }
    }

}

int server_bet(game_state_t *game) {
    //This was our function to determine if everyone has called or folded
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if(game->player_status[i] != 0) {
            return 1;
        }
    }
    return 0;
}

// Returns 1 if all bets are the same among active players
int check_betting_end(game_state_t *game) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->player_status[i] == PLAYER_ACTIVE) {
            if (game->current_bets[i] != game->highest_bet) {
                return 0;
            }
        }
    }
    return 1;
}

void server_community(game_state_t *game) {
    //This function checked the game state and dealt new community cards if needed;
    if(game->round_stage == ROUND_FLOP){
        game->community_cards[0] = game->deck[game->next_card++];
        game->community_cards[1] = game->deck[game->next_card++];
        game->community_cards[2] = game->deck[game->next_card++];
    }

    if(game->round_stage == ROUND_TURN){ 
        game->community_cards[3] = game->deck[game->next_card++];
    }
    if(game->round_stage == ROUND_RIVER){ 
        game->community_cards[4] = game->deck[game->next_card++];
    }
}

void server_end(game_state_t *game) {
    //This function sends the end packet
    player_id_t winner;
    int active_players = 0;
    
    for (int k = 0; k < MAX_PLAYERS; k++) {
        if (game->player_status[k] == PLAYER_ACTIVE) {
            winner = k;
            active_players++;
        }
    }
    if (active_players > 1) {
        winner = find_winner(game);
    }

    game->player_stacks[winner] += game->pot_size;

    for(int i = 0; i < MAX_PLAYERS; i++) {
        if(game->player_status[i] != PLAYER_LEFT) {
            server_packet_t out;
            build_end_packet(game, winner, &out);
            send(game->sockets[i], &out, sizeof(server_packet_t), 0);
        }
    }
}

int compare_cards(const void *a, const void *b) {
    card_t card_a = *(card_t *)a;
    card_t card_b = *(card_t *)b;
    return RANK(card_a) - RANK(card_b);
}

int is_four_of_a_kind(card_t *player_hand, int *ranks, int *hand_value) {
    
    int rank = -1;

    for(int i = 12; i >= 0; i--) {
        if(ranks[i] == 4) {
            rank = i;
            break;
        }
    }

    if (rank == -1) {
        return 0;
    }

    int tie_breaker = -1;
    for (int i = 6; i >= 0; i--) {
        if (RANK(player_hand[i]) != rank) {
            tie_breaker = RANK(player_hand[i]);
            break;
        }
    }

    *hand_value = 800 + (rank * 10) + tie_breaker;
    return 1;
}

int is_three_of_a_kind(card_t *player_hand, int *ranks, int *hand_value) { 

    int rank = -1;
    for(int i = 12; i >= 0; i--) {
        if(ranks[i] == 3) {
            rank = i;
            break;
        }
    }

    if (rank == -1) {
        return 0;
    }

    int tie_breaker1 = -1;
    int tie_breaker2 = -1;

    for (int i = 6; i >= 0; i--) {
        int r = RANK(player_hand[i]);
        if (r != rank) {
            if (tie_breaker1 == -1) {
                tie_breaker1 = r;
            } 
            else if (tie_breaker2 == -1) {
                tie_breaker2 = r;
                break;
            }
        }
    }

    *hand_value = 400 + (rank * 10) + tie_breaker1 + tie_breaker2;
    return 1;
}

int is_two_pair(card_t *player_hand, int *ranks, int *hand_value) {
    int pair_1 = -1;
    int pair_2 = -1;
   
    for(int i = 12; i >= 0; i--) {
        if (ranks[i] == 2) {
            if (pair_1 == -1) {
                pair_1 = i;
            } else if (pair_2 == -1) {
                pair_2 = i;
                break; 
            }
        }
    }

    if(pair_1 < 0 || pair_2 < 0) {
        return 0;
    }

    int tie_breaker = -1;
    for (int i = 6; i >= 0; i--) {
        if (RANK(player_hand[i]) != pair_1 && RANK(player_hand[i]) != pair_2) {
            tie_breaker = RANK(player_hand[i]);
            break;
        }
    }

    *hand_value = 300 + (pair_1  * 10) + pair_2 + tie_breaker;
    return 1;
}

int is_one_pair(card_t *player_hand, int *ranks, int *hand_value) {

    int rank = -1;

    for(int i = 12; i >= 0; i--) {
        if (ranks[i] == 2) {
           rank = i;
           break;
        }
    }

    if (rank == -1) {
        return 0;
    }

    int tie_breaker1 = -1;
    int tie_breaker2 = -1;
    int tie_breaker3 = -1;

    for (int i = 6; i >= 0; i--) {
        int r = RANK(player_hand[i]);
        if (r != rank) {
            if (tie_breaker1 == -1) {
                tie_breaker1 = r;
            } 
            else if (tie_breaker2 == -1) {
                tie_breaker2 = r;
            }
            else if (tie_breaker3 == -1) {
                tie_breaker3 = r;
                break;
            }
        }
    }

    *hand_value = 200 + (rank * 10) + tie_breaker1 + tie_breaker2 + tie_breaker3;
    return 1;
}

int is_full_house(int *ranks, int *hand_value) {
    int three = -1;
    int pair = -1;

    for (int i = 12; i >= 0; i--) {
        if (ranks[i] == 3) {
            three = i;
            break;
        }
    }
    if (three == -1) {
        return 0; 
    }

    for (int i = 12; i >= 0; i--) {
        if (ranks[i] == 2) {
            pair = i;
            break;
        }
    }

    if (pair == -1) return 0;

    *hand_value = 700 + (three * 10) + pair;
    return 1;
}

int is_flush(card_t *player_hand, int *suits, int *hand_value) {

    int flush = -1;

    for (int i = 0; i < 4; i++) {
        if (suits[i] >= 5) {
            flush = i;
            break;
        }
    }

    if (flush == -1) {
        return 0;
    }
    
    int highest = -1;

    for (int i = 6; i >= 0; i--) {
        if (SUITE(player_hand[i]) == flush) {
            highest = RANK(player_hand[i]);
            break;
        }
    }

    *hand_value = 600 + (flush * 10) + highest;
    return 1;
}

int is_straight(int *ranks, int *hand_value, int checking_straight_flush) {

    for (int i = 12; i >= 4; i--) {
        if (ranks[i] >= 1 && ranks[i-1] >= 1 && ranks[i-2] >= 1 && ranks[i-3] >= 1 && ranks[i-4] >= 1) {
            if(checking_straight_flush == 1) {
                *hand_value = 900 + (3  * 10); 
                return 1;
            }
            *hand_value = 500 + (i  * 10); 
            return 1;
        }
    }

    if(ranks[12] == 1 && ranks[0] == 1 && ranks[1] == 1 && ranks[2] == 1 && ranks[3] == 1) {
        if(checking_straight_flush == 1) {
            *hand_value = 900 + (3  * 10); 
            return 1;
        }
        *hand_value = 500 + (3  * 10); 
        return 1;
    }
    return 0;
}

int is_straight_flush(card_t *player_hand, int *suits, int *hand_value) { 
    int suit = -1;

    for (int i = 0; i < 4; i++) {
        if (suits[i] >= 5) {
            suit = i;
            break;
        }
    }

    if (suit == -1) {
        return 0;
    }

    card_t cards_with_same_suit[7];
    int size = 0;
    for (int i = 0; i < 7; i++) {
        if (SUITE(player_hand[i]) == suit) {
            cards_with_same_suit[size++] = player_hand[i];
        }
    }

    qsort(cards_with_same_suit, size, sizeof(card_t), compare_cards);

    int ranks[13] = {0}; 
    for (int i = 0; i < size; i++) {
        ranks[RANK(cards_with_same_suit[i])]++;
    }

    return is_straight(ranks, hand_value, 1);
}

int high_card(card_t *player_hand) {
    int value = 0;

    for (int i = 6; i >= 0; i--) {
        value += RANK(player_hand[i]);
    }
    return value;
}

int evaluate_hand(game_state_t *game, player_id_t pid) {

    card_t player_hand[7] = {
                game->player_hands[pid][0],
                game->player_hands[pid][1],
                game->community_cards[0],
                game->community_cards[1],
                game->community_cards[2],
                game->community_cards[3],
                game->community_cards[4]
            };

    qsort(player_hand, 7, sizeof(card_t), compare_cards);

    int ranks[13] = {0}; 
    int suits[4] = {0};

    for (int i = 0; i < 7; i++) {
        ranks[RANK(player_hand[i])]++;
        suits[SUITE(player_hand[i])]++;
    }

    int hand_value = 0;

    if (is_straight_flush(player_hand, suits, &hand_value)) return hand_value;
    if (is_four_of_a_kind(player_hand, ranks, &hand_value)) return hand_value;
    if (is_full_house(ranks, &hand_value)) return hand_value;
    if (is_flush(player_hand, suits, &hand_value)) return hand_value;
    if (is_straight(ranks, &hand_value, 0)) return hand_value;
    if (is_three_of_a_kind(player_hand, ranks, &hand_value)) return hand_value;
    if (is_two_pair(player_hand, ranks, &hand_value)) return hand_value;
    if (is_one_pair(player_hand, ranks, &hand_value)) return hand_value;
        
    return high_card(player_hand);
}

int find_winner(game_state_t *game) {
    //We wrote this function that looks at the game state and returns the player id for the best 5 card hand.
    int highest_value = -1;
    int player_with_best = -1;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->player_status[i] == PLAYER_ACTIVE) {
            int value = evaluate_hand(game, i);
            if (value > highest_value) {
                highest_value = value;
                player_with_best = i;
            }
        }
    }
    return player_with_best;
}