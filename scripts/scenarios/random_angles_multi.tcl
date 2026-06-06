# Randomize angle parameters across multiple updates with different seeds
srand 7
randomize_params *Angle*
randomize_params *ANGLE*
update
dump vertices

srand 77
randomize_params *Angle*
randomize_params *ANGLE*
update
dump vertices

srand 777
randomize_params *Angle*
randomize_params *ANGLE*
update
dump vertices
