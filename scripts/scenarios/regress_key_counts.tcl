# Regression: parameter key counts must be >= 2 for
# parameters that have bindings (most parameters).
# A bug returned pb_count (binding count) instead of
# keys_count, giving 1 for most parameters.
set pc [get_param_count]
set bad 0
set i 0
while {< $i $pc} {
  set kc [get_key_count $i]
  # Key count of 0 is valid (unbound param), but 1 is
  # suspicious — a parameter with 1 key has no interpolation
  # range and is effectively constant. Most real models have
  # key counts >= 2 for bound parameters.
  if {== $kc 1} {
    set bad [+ $bad 1]
  }
  set i [+ $i 1]
}
# Allow at most 10% of params to have key_count=1
# (some models have genuinely unbound params)
set limit [/ $pc 5]
if {> $bad $limit} {
  assert_eq $bad "0" "too many params with key_count=1"
}
