# Regression: parameter changes must propagate to vertices.
# Dump vertices at default, then at max params. Reference
# comparison catches if vertices don't change (the bug: model
# frozen because dirty_update was never set).

dump vertices

reset_flags
set_param {*} max
update

dump vertices
