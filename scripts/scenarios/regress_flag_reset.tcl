# Regression: reset_flags preserves csmIsVisible (bit 0),
# only clears change flags (bits 1-6).
update

# After initial update, visible drawables have 0x7f
set flags_before [get_dynamic_flags 0]
set was_visible [bitand $flags_before 1]

reset_flags

# After reset, visibility bit should be preserved
set flags_after [get_dynamic_flags 0]
set still_visible [bitand $flags_after 1]
assert_eq $still_visible $was_visible "visibility preserved after reset"

# Change flags (bits 1-6) should be cleared
set change_bits [bitand $flags_after 126]
assert_eq $change_bits "0" "change flags cleared after reset"
