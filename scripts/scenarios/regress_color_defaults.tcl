# Regression: color alpha channels must be initialized to 1.
# The bug was: memset zeroed all RGBA, interpolation only wrote
# RGB, leaving alpha=0. Multiply (0,0,0,0) made everything invisible.
# This test emits color data and verifies via reference comparison.
dump colors
