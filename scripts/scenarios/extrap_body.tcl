# Extrapolation test: body angle parameters beyond range
set_param *BodyAngleX* -20
set_param *BodyAngleY* -20
set_param *BodyAngleZ* -20
update
dump vertices
