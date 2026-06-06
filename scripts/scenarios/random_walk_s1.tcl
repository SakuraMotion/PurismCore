# Random walk: 5 successive updates with incremental randomization (seed 1)
# Each iteration randomizes a subset and updates, testing accumulation behavior
srand 1

randomize_params *Angle*
randomize_params *ANGLE*
update
dump vertices

randomize_params *Eye*
randomize_params *EYE*
update
dump vertices

randomize_params *Mouth*
randomize_params *MOUTH*
update
dump vertices

randomize_params *Body*
randomize_params *BODY*
update
dump vertices

randomize_params
update
dump all
