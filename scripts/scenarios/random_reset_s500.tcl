# Randomize, update, then reset to defaults and update again (seed 500)
# Tests that the pipeline produces correct results after parameter reset
srand 500
randomize_params
update
dump drawables
dump vertices

# Reset all parameters to defaults
set_param * default
update
dump drawables
dump vertices
