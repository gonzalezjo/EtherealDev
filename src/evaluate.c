/*
  Ethereal is a UCI chess playing engine authored by Andrew Grant.
  <https://github.com/AndyGrant/Ethereal>     <andrew@grantnet.us>

  Ethereal is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Ethereal is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attacks.h"
#include "board.h"
#include "bitboards.h"
#include "castle.h"
#include "evaluate.h"
#include "masks.h"
#include "movegen.h"
#include "psqt.h"
#include "transposition.h"
#include "types.h"

#ifdef TUNE
    const int TRACE = 1;
    const EvalTrace EmptyTrace;
    EvalTrace T;
#else
    const int TRACE = 0;
    EvalTrace T;
#endif

#define S(mg, eg) (MakeScore((mg), (eg)))

/* Material Value Evaluation Terms */

const int PawnValue   = S( 106, 133);
const int KnightValue = S( 458, 404);
const int BishopValue = S( 478, 421);
const int RookValue   = S( 666, 707);
const int QueenValue  = S(1225,1347);
const int KingValue   = S(   0,   0);

const int PieceValues[8][PHASE_NB] = {
    { 106, 133}, { 458, 404}, { 478, 421}, { 666, 707},
    {1225,1347}, {   0,   0}, {   0,   0}, {   0,   0},
};

/* Pawn Evaluation Terms */

const int PawnCandidatePasser[2][RANK_NB] = {
   {S(   0,   0), S( -29,  -9), S( -13,  11), S( -13,  28),
    S(   3,  63), S(  52,  71), S(   0,   0), S(   0,   0)},
   {S(   0,   0), S( -15,  15), S(  -6,  21), S(   4,  45),
    S(  16,  87), S(  30,  65), S(   0,   0), S(   0,   0)},
};

const int PawnIsolated = S(  -8,  -9);

const int PawnStacked = S( -18, -29);

const int PawnBackwards[2] = { S(   8,  -2), S(  -6, -19) };

const int PawnConnected32[32] = {
    S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
    S(  -2,  -9), S(  11,   1), S(   4,   0), S(   4,  18),
    S(  15,   0), S(  34,  -2), S(  22,   9), S(  26,  18),
    S(  10,   0), S(  22,   4), S(  10,  11), S(  15,  23),
    S(  15,   7), S(  24,  14), S(  29,  21), S(  33,  21),
    S(  58,  24), S(  55,  45), S(  69,  52), S(  84,  57),
    S( 117,   6), S( 212,   8), S( 226,  36), S( 234,  40),
    S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
};

/* Knight Evaluation Terms */

const int KnightOutpost[2] = { S(   7, -28), S(  31,  -2) };

const int KnightBehindPawn = S(   4,  22);

const int KnightMobility[9] = {
    S( -81,-120), S( -38, -99), S( -23, -44), S(  -9, -18),
    S(   2,  -7), S(   9,  10), S(  18,  13), S(  28,  13),
    S(  39,  -2),
};

/* Bishop Evaluation Terms */

const int BishopPair = S(  24,  74);

const int BishopRammedPawns = S( -10, -14);

const int BishopOutpost[2] = { S(  11, -13), S(  42,   0) };

const int BishopBehindPawn = S(   3,  21);

const int BishopMobility[14] = {
    S( -66,-154), S( -32, -98), S( -12, -59), S(  -1, -31),
    S(   7, -17), S(  15,  -1), S(  17,   8), S(  18,  12),
    S(  17,  18), S(  20,  18), S(  18,  19), S(  34,   3),
    S(  35,  12), S(  70, -30),
};

/* Rook Evaluation Terms */

const int RookFile[2] = { S(  18,   6), S(  41,   1) };

const int RookOnSeventh = S(  -3,  33);

const int RookMobility[15] = {
    S(-149,-117), S( -55,-117), S( -14, -66), S(  -7, -24),
    S(  -7,  -3), S(  -8,  13), S(  -6,  24), S(   0,  27),
    S(   5,  30), S(   9,  35), S(  11,  41), S(  14,  46),
    S(  15,  51), S(  21,  45), S(  70,  11),
};

/* Queen Evaluation Terms */

const int QueenMobility[28] = {
    S( -62,-263), S(-206,-388), S( -64,-200), S( -32,-193),
    S( -20,-142), S( -16, -82), S( -10, -50), S( -10, -27),
    S(  -7, -17), S(  -6,   4), S(  -4,  16), S(  -2,  29),
    S(  -1,  30), S(   0,  40), S(  -1,  44), S(  -2,  46),
    S(  -3,  48), S(  -8,  47), S(  -6,  39), S( -10,  33),
    S(  -2,   8), S(  16, -14), S(  27, -53), S(  36, -76),
    S(  23, -94), S(  33,-122), S( -63, -63), S( -53,-110),
};

/* King Evaluation Terms */

const int KingDefenders[12] = {
    S( -18,  -2), S(  -9,   0), S(   0,   2), S(   7,   5),
    S(  16,   6), S(  26,   3), S(  32,  -6), S(   7,   0),
    S(  12,   6), S(  12,   6), S(  12,   6), S(  12,   6),
};

const int KingShelter[2][FILE_NB][RANK_NB] = {
  {{S( -11,   3), S(  18, -27), S(  21, -11), S(  11,   0),
    S(   7,   2), S( -10,   6), S(  -7, -35), S( -48,  18)},
   {S(  20,  -6), S(  24, -18), S(   1,  -6), S( -15,   3),
    S( -30,  17), S( -71,  67), S(  87,  75), S( -27,   4)},
   {S(  35,  -3), S(  19, -12), S( -24,   3), S(  -9, -12),
    S( -20,  -6), S( -26,   6), S(  -3,  68), S( -12,  -2)},
   {S(   7,  11), S(  23, -11), S(   6, -14), S(  17, -26),
    S(  25, -37), S( -64,  16), S(-134,  56), S(   7,  -8)},
   {S( -10,   7), S(   3,  -4), S( -24,   0), S( -15,   0),
    S( -18, -10), S( -44,   7), S(  30, -26), S(  -3,  -3)},
   {S(  44, -14), S(  25, -19), S( -22,  -1), S( -12, -21),
    S(   4, -25), S(  17, -17), S(  42, -27), S( -26,   1)},
   {S(  27, -10), S(   3, -19), S( -21,  -5), S( -17, -12),
    S( -28,  -9), S( -42,  37), S(  -3,  43), S( -11,   4)},
   {S(  -7,  -9), S(   9, -19), S(  12,  -3), S(   0,   7),
    S(  -9,  13), S( -17,  43), S(-186,  84), S( -16,  17)}},
  {{S(   0,   0), S(  -6, -28), S(   3, -23), S( -40,  13),
    S( -40,   7), S(   1,  36), S(-166,  -5), S( -46,  10)},
   {S(   0,   0), S(  26, -20), S(   7,  -8), S( -18,  -5),
    S(  -1,  -8), S(  14,  49), S(-183,   0), S( -42,   8)},
   {S(   0,   0), S(  32, -11), S(  -3,  -7), S(   6, -20),
    S(  12,  -9), S( -68,  47), S( -82, -67), S( -25,  -4)},
   {S(   0,   0), S(  -2,   6), S(  -2,   0), S( -16,  -4),
    S( -29,   0), S( -91,  42), S(   9, -37), S( -22,  -8)},
   {S(   0,   0), S(  12,   0), S(  12,  -8), S(  14, -15),
    S(  13, -26), S( -64,  22), S(-106, -55), S(   0, -10)},
   {S(   0,   0), S(   7,  -9), S( -20,  -2), S( -28, -12),
    S(  13, -25), S( -36,   0), S(  56,  30), S( -20,  -6)},
   {S(   0,   0), S(  27, -16), S(  14, -16), S(  -7, -14),
    S( -26,   3), S( -21,  18), S( -53, -39), S( -33,  12)},
   {S(   0,   0), S(  17, -42), S(  22, -32), S( -15, -10),
    S( -17,  14), S( -23,  43), S(-219, -42), S( -23,   7)}},
};

const int KingStorm[2][FILE_NB/2][RANK_NB] = {
  {{S( -14,  28), S( 129, -19), S( -29,  26), S( -24,  12),
    S( -16,   5), S( -10,   0), S( -21,   9), S( -23,  -1)},
   {S( -10,  50), S(  67,   8), S( -19,  27), S(  -4,  15),
    S(  -3,   7), S(   6,  -1), S(   0,   2), S( -13,   1)},
   {S(   6,  40), S(  25,  18), S( -21,  23), S(  -9,  12),
    S(   7,   3), S(  10,   0), S(  12,  -6), S(   4,   0)},
   {S(  -5,  29), S(  24,  20), S( -15,  12), S( -16,   7),
    S( -14,   4), S(   5,  -8), S(   0,  -4), S( -16,   5)}},
  {{S(   0,   0), S( -28, -29), S( -18,   1), S(  18, -14),
    S(  10,  -5), S(  -1, -17), S(   2,  -4), S(  26,  41)},
   {S(   0,   0), S( -30, -57), S(  -8,  -8), S(  34, -11),
    S(   0,   0), S(  13, -24), S(   5, -14), S( -24,   8)},
   {S(   0,   0), S( -48, -75), S( -30,  -6), S(  13,  -9),
    S(   5,  -1), S(  -6, -14), S(  -6, -22), S( -17,  11)},
   {S(   0,   0), S(  -3, -36), S( -19, -15), S( -12,  -1),
    S(  -5,  -3), S(   5, -27), S(  93, -29), S(  15,  26)}},
};

/* King Safety Evaluation Terms */

const int KSAttackWeight[]  = { 0, 16, 6, 10, 8, 0 };
const int KSAttackValue     =   44;
const int KSWeakSquares     =   38;
const int KSFriendlyPawns   =  -22;
const int KSNoEnemyQueens   = -276;
const int KSSafeQueenCheck  =   95;
const int KSSafeRookCheck   =   94;
const int KSSafeBishopCheck =   51;
const int KSSafeKnightCheck =  123;
const int KSAdjustment      =  -18;

/* Passed Pawn Evaluation Terms */

const int PassedPawn[2][2][RANK_NB] = {
  {{S(   0,   0), S( -49,   8), S( -69,  29), S( -89,  30),
    S( -13,  20), S(  87,  -4), S( 177,  53), S(   0,   0)},
   {S(   0,   0), S( -38,   8), S( -63,  28), S( -76,  29),
    S( -19,  30), S( 105,  20), S( 205,  84), S(   0,   0)}},
  {{S(   0,   0), S( -38,  16), S( -65,  23), S( -79,  36),
    S(  -8,  36), S( 111,  32), S( 281, 116), S(   0,   0)},
   {S(   0,   0), S( -39,  11), S( -58,  19), S( -71,  38),
    S( -10,  60), S(  98, 132), S( 194, 282), S(   0,   0)}},
};

const int PassedFriendlyDistance[RANK_NB] = {
    S(   0,   0), S(   0,   0), S(   2,  -4), S(   8, -12),
    S(   7, -17), S(  -8, -15), S( -14,  -9), S(   0,   0),
};

const int PassedEnemyDistance[RANK_NB] = {
    S(   0,   0), S(   5,  -1), S(   8,   0), S(   9,  10),
    S(   1,  25), S(   6,  35), S(  19,  38), S(   0,   0),
};

const int PassedSafePromotionPath = S( -34,  38);

/* Threat Evaluation Terms */

const int ThreatWeakPawn             = S( -11, -26);
const int ThreatMinorAttackedByPawn  = S( -57, -49);
const int ThreatMinorAttackedByMinor = S( -28, -36);
const int ThreatMinorAttackedByMajor = S( -26, -42);
const int ThreatRookAttackedByLesser = S( -58, -15);
const int ThreatQueenAttackedByOne   = S( -49, -22);
const int ThreatOverloadedPieces     = S(  -9, -18);
const int ThreatByPawnPush           = S(  17,  21);

/* General Evaluation Terms */

const int Tempo[COLOUR_NB] = { S(  25,  12), S( -25, -12) };

#undef S

int evaluateBoard(Board* board, PawnKingTable* pktable){

    EvalInfo ei;
    int phase, factor, eval, pkeval;

    // Setup and perform all evaluations
    initializeEvalInfo(&ei, board, pktable);
    eval   = evaluatePieces(&ei, board);
    pkeval = ei.pkeval[WHITE] - ei.pkeval[BLACK];
    eval  += pkeval + board->psqtmat + Tempo[board->turn];

    // Calcuate the game phase based on remaining material (Fruit Method)
    phase = 24 - 4 * popcount(board->pieces[QUEEN ])
               - 2 * popcount(board->pieces[ROOK  ])
               - 1 * popcount(board->pieces[KNIGHT]
                             |board->pieces[BISHOP]);
    phase = (phase * 256 + 12) / 24;

    // Scale evaluation based on remaining material
    factor = evaluateScaleFactor(board);

    // Compute the interpolated and scaled evaluation
    eval = (ScoreMG(eval) * (256 - phase)
         +  ScoreEG(eval) * phase * factor / SCALE_NORMAL) / 256;

    // Store a new Pawn King Entry if we did not have one
    if (ei.pkentry == NULL && pktable != NULL)
        storePawnKingEntry(pktable, board->pkhash, ei.passedPawns, pkeval);

    // Return the evaluation relative to the side to move
    return board->turn == WHITE ? eval : -eval;
}

int evaluatePieces(EvalInfo *ei, Board *board) {

    int eval = 0;

    eval += evaluatePawns(ei, board, WHITE)
          - evaluatePawns(ei, board, BLACK);

    eval += evaluateKnights(ei, board, WHITE)
          - evaluateKnights(ei, board, BLACK);

    eval += evaluateBishops(ei, board, WHITE)
          - evaluateBishops(ei, board, BLACK);

    eval += evaluateRooks(ei, board, WHITE)
          - evaluateRooks(ei, board, BLACK);

    eval += evaluateQueens(ei, board, WHITE)
          - evaluateQueens(ei, board, BLACK);

    eval += evaluateKings(ei, board, WHITE)
          - evaluateKings(ei, board, BLACK);

    eval += evaluatePassedPawns(ei, board, WHITE)
          - evaluatePassedPawns(ei, board, BLACK);

    eval += evaluateThreats(ei, board, WHITE)
          - evaluateThreats(ei, board, BLACK);

    return eval;
}

int evaluatePawns(EvalInfo *ei, Board *board, int colour) {

    const int US = colour, THEM = !colour;
    const int Forward = (colour == WHITE) ? 8 : -8;

    int sq, flag, eval = 0, pkeval = 0;
    uint64_t pawns, myPawns, tempPawns, enemyPawns, attacks;

    // Store off pawn attacks for king safety and threat computations
    ei->attackedBy2[US]      = ei->pawnAttacks[US] & ei->attacked[US];
    ei->attacked[US]        |= ei->pawnAttacks[US];
    ei->attackedBy[US][PAWN] = ei->pawnAttacks[US];

    // Update attacker counts for King Safety computation
    attacks = ei->pawnAttacks[US] & ei->kingAreas[THEM];
    ei->kingAttacksCount[US] += popcount(attacks);

    // Pawn hash holds the rest of the pawn evaluation
    if (ei->pkentry != NULL) return eval;

    pawns = board->pieces[PAWN];
    myPawns = tempPawns = pawns & board->colours[US];
    enemyPawns = pawns & board->colours[THEM];

    // Evaluate each pawn (but not for being passed)
    while (tempPawns) {

        // Pop off the next pawn
        sq = poplsb(&tempPawns);
        if (TRACE) T.PawnValue[US]++;
        if (TRACE) T.PawnPSQT32[relativeSquare32(sq, US)][US]++;

        uint64_t stoppers    = enemyPawns & passedPawnMasks(US, sq);
        uint64_t threats     = enemyPawns & pawnAttacks(US, sq);
        uint64_t support     = myPawns    & pawnAttacks(THEM, sq);
        uint64_t pushThreats = enemyPawns & pawnAttacks(US, sq + Forward);
        uint64_t pushSupport = myPawns    & pawnAttacks(THEM, sq + Forward);
        uint64_t leftovers   = stoppers ^ threats ^ pushThreats;

        // Save passed pawn information for later evaluation
        if (!stoppers) setBit(&ei->passedPawns, sq);

        // Apply a bonus for pawns which will become passers by advancing a single
        // square when exchanging our supporters with the remaining passer stoppers
        else if (!leftovers && popcount(pushSupport) >= popcount(pushThreats)) {
            flag = popcount(support) >= popcount(threats);
            pkeval += PawnCandidatePasser[flag][relativeRankOf(US, sq)];
            if (TRACE) T.PawnCandidatePasser[flag][relativeRankOf(US, sq)][US]++;
        }

        // Apply a penalty if the pawn is isolated
        if (!(adjacentFilesMasks(fileOf(sq)) & myPawns)) {
            pkeval += PawnIsolated;
            if (TRACE) T.PawnIsolated[US]++;
        }

        // Apply a penalty if the pawn is stacked
        if (Files[fileOf(sq)] & tempPawns) {
            pkeval += PawnStacked;
            if (TRACE) T.PawnStacked[US]++;
        }

        // Apply a penalty if the pawn is backward
        if (   !(passedPawnMasks(THEM, sq) & myPawns)
            &&  (testBit(ei->pawnAttacks[THEM], sq + Forward))) {
            flag = !(Files[fileOf(sq)] & enemyPawns);
            pkeval += PawnBackwards[flag];
            if (TRACE) T.PawnBackwards[flag][US]++;
        }

        // Apply a bonus if the pawn is connected and not backward
        else if (pawnConnectedMasks(US, sq) & myPawns) {
            pkeval += PawnConnected32[relativeSquare32(sq, US)];
            if (TRACE) T.PawnConnected32[relativeSquare32(sq, US)][US]++;
        }
    }

    ei->pkeval[US] = pkeval; // Save eval for the Pawn Hash

    return eval;
}

int evaluateKnights(EvalInfo *ei, Board *board, int colour) {

    const int US = colour, THEM = !colour;

    int sq, defended, count, eval = 0;
    uint64_t attacks;

    uint64_t enemyPawns  = board->pieces[PAWN  ] & board->colours[THEM];
    uint64_t tempKnights = board->pieces[KNIGHT] & board->colours[US  ];

    ei->attackedBy[US][KNIGHT] = 0ull;

    // Evaluate each knight
    while (tempKnights) {

        // Pop off the next knight
        sq = poplsb(&tempKnights);
        if (TRACE) T.KnightValue[US]++;
        if (TRACE) T.KnightPSQT32[relativeSquare32(sq, US)][US]++;

        // Compute possible attacks and store off information for king safety
        attacks = knightAttacks(sq);
        ei->attackedBy2[US]        |= attacks & ei->attacked[US];
        ei->attacked[US]           |= attacks;
        ei->attackedBy[US][KNIGHT] |= attacks;

        // Apply a bonus if the knight is on an outpost square, and cannot be attacked
        // by an enemy pawn. Increase the bonus if one of our pawns supports the knight
        if (     testBit(outpostRanksMasks(US), sq)
            && !(outpostSquareMasks(US, sq) & enemyPawns)) {
            defended = testBit(ei->pawnAttacks[US], sq);
            eval += KnightOutpost[defended];
            if (TRACE) T.KnightOutpost[defended][US]++;
        }

        // Apply a bonus if the knight is behind a pawn
        if (testBit(pawnAdvance(board->pieces[PAWN], 0ull, THEM), sq)) {
            eval += KnightBehindPawn;
            if (TRACE) T.KnightBehindPawn[US]++;
        }

        // Apply a bonus (or penalty) based on the mobility of the knight
        count = popcount(ei->mobilityAreas[US] & attacks);
        eval += KnightMobility[count];
        if (TRACE) T.KnightMobility[count][US]++;

        // Update for King Safety calculation
        attacks = attacks & ei->kingAreas[THEM];
        if (attacks) {
            ei->kingAttacksCount[US] += popcount(attacks);
            ei->kingAttackersCount[US] += 1;
            ei->kingAttackersWeight[US] += KSAttackWeight[KNIGHT];
        }
    }

    return eval;
}

int evaluateBishops(EvalInfo *ei, Board *board, int colour) {

    const int US = colour, THEM = !colour;

    int sq, defended, count, eval = 0;
    uint64_t attacks;

    uint64_t myPawns     = board->pieces[PAWN  ] & board->colours[US  ];
    uint64_t enemyPawns  = board->pieces[PAWN  ] & board->colours[THEM];
    uint64_t tempBishops = board->pieces[BISHOP] & board->colours[US  ];

    ei->attackedBy[US][BISHOP] = 0ull;

    // Apply a bonus for having a pair of bishops
    if ((tempBishops & WHITE_SQUARES) && (tempBishops & BLACK_SQUARES)) {
        eval += BishopPair;
        if (TRACE) T.BishopPair[US]++;
    }

    // Evaluate each bishop
    while (tempBishops) {

        // Pop off the next Bishop
        sq = poplsb(&tempBishops);
        if (TRACE) T.BishopValue[US]++;
        if (TRACE) T.BishopPSQT32[relativeSquare32(sq, US)][US]++;

        // Compute possible attacks and store off information for king safety
        attacks = bishopAttacks(sq, ei->occupiedMinusBishops[US]);
        ei->attackedBy2[US]        |= attacks & ei->attacked[US];
        ei->attacked[US]           |= attacks;
        ei->attackedBy[US][BISHOP] |= attacks;

        // Apply a penalty for the bishop based on number of rammed pawns
        // of our own colour, which reside on the same shade of square as the bishop
        count = popcount(ei->rammedPawns[US] & (testBit(WHITE_SQUARES, sq) ? WHITE_SQUARES : BLACK_SQUARES));
        eval += count * BishopRammedPawns;
        if (TRACE) T.BishopRammedPawns[US] += count;

        // Apply a bonus if the bishop is on an outpost square, and cannot be attacked
        // by an enemy pawn. Increase the bonus if one of our pawns supports the bishop.
        if (     testBit(outpostRanksMasks(US), sq)
            && !(outpostSquareMasks(US, sq) & enemyPawns)) {
            defended = testBit(ei->pawnAttacks[US], sq);
            eval += BishopOutpost[defended];
            if (TRACE) T.BishopOutpost[defended][US]++;
        }

        // Apply a bonus if the bishop is behind a pawn
        if (testBit(pawnAdvance((myPawns | enemyPawns), 0ull, THEM), sq)) {
            eval += BishopBehindPawn;
            if (TRACE) T.BishopBehindPawn[US]++;
        }

        // Apply a bonus (or penalty) based on the mobility of the bishop
        count = popcount(ei->mobilityAreas[US] & attacks);
        eval += BishopMobility[count];
        if (TRACE) T.BishopMobility[count][US]++;

        // Update for King Safety calculation
        attacks = attacks & ei->kingAreas[THEM];
        if (attacks) {
            ei->kingAttacksCount[US] += popcount(attacks);
            ei->kingAttackersCount[US] += 1;
            ei->kingAttackersWeight[US] += KSAttackWeight[BISHOP];
        }
    }

    return eval;
}

int evaluateRooks(EvalInfo *ei, Board *board, int colour) {

    const int US = colour, THEM = !colour;

    int sq, open, count, eval = 0;
    uint64_t attacks;

    uint64_t myPawns    = board->pieces[PAWN] & board->colours[  US];
    uint64_t enemyPawns = board->pieces[PAWN] & board->colours[THEM];
    uint64_t tempRooks  = board->pieces[ROOK] & board->colours[  US];

    ei->attackedBy[US][ROOK] = 0ull;

    // Evaluate each rook
    while (tempRooks) {

        // Pop off the next rook
        sq = poplsb(&tempRooks);
        if (TRACE) T.RookValue[US]++;
        if (TRACE) T.RookPSQT32[relativeSquare32(sq, US)][US]++;

        // Compute possible attacks and store off information for king safety
        attacks = rookAttacks(sq, ei->occupiedMinusRooks[US]);
        ei->attackedBy2[US]      |= attacks & ei->attacked[US];
        ei->attacked[US]         |= attacks;
        ei->attackedBy[US][ROOK] |= attacks;

        // Rook is on a semi-open file if there are no pawns of the rook's
        // colour on the file. If there are no pawns at all, it is an open file
        if (!(myPawns & Files[fileOf(sq)])) {
            open = !(enemyPawns & Files[fileOf(sq)]);
            eval += RookFile[open];
            if (TRACE) T.RookFile[open][US]++;
        }

        // Rook gains a bonus for being located on seventh rank relative to its
        // colour so long as the enemy king is on the last two ranks of the board
        if (   relativeRankOf(US, sq) == 6
            && relativeRankOf(US, ei->kingSquare[THEM]) >= 6) {
            eval += RookOnSeventh;
            if (TRACE) T.RookOnSeventh[US]++;
        }

        // Apply a bonus (or penalty) based on the mobility of the rook
        count = popcount(ei->mobilityAreas[US] & attacks);
        eval += RookMobility[count];
        if (TRACE) T.RookMobility[count][US]++;

        // Update for King Safety calculation
        attacks = attacks & ei->kingAreas[THEM];
        if (attacks) {
            ei->kingAttacksCount[US] += popcount(attacks);
            ei->kingAttackersCount[US] += 1;
            ei->kingAttackersWeight[US] += KSAttackWeight[ROOK];
        }
    }

    return eval;
}

int evaluateQueens(EvalInfo *ei, Board *board, int colour) {

    const int US = colour, THEM = !colour;

    int sq, count, eval = 0;
    uint64_t tempQueens, attacks;

    tempQueens = board->pieces[QUEEN] & board->colours[US];

    ei->attackedBy[US][QUEEN] = 0ull;

    // Evaluate each queen
    while (tempQueens) {

        // Pop off the next queen
        sq = poplsb(&tempQueens);
        if (TRACE) T.QueenValue[US]++;
        if (TRACE) T.QueenPSQT32[relativeSquare32(sq, US)][US]++;

        // Compute possible attacks and store off information for king safety
        attacks = rookAttacks(sq, ei->occupiedMinusRooks[US])
                | bishopAttacks(sq, ei->occupiedMinusBishops[US]);
        ei->attackedBy2[US]       |= attacks & ei->attacked[US];
        ei->attacked[US]          |= attacks;
        ei->attackedBy[US][QUEEN] |= attacks;

        // Apply a bonus (or penalty) based on the mobility of the queen
        count = popcount(ei->mobilityAreas[US] & attacks);
        eval += QueenMobility[count];
        if (TRACE) T.QueenMobility[count][US]++;

        // Update for King Safety calculation
        attacks = attacks & ei->kingAreas[THEM];
        if (attacks) {
            ei->kingAttacksCount[US] += popcount(attacks);
            ei->kingAttackersCount[US] += 1;
            ei->kingAttackersWeight[US] += KSAttackWeight[QUEEN];
        }
    }

    return eval;
}

int evaluateKings(EvalInfo *ei, Board *board, int colour) {

    const int US = colour, THEM = !colour;

    int count, eval = 0;

    uint64_t myPawns     = board->pieces[PAWN ] & board->colours[  US];
    uint64_t enemyPawns  = board->pieces[PAWN ] & board->colours[THEM];
    uint64_t enemyQueens = board->pieces[QUEEN] & board->colours[THEM];

    uint64_t myDefenders  = (board->pieces[PAWN  ] & board->colours[US])
                          | (board->pieces[KNIGHT] & board->colours[US])
                          | (board->pieces[BISHOP] & board->colours[US]);

    int kingSq = ei->kingSquare[US];
    int kingFile = fileOf(kingSq);
    int kingRank = rankOf(kingSq);

    if (TRACE) T.KingValue[US]++;
    if (TRACE) T.KingPSQT32[relativeSquare32(kingSq, US)][US]++;

    // Bonus for our pawns and minors sitting within our king area
    count = popcount(myDefenders & ei->kingAreas[US]);
    eval += KingDefenders[count];
    if (TRACE) T.KingDefenders[count][US]++;

    // Perform King Safety when we have two attackers, or
    // one attacker with a potential for a Queen attacker
    if (ei->kingAttackersCount[THEM] > 1 - popcount(enemyQueens)) {

        // Weak squares are attacked by the enemy, defended no more
        // than once and only defended by our Queens or our King
        uint64_t weak =   ei->attacked[THEM]
                      &  ~ei->attackedBy2[US]
                      & (~ei->attacked[US] | ei->attackedBy[US][QUEEN] | ei->attackedBy[US][KING]);

        // Usually the King Area is 9 squares. Scale are attack counts to account for
        // when the king is in an open area and expects more attacks, or the opposite
        float scaledAttackCounts = 9.0 * ei->kingAttacksCount[THEM] / popcount(ei->kingAreas[US]);

        // Safe target squares are defended or are weak and attacked by two.
        // We exclude squares containing pieces which we cannot capture.
        uint64_t safe =  ~board->colours[THEM]
                      & (~ei->attacked[US] | (weak & ei->attackedBy2[THEM]));

        // Find square and piece combinations which would check our King
        uint64_t occupied      = board->colours[WHITE] | board->colours[BLACK];
        uint64_t knightThreats = knightAttacks(kingSq);
        uint64_t bishopThreats = bishopAttacks(kingSq, occupied);
        uint64_t rookThreats   = rookAttacks(kingSq, occupied);
        uint64_t queenThreats  = bishopThreats | rookThreats;

        // Identify if there are pieces which can move to the checking squares safely.
        // We consider forking a Queen to be a safe check, even with our own Queen.
        uint64_t knightChecks = knightThreats & safe & ei->attackedBy[THEM][KNIGHT];
        uint64_t bishopChecks = bishopThreats & safe & ei->attackedBy[THEM][BISHOP];
        uint64_t rookChecks   = rookThreats   & safe & ei->attackedBy[THEM][ROOK  ];
        uint64_t queenChecks  = queenThreats  & safe & ei->attackedBy[THEM][QUEEN ];

        count  = ei->kingAttackersCount[THEM] * ei->kingAttackersWeight[THEM];

        count += KSAttackValue     * scaledAttackCounts
               + KSWeakSquares     * popcount(weak & ei->kingAreas[US])
               + KSFriendlyPawns   * popcount(myPawns & ei->kingAreas[US] & ~weak)
               + KSNoEnemyQueens   * !enemyQueens
               + KSSafeQueenCheck  * popcount(queenChecks)
               + KSSafeRookCheck   * popcount(rookChecks)
               + KSSafeBishopCheck * popcount(bishopChecks)
               + KSSafeKnightCheck * popcount(knightChecks)
               + KSAdjustment;

        // Convert safety to an MG and EG score, if we are unsafe
        if (count > 0) eval -= MakeScore(count * count / 720, count / 20);
    }

    // King Shelter & King Storm are stored in the Pawn King Table
    if (ei->pkentry != NULL) return eval;

    // Evaluate King Shelter & King Storm threat by looking at the file of our King,
    // as well as the adjacent files. When looking at pawn distances, we will use a
    // distance of 7 to denote a missing pawn, since distance 7 is not possible otherwise.
    for (int file = MAX(0, kingFile - 1); file <= MIN(FILE_NB - 1, kingFile + 1); file++) {

        // Find closest friendly pawn at or above our King on a given file
        uint64_t ours = myPawns & Files[file] & forwardRanksMasks(US, kingRank);
        int ourDist = !ours ? 7 : abs(kingRank - rankOf(backmost(US, ours)));

        // Find closest enemy pawn at or above our King on a given file
        uint64_t theirs = enemyPawns & Files[file] & forwardRanksMasks(US, kingRank);
        int theirDist = !theirs ? 7 : abs(kingRank - rankOf(backmost(US, theirs)));

        // Evaluate King Shelter using pawn distance. Use seperate evaluation
        // depending on the file, and if we are looking at the King's file
        ei->pkeval[US] += KingShelter[file == kingFile][file][ourDist];
        if (TRACE) T.KingShelter[file == kingFile][file][ourDist][US]++;

        // Evaluate King Storm using enemy pawn distance. Use a seperate evaluation
        // depending on the file, and if the opponent's pawn is blocked by our own
        int blocked = (ourDist != 7 && (ourDist == theirDist - 1));
        ei->pkeval[US] += KingStorm[blocked][mirrorFile(file)][theirDist];
        if (TRACE) T.KingStorm[blocked][mirrorFile(file)][theirDist][US]++;
    }

    return eval;
}

int evaluatePassedPawns(EvalInfo* ei, Board* board, int colour){

    const int US = colour, THEM = !colour;

    int sq, rank, dist, flag, canAdvance, safeAdvance, eval = 0;

    uint64_t bitboard;
    uint64_t tempPawns = board->colours[US] & ei->passedPawns;
    uint64_t occupied  = board->colours[WHITE] | board->colours[BLACK];

    // Evaluate each passed pawn
    while (tempPawns){

        // Pop off the next passed Pawn
        sq = poplsb(&tempPawns);
        rank = relativeRankOf(US, sq);
        bitboard = pawnAdvance(1ull << sq, 0ull, US);

        // Evaluate based on rank, ability to advance, and safety
        canAdvance = !(bitboard & occupied);
        safeAdvance = !(bitboard & ei->attacked[THEM]);
        eval += PassedPawn[canAdvance][safeAdvance][rank];
        if (TRACE) T.PassedPawn[canAdvance][safeAdvance][rank][US]++;

        // Evaluate based on distance from our king
        dist = distanceBetween(sq, ei->kingSquare[US]);
        eval += dist * PassedFriendlyDistance[rank];
        if (TRACE) T.PassedFriendlyDistance[rank][US] += dist;

        // Evaluate based on distance from their king
        dist = distanceBetween(sq, ei->kingSquare[THEM]);
        eval += dist * PassedEnemyDistance[rank];
        if (TRACE) T.PassedEnemyDistance[rank][US] += dist;

        // Apply a bonus when the path to promoting is uncontested
        bitboard = forwardRanksMasks(US, rankOf(sq)) & Files[fileOf(sq)];
        flag = !(bitboard & ei->attacked[THEM]);
        eval += flag * PassedSafePromotionPath;
        if (TRACE) T.PassedSafePromotionPath[US] += flag;
    }

    return eval;
}

int evaluateThreats(EvalInfo *ei, Board *board, int colour) {

    const int US = colour, THEM = !colour;
    const uint64_t Rank3Rel = US == WHITE ? RANK_3 : RANK_6;

    int count, eval = 0;

    uint64_t friendly = board->colours[  US];
    uint64_t enemy    = board->colours[THEM];
    uint64_t occupied = friendly | enemy;

    uint64_t pawns   = friendly & board->pieces[PAWN  ];
    uint64_t knights = friendly & board->pieces[KNIGHT];
    uint64_t bishops = friendly & board->pieces[BISHOP];
    uint64_t rooks   = friendly & board->pieces[ROOK  ];
    uint64_t queens  = friendly & board->pieces[QUEEN ];

    uint64_t attacksByPawns  = ei->attackedBy[THEM][PAWN  ];
    uint64_t attacksByMinors = ei->attackedBy[THEM][KNIGHT] | ei->attackedBy[THEM][BISHOP];
    uint64_t attacksByMajors = ei->attackedBy[THEM][ROOK  ] | ei->attackedBy[THEM][QUEEN ];

    // Squares with more attackers, few defenders, and no pawn support
    uint64_t poorlyDefended = (ei->attacked[THEM] & ~ei->attacked[US])
                            | (ei->attackedBy2[THEM] & ~ei->attackedBy2[US] & ~ei->attackedBy[US][PAWN]);

    // A friendly minor / major is overloaded if attacked and defended by exactly one
    uint64_t overloaded = (knights | bishops | rooks | queens)
                        & ei->attacked[  US] & ~ei->attackedBy2[  US]
                        & ei->attacked[THEM] & ~ei->attackedBy2[THEM];

    // Look for enemy non-pawn pieces which we may threaten with a pawn advance.
    // Don't consider pieces we already threaten, pawn moves which would be countered
    // by a pawn capture, and squares which are completely unprotected by our pieces.
    uint64_t pushThreat  = pawnAdvance(pawns, occupied, US);
    pushThreat |= pawnAdvance(pushThreat & ~attacksByPawns & Rank3Rel, occupied, US);
    pushThreat &= ~attacksByPawns & (ei->attacked[US] | ~ei->attacked[THEM]);
    pushThreat  = pawnAttackSpan(pushThreat, enemy & ~ei->attackedBy[US][PAWN], US);

    // Penalty for each of our poorly supported pawns
    count = popcount(pawns & ~attacksByPawns & poorlyDefended);
    eval += count * ThreatWeakPawn;
    if (TRACE) T.ThreatWeakPawn[US] += count;

    // Penalty for pawn threats against our minors
    count = popcount((knights | bishops) & attacksByPawns);
    eval += count * ThreatMinorAttackedByPawn;
    if (TRACE) T.ThreatMinorAttackedByPawn[US] += count;

    // Penalty for any minor threat against minor pieces
    count = popcount((knights | bishops) & attacksByMinors);
    eval += count * ThreatMinorAttackedByMinor;
    if (TRACE) T.ThreatMinorAttackedByMinor[US] += count;

    // Penalty for all major threats against poorly supported minors
    count = popcount((knights | bishops) & poorlyDefended & attacksByMajors);
    eval += count * ThreatMinorAttackedByMajor;
    if (TRACE) T.ThreatMinorAttackedByMajor[US] += count;

    // Penalty for pawn and minor threats against our rooks
    count = popcount(rooks & (attacksByPawns | attacksByMinors));
    eval += count * ThreatRookAttackedByLesser;
    if (TRACE) T.ThreatRookAttackedByLesser[US] += count;

    // Penalty for any threat against our queens
    count = popcount(queens & ei->attacked[THEM]);
    eval += count * ThreatQueenAttackedByOne;
    if (TRACE) T.ThreatQueenAttackedByOne[US] += count;

    // Penalty for any overloaded minors or majors
    count = popcount(overloaded);
    eval += count * ThreatOverloadedPieces;
    if (TRACE) T.ThreatOverloadedPieces[US] += count;

    // Bonus for giving threats by safe pawn pushes
    count = popcount(pushThreat);
    eval += count * ThreatByPawnPush;
    if (TRACE) T.ThreatByPawnPush[colour] += count;

    return eval;
}

int evaluateScaleFactor(Board *board) {

    uint64_t white   = board->colours[WHITE];
    uint64_t black   = board->colours[BLACK];
    uint64_t knights = board->pieces[KNIGHT];
    uint64_t bishops = board->pieces[BISHOP];
    uint64_t rooks   = board->pieces[ROOK  ];
    uint64_t queens  = board->pieces[QUEEN ];

    if (    onlyOne(white & bishops)
        &&  onlyOne(black & bishops)
        &&  onlyOne(bishops & WHITE_SQUARES)) {

        if (!(knights | rooks | queens))
            return SCALE_OCB_BISHOPS_ONLY;

        if (   !(rooks | queens)
            &&  onlyOne(white & knights)
            &&  onlyOne(black & knights))
            return SCALE_OCB_ONE_KNIGHT;

        if (   !(knights | queens)
            && onlyOne(white & rooks)
            && onlyOne(black & rooks))
            return SCALE_OCB_ONE_ROOK;
    }

    return SCALE_NORMAL;
}

void initializeEvalInfo(EvalInfo* ei, Board* board, PawnKingTable* pktable){

    uint64_t white   = board->colours[WHITE];
    uint64_t black   = board->colours[BLACK];
    uint64_t pawns   = board->pieces[PAWN];
    uint64_t bishops = board->pieces[BISHOP];
    uint64_t rooks   = board->pieces[ROOK];
    uint64_t queens  = board->pieces[QUEEN];
    uint64_t kings   = board->pieces[KING];

    uint64_t whitePawns = white & pawns;
    uint64_t blackPawns = black & pawns;

    int wKingSq = ei->kingSquare[WHITE] = getlsb(white & kings);
    int bKingSq = ei->kingSquare[BLACK] = getlsb(black & kings);

    ei->pawnAttacks[WHITE] = pawnAttackSpan(whitePawns, ~0ull, WHITE);
    ei->pawnAttacks[BLACK] = pawnAttackSpan(blackPawns, ~0ull, BLACK);

    ei->rammedPawns[WHITE] = pawnAdvance(blackPawns, ~whitePawns, BLACK);
    ei->rammedPawns[BLACK] = pawnAdvance(whitePawns, ~blackPawns, WHITE);

    ei->blockedPawns[WHITE] = pawnAdvance(white | black, ~whitePawns, BLACK);
    ei->blockedPawns[BLACK] = pawnAdvance(white | black, ~blackPawns, WHITE);

    ei->kingAreas[WHITE] = kingAreaMasks(WHITE, wKingSq);
    ei->kingAreas[BLACK] = kingAreaMasks(BLACK, bKingSq);

    ei->mobilityAreas[WHITE] = ~(ei->pawnAttacks[BLACK] | (white & kings) | ei->blockedPawns[WHITE]);
    ei->mobilityAreas[BLACK] = ~(ei->pawnAttacks[WHITE] | (black & kings) | ei->blockedPawns[BLACK]);

    ei->attacked[WHITE] = ei->attackedBy[WHITE][KING] = kingAttacks(wKingSq);
    ei->attacked[BLACK] = ei->attackedBy[BLACK][KING] = kingAttacks(bKingSq);

    ei->occupiedMinusBishops[WHITE] = (white | black) ^ (white & (bishops | queens));
    ei->occupiedMinusBishops[BLACK] = (white | black) ^ (black & (bishops | queens));

    ei->occupiedMinusRooks[WHITE] = (white | black) ^ (white & (rooks | queens));
    ei->occupiedMinusRooks[BLACK] = (white | black) ^ (black & (rooks | queens));

    ei->kingAttacksCount[WHITE]    = ei->kingAttacksCount[BLACK]    = 0;
    ei->kingAttackersCount[WHITE]  = ei->kingAttackersCount[BLACK]  = 0;
    ei->kingAttackersWeight[WHITE] = ei->kingAttackersWeight[BLACK] = 0;

    ei->pkentry       =     pktable == NULL ? NULL : getPawnKingEntry(pktable, board->pkhash);
    ei->passedPawns   = ei->pkentry == NULL ? 0ull : ei->pkentry->passed;
    ei->pkeval[WHITE] = ei->pkentry == NULL ? 0    : ei->pkentry->eval;
    ei->pkeval[BLACK] = ei->pkentry == NULL ? 0    : 0;
}
