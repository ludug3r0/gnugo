/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This is GNU Go, a Go program. Contact gnugo@gnu.org, or see       *
 * http://www.gnu.org/software/gnugo/ for more information.          *
 *                                                                   *
 * Copyright 1999, 2000, 2001, 2002 and 2003                         *
 * by the Free Software Foundation.                                  *
 *                                                                   *
 * This program is free software; you can redistribute it and/or     *
 * modify it under the terms of the GNU General Public License as    *
 * published by the Free Software Foundation - version 2             *
 *                                                                   *
 * This program is distributed in the hope that it will be useful,   *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the     *
 * GNU General Public License in file COPYING for more details.      *
 *                                                                   *
 * You should have received a copy of the GNU General Public         *
 * License along with this program; if not, write to the Free        *
 * Software Foundation, Inc., 59 Temple Place - Suite 330,           *
 * Boston, MA 02111, USA.                                            *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "gnugo.h"

#include <stdio.h>
#include <stdlib.h>

#include "liberty.h"

#define INFINITY 1000

static void update_status(int dr, enum dragon_status new_status, 
   			  enum dragon_status new_safety);


/* semeai() searches for pairs of dragons of opposite color which
 * have safety DEAD. If such a pair is found, owl_analyze_semeai is
 * called to read out which dragon will prevail in a semeai, and
 * whether a move now will make a difference in the outcome. The
 * dragon statuses are revised, and if a move now will make a
 * difference in the outcome this information is stored in
 * dragon_data2 and an owl reason is later generated by
 * semeai_move_reasons().
 */

#define MAX_DRAGONS 50

void
semeai()
{
  int semeai_results_first[MAX_DRAGONS][MAX_DRAGONS];
  int semeai_results_second[MAX_DRAGONS][MAX_DRAGONS];
  int semeai_move[MAX_DRAGONS][MAX_DRAGONS];
  char semeai_certain[MAX_DRAGONS][MAX_DRAGONS];
  int d1, d2;
  int k;
  int num_dragons = number_of_dragons;

  if (num_dragons > MAX_DRAGONS) {
    TRACE("Too many dragons!!! Might disregard some semeais.");
    num_dragons = MAX_DRAGONS;
  }

  for (d1 = 0; d1 < num_dragons; d1++)
    for (d2 = 0; d2 < num_dragons; d2++) {
      semeai_results_first[d1][d2] = -1;
      semeai_results_second[d1][d2] = -1;
    }

  for (d1 = 0; d1 < num_dragons; d1++)
    for (k = 0; k < dragon2[d1].neighbors; k++) {
      int apos = DRAGON(d1).origin;
      int bpos = DRAGON(dragon2[d1].adjacent[k]).origin;
      int result_certain;
      
      d2 = dragon[bpos].id;

      /* Look for semeais */
      
      if (dragon[apos].color == dragon[bpos].color
	  || (dragon[apos].status != DEAD
	      && dragon[apos].status != CRITICAL)
	  || (dragon[bpos].status != DEAD
	      && dragon[bpos].status != CRITICAL))
	continue;
      
      /* A dragon consisting of a single worm which is tactically dead or
       * critical and having just one neighbor should be ignored, since
       * the owl code is more reliable than the semeai code in such cases.
       * We do allow these cases if the worm has 4 liberties and can be
       * defended.
       */
      
      if (dragon[apos].size == worm[apos].size
	  && worm[apos].attack_codes[0]
	  && (worm[apos].liberties < 4
	      || worm[apos].defense_codes[0] == 0))
	continue;
      
      if (dragon[bpos].size == worm[bpos].size
	  && worm[bpos].attack_codes[0]
	  && (worm[bpos].liberties < 4
	      || worm[bpos].defense_codes[0] == 0))
	continue;
      /* Ignore inessential worms or dragons */
      
      if (worm[apos].inessential 
	  || DRAGON2(apos).safety == INESSENTIAL
	  || worm[bpos].inessential 
	  || DRAGON2(bpos).safety == INESSENTIAL)
	continue;

      /* If either dragon is a single stone, this is best left
       * to the owl code */
      if (dragon[apos].size == 1 || dragon[bpos].size == 1)
	continue;

      /* The array semeai_results_first[d1][d2] will contain the status
       * of d1 after the d1 d2 semeai, giving d1 the first move.
       * The array semeai_results_second[d1][d2] will contain the status
       * of d1 after the d1 d2 semeai, giving d2 the first move.
       */
      
      DEBUG(DEBUG_SEMEAI, "Considering semeai between %1m and %1m\n",
	    apos, bpos);
      owl_analyze_semeai(apos, bpos,
			 &(semeai_results_first[d1][d2]), 
			 &(semeai_results_second[d1][d2]),
			 &(semeai_move[d1][d2]), 1, &result_certain);
      DEBUG(DEBUG_SEMEAI, "results if %s moves first: %s %s, %1m%s\n",
	    board[apos] == BLACK ? "black" : "white",
	    result_to_string(semeai_results_first[d1][d2]),
	    result_to_string(semeai_results_second[d1][d2]),
	    semeai_move[d1][d2], result_certain ? "" : " (uncertain)");
      semeai_certain[d1][d2] = result_certain;
    }
  
  /* Look for dragons which lose all their semeais outright. The
   * winners in those semeais are considered safe and further semeais
   * they are involved in are disregarded. See semeai:81-86 and
   * nicklas5:1211 for examples of where this is useful.
   *
   * Note: To handle multiple simultaneous semeais properly we would
   * have to make simultaneous semeai reading. Lacking that we can
   * only get rough guesses of the correct status of the involved
   * dragons. This code is not guaranteed to be correct in all
   * situations but should usually be an improvement.
   */
  for (d1 = 0; d1 < num_dragons; d1++) {
    int involved_in_semeai = 0;
    int all_lost = 1;
    for (d2 = 0; d2 < num_dragons; d2++) {
      if (semeai_results_first[d1][d2] != -1) {
	involved_in_semeai = 1;
	if (semeai_results_first[d1][d2] != 0) {
	  all_lost = 0;
	  break;
	}
      }
    }
    
    if (involved_in_semeai && all_lost) {
      /* Leave the status changes to the main loop below. Here we just
       * remove the presumably irrelevant semeai results.
       */
      for (d2 = 0; d2 < num_dragons; d2++) {
	if (semeai_results_first[d1][d2] == 0) {
	  int d3;
	  for (d3 = 0; d3 < num_dragons; d3++) {
	    if (semeai_results_second[d3][d2] > 0) {
	      semeai_results_first[d3][d2] = -1;
	      semeai_results_second[d3][d2] = -1;
	      semeai_results_first[d2][d3] = -1;
	      semeai_results_second[d2][d3] = -1;
	    }
	  }
	}
      }
    }
  }

  for (d1 = 0; d1 < num_dragons; d1++) {
    int semeai_found = 0;
    int best_defense = 0;
    int best_attack = 0;
    int defense_move = PASS_MOVE;
    int attack_move = PASS_MOVE;
    int defense_certain = 0;
    int attack_certain = 0;
    int semeai_target = 0;
    
    for (d2 = 0; d2 < num_dragons; d2++) {
      if (semeai_results_first[d1][d2] == -1)
	continue;
      gg_assert(semeai_results_second[d1][d2] != -1);
      semeai_found = 1;

      if (best_defense < semeai_results_first[d1][d2]
	  || (best_defense == semeai_results_first[d1][d2]
	      && defense_certain < semeai_certain[d1][d2])) {
	best_defense = semeai_results_first[d1][d2];
	defense_move = semeai_move[d1][d2];
	defense_certain = semeai_certain[d1][d2];
	semeai_target = dragon2[d2].origin;
      }
      if (best_attack < semeai_results_second[d2][d1]
	  || (best_attack == semeai_results_second[d2][d1]
	      && attack_certain < semeai_certain[d2][d1])) {
	best_attack = semeai_results_second[d2][d1];
	attack_move = semeai_move[d2][d1];
	attack_certain = semeai_certain[d2][d1];
	semeai_target = dragon2[d2].origin;
      }
    }
    
    if (semeai_found) {
      dragon2[d1].semeai = 1;
      if (best_defense != 0 && best_attack != 0) {
	update_status(DRAGON(d1).origin, CRITICAL, CRITICAL);
	dragon2[d1].semeai_defense_point = defense_move;
	dragon2[d1].semeai_defense_certain = defense_certain;
	dragon2[d1].semeai_attack_point = attack_move;
	dragon2[d1].semeai_attack_certain = attack_certain;
	dragon2[d1].semeai_target = semeai_target;
      }
      else if (best_attack == 0 && attack_certain)
	update_status(DRAGON(d1).origin, ALIVE, ALIVE);
    }
  }
}


/* liberty_of_dragon(pos, origin) returns true if the vertex at (pos) is a
 * liberty of the dragon with origin at (origin).
 */
static int 
liberty_of_dragon(int pos, int origin)
{
  int k;
  if (pos == NO_MOVE)
    return 0;

  if (board[pos] != EMPTY)
    return 0;

  for (k = 0; k < 4; k++)
    if (ON_BOARD(pos + delta[k]) && dragon[pos + delta[k]].origin == origin)
      return 1;

  return 0;
}


/* This function adds the semeai related move reasons, using the information
 * stored in the dragon2 array.
 *
 * If the semeai had an uncertain result, and there is a owl move with
 * certain result doing the same, we don't trust the semeai move.
 */
void
semeai_move_reasons(int color)
{
  int other = OTHER_COLOR(color);
  int d;
  int liberties;
  int libs[MAXLIBS];
  int r;
  int resulta, resultb, semeai_move, s_result_certain;

  for (d = 0; d < number_of_dragons; d++)
    if (dragon2[d].semeai && DRAGON(d).status == CRITICAL) {
      if (DRAGON(d).color == color
          && dragon2[d].semeai_defense_point
	  && (dragon2[d].owl_defense_point == NO_MOVE
	      || dragon2[d].semeai_defense_certain >= 
	         dragon2[d].owl_defense_certain)) {
	/* My dragon can be defended */
	add_semeai_move(dragon2[d].semeai_defense_point, dragon2[d].origin);
	DEBUG(DEBUG_SEMEAI, "Adding semeai defense move for %1m at %1m\n",
	      DRAGON(d).origin, dragon2[d].semeai_defense_point);
	if (liberty_of_dragon(dragon2[d].semeai_defense_point,
			      dragon2[d].semeai_target)
	    && !liberty_of_dragon(dragon2[d].semeai_defense_point,
				  dragon2[d].origin)
	    && !is_self_atari(dragon2[d].semeai_defense_point, color)) {
	  
	  /* If this is a move to fill the non-common liberties of the
	   * target, and is not a ko or snap-back, then we try all
	   * non-common liberties of the target and add all winning
	   * moves to the move list.
	   */

          liberties = findlib(dragon2[d].semeai_target, MAXLIBS, libs);

          for (r = 0; r < liberties; r++) {
            if (!liberty_of_dragon(libs[r], dragon2[d].origin)
		&& !is_self_atari(libs[r], color)
		&& libs[r] != dragon2[d].semeai_defense_point) {
              owl_analyze_semeai_after_move(libs[r], color,
					    dragon2[d].semeai_target,
					    dragon2[d].origin,
					    &resulta, &resultb, &semeai_move,
					    1, &s_result_certain);
              if (resulta == 0 && resultb == 0) {
	        add_semeai_move(libs[r], dragon2[d].origin);
	        DEBUG(DEBUG_SEMEAI,
		      "Adding semeai defense move for %1m at %1m\n",
		      DRAGON(d).origin, libs[r]);
	      }
            }
	  }
	}
      }
      else if (DRAGON(d).color == other
	       && dragon2[d].semeai_attack_point
	       && (dragon2[d].owl_attack_point == NO_MOVE
		   || dragon2[d].semeai_attack_certain >= 
		      dragon2[d].owl_attack_certain)) {
	/* Your dragon can be attacked */
	add_semeai_move(dragon2[d].semeai_attack_point, dragon2[d].origin);
	DEBUG(DEBUG_SEMEAI, "Adding semeai attack move for %1m at %1m\n",
	      DRAGON(d).origin, dragon2[d].semeai_attack_point);
	if (liberty_of_dragon(dragon2[d].semeai_attack_point,
			      dragon2[d].origin)
	    && !liberty_of_dragon(dragon2[d].semeai_attack_point,
				  dragon2[d].semeai_target)
	    && !is_self_atari(dragon2[d].semeai_attack_point, color)) {

          liberties = findlib(dragon2[d].origin, MAXLIBS, libs);

          for (r = 0; r < liberties; r++) {
            if (!liberty_of_dragon(libs[r], dragon2[d].semeai_target)
		&& !is_self_atari(libs[r], color)
		&& libs[r] != dragon2[d].semeai_attack_point) {
              owl_analyze_semeai_after_move(libs[r], color, dragon2[d].origin,
					    dragon2[d].semeai_target,
					    &resulta, &resultb, &semeai_move,
					    1, &s_result_certain);
              if (resulta == 0 && resultb == 0) {
	        add_semeai_move(libs[r], dragon2[d].origin);
	        DEBUG(DEBUG_SEMEAI,
		      "Adding semeai attack move for %1m at %1m\n",
		      DRAGON(d).origin, libs[r]);
	      }
            }
	  }
	}
      }
    }
}


/* Change the status and safety of a dragon */

static void
update_status(int dr, enum dragon_status new_status,
    	      enum dragon_status new_safety)
{
  int pos;

  if (dragon[dr].status != new_status
      && (dragon[dr].status != CRITICAL || new_status != DEAD)) {
    DEBUG(DEBUG_SEMEAI, "Changing status of %1m from %s to %s.\n", dr,
	  status_to_string(dragon[dr].status),
	  status_to_string(new_status));
    for (pos = BOARDMIN; pos < BOARDMAX; pos++)
      if (IS_STONE(board[pos]) && is_same_dragon(dr, pos))
	dragon[pos].status = new_status;
  }

  if (DRAGON2(dr).safety != new_safety
      && (DRAGON2(dr).safety != CRITICAL || new_safety != DEAD)) {
    DEBUG(DEBUG_SEMEAI, "Changing safety of %1m from %s to %s.\n", dr,
	  status_to_string(DRAGON2(dr).safety), status_to_string(new_safety));
    DRAGON2(dr).safety = new_safety;
  }
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
