# Regression: reset_flags re-baselines the internal diff state without
# erasing the current frame's flags from the array (official Cubism Core
# semantics). Host apps call update -> reset -> read-while-drawing, so the
# change bits (1-6) must remain readable after the reset; the next update
# overwrites the array with the new diff.
update

# After initial (force) update, visible drawables have 0x7f
set flags_before [get_dynamic_flags 0]
set was_visible [bitand $flags_before 1]
set was_changed [bitand $flags_before 126]

reset_flags

# After reset, the array must still show the current frame's flags
set flags_after [get_dynamic_flags 0]
set still_visible [bitand $flags_after 1]
assert_eq $still_visible $was_visible "visibility preserved after reset"

# Change flags (bits 1-6) must remain readable after reset
set change_bits [bitand $flags_after 126]
assert_eq $change_bits $was_changed "change flags still readable after reset"

# The next update re-baselines and overwrites the array with a fresh diff
update
set flags_next [get_dynamic_flags 0]
assert_eq [bitand $flags_next 1] $was_visible "visibility consistent on next update"
