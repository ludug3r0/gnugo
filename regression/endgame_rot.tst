reset_reading_node_counter
reset_owl_node_counter
reset_life_node_counter
reset_trymove_counter

loadsgf rot3/games/endgame2.sgf 13
213 gg_genmove white
#? [M4|G17]*

loadsgf rot3/games/endgame2.sgf 15
215 gg_genmove white
#? [G17]*


# Report number of nodes visited by the tactical reading
10000 get_reading_node_counter
#?[0]&

# Report number of nodes visited by the owl code
10001 get_owl_node_counter
#?[0]&

# Report number of nodes visited by the life code
10002 get_life_node_counter
#?[0]&

# Report number of trymoves/trykos visited by the test
10003 get_trymove_counter
#?[0]&

