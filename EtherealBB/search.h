#ifndef _SEARCH_H
#define _SEARCH_H

#include <stdint.h>

#include "types.h"


#define MaxKillers  (3)

/* Prototypes */
uint16_t get_best_move(Board * board, int seconds, int send_results);
int alpha_beta_prune(Board * board, int alpha, int beta, int depth, int height, int node_type);
int quiescence_search(Board * board, int alpha, int beta, int height);
void heuristic_eval_moves(Board * board, int * values, uint16_t * moves, int size, int height, uint16_t best_move);
uint16_t get_next_move(uint16_t * moves, int * values, int index, int size);
int evaluate_board(Board * board);


#endif