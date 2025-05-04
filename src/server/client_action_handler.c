#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "client_action_handler.h"
#include "game_logic.h"

/**
 * @brief Processes packet from client and generates a server response packet.
 * 
 * If the action is valid, a SERVER_INFO packet will be generated containing the updated game state.
 * If the action is invalid or out of turn, a SERVER_NACK packet is returned with an optional error message.
 * 
 * @param pid The ID of the client/player who sent the packet.
 * @param in Pointer to the client_packet_t received from the client.
 * @param out Pointer to a server_packet_t that will be filled with the response.
 * @return 0 if successful processing, -1 on NACK or error.
 */
int handle_client_action(game_state_t *game, player_id_t pid, const client_packet_t *in, server_packet_t *out) {
    //Optional function, see documentation above. Strongly reccomended.
    if (game->current_player != pid) {
        printf("[Server] It is not player %d's turn.\n", pid);
        out->packet_type = NACK;
        send(game->sockets[pid], out, sizeof(server_packet_t), 0);
        return -1;
    }

    int min_to_call = game->highest_bet - game->current_bets[pid];
    printf("Min to call %d\n", min_to_call);

    switch (in->packet_type) {
        case JOIN:
            printf("Action JOIN\n");
        case READY:
            printf("Action READY\n");
        case LEAVE:
            printf("[Server] Player %d cannot %s during dealing phase.\n", pid,
            in->packet_type == JOIN ? "join" :
            in->packet_type == READY ? "ready" : "leave");
            out->packet_type = NACK;
            break;

        case CHECK:
            printf("Action CHECK\n");
            if (min_to_call > 0) {
                out->packet_type = NACK;
                printf("[Server] Player %d cannot check: must call or fold.\n", pid);
                break;
            }
            out->packet_type = ACK;
            break;
        

        case CALL:
            printf("Action CALL\n");
            if(min_to_call > game->player_stacks[pid]){
                printf("[Server] Player %d attempted to call with insufficent chips.\n", pid);
                out->packet_type = NACK;
                break;
            }
            game->player_stacks[pid] -= min_to_call;
            game->current_bets[pid] += min_to_call;
            game->pot_size += min_to_call;
            out->packet_type = ACK;
            break;
        
        case RAISE: 
            printf("Action RAISE\n");
            if (in->params[0] > game->player_stacks[pid] || in->params[0] < min_to_call) {
                printf("[Server] Player %d attempeted to raise more than they had or raise less than the call amount.\n", pid);
                out->packet_type = NACK;
                break;
            }
            game->player_stacks[pid] -= in->params[0];
            game->current_bets[pid] += in->params[0];
            game->pot_size += in->params[0];
            game->highest_bet = game->current_bets[pid];
            out->packet_type = ACK;
            break;
        
        case FOLD: 
            printf("Action FOLD\n");
            game->player_status[pid] = PLAYER_FOLDED;
            out->packet_type = ACK;
            break;
        
    }

    send(game->sockets[pid], out, sizeof(server_packet_t), 0);
    
    return (out->packet_type == NACK) ? -1 : 0;
}

void build_info_packet(game_state_t *game, player_id_t pid, server_packet_t *out) {
    //Put state info from "game" (for player pid) into packet "out"
    out->packet_type = INFO;
    out->info.player_cards[0] = game->player_hands[pid][0];
    out->info.player_cards[1] = game->player_hands[pid][1];
    out->info.dealer = game->dealer_player;
    out->info.player_turn = game->current_player;
    out->info.bet_size = game->highest_bet;
    out->info.pot_size = game->pot_size;

    memcpy(out->info.player_stacks, game->player_stacks, MAX_PLAYERS * sizeof(int));
    memcpy(out->info.player_bets, game->current_bets, MAX_PLAYERS * sizeof(int));
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->player_status[i] == PLAYER_ACTIVE || game->player_status[i] == PLAYER_ALLIN) {
            out->info.player_status[i] = 1;
        } else if (game->player_status[i] == PLAYER_FOLDED) {
            out->info.player_status[i] = 0;
        } else {
            out->info.player_status[i] = 2;
        }
    }


    int community_cards = 0;
    switch (game->round_stage) {
        case ROUND_PREFLOP: 
                    community_cards = 0; 
                    break;
        case ROUND_FLOP: 
                    community_cards = 3; 
                    break;
        case ROUND_TURN: 
                    community_cards = 4; 
                    break;
        case ROUND_RIVER: 
                    community_cards = 5; 
                    break;
        default: 
                    community_cards = 0;
                    break;
    }


    memcpy(out->info.community_cards, game->community_cards, community_cards * sizeof(card_t));
    for (int i = community_cards; i < MAX_COMMUNITY_CARDS; i++) {
        out->info.community_cards[i] = NOCARD; 
    }

}

void build_end_packet(game_state_t *game, player_id_t winner, server_packet_t *out) {
    //Put state info from "game" (and calculate winner) into packet "out"
    out->packet_type = END;
    out->end.winner = winner;
    out->end.dealer = game->dealer_player;
    out->end.pot_size = game->pot_size;
    
    memcpy(out->end.player_cards, game->player_hands, sizeof(game->player_hands));

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if(game->player_status[i] == PLAYER_LEFT) {
            out->end.player_cards[i][0] = NOCARD;
            out->end.player_cards[i][1] = NOCARD;
        }
    }
   
    memcpy(out->end.community_cards, game->community_cards, MAX_COMMUNITY_CARDS * sizeof(card_t));
    memcpy(out->end.player_stacks, game->player_stacks, MAX_PLAYERS * sizeof(int));
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->player_status[i] == PLAYER_ACTIVE || game->player_status[i] == PLAYER_ALLIN) {
            out->end.player_status[i] = 1;
        } else if (game->player_status[i] == PLAYER_FOLDED) {
            out->end.player_status[i] = 0;
        } else {
            out->end.player_status[i] = 2;
        }
    }

}
