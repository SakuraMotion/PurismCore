# Regression: after reset_flags + param change + update,
# dynamic flags must have csmVertexPositionsDidChange (0x20) set.
# Without dirty_update, the flag was never set and the model froze.
set_param {*} max
update

# After init+update, all visible drawables have 0x7f (all changed)
# Now do the real test: reset, change, update again
reset_flags
set_param {*} min
update

# Check first drawable's dynamic flags
set flags [get_dynamic_flags 0]
# flags should have bit 0x20 (vertex changed) set
# 0x21 = visible + vertex changed (minimum expected)
# Actual value depends on model, but vertex_changed MUST be set
set has_vertex [bitand $flags 32]
assert_neq $has_vertex "0" "vertex_changed must be set after param change"
