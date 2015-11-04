require("lua/objects")
require("lua/tracer")

tracer_clear()

-- DEFINITIONS
m_chequer_bw = mat_chequer {
	c0 = var_vec3({0.9, 0.9, 0.9}),
	c1 = var_vec3({0.1, 0.1, 0.1}),
	sq_size = var_float(5.0),
}

--[[
obj_csg_subtract {
	main = obj_plane {
		dir = "+y",
		pos = {0, -2, 0},
		mat = m_chequer_bw,
	},

	cutters = {
		obj_sphere {
			pos = {0, -2, 0},
			rad = 1.0,
			mat = mat_solid {
				c0 = {0.9, 0.3, 0.9},
			},
		},
	},
}
]]

obj_plane {
	name = "floor",
	dir = "+y",
	pos = var_vec3({0, -2, 0}),
	mat = m_chequer_bw,
}

--[[
obj_plane {
	dir = "+z",
	pos = {0, 0, -20},
	mat = mat_solid { c0 = {1.0, 1.0, 0.0}, },
}
]]

--[[
obj_sphere {
	pos = {-3, 3.0, -10},
	rad = 5.0,
	mat = mat_solid { c0 = {1.0, 0.5, 0.0}, },
}

obj_sphere {
	pos = {3, 3.0, -10},
	rad = 5.0,
	mat = mat_solid { c0 = {0.0, 0.5, 1.0}, },
}
]]

obj_box {
	name = "box1",
	pos1 = var_vec3({-1, -1, -3}),
	pos2 = var_vec3({ 1,  1, -1}),
	mat = mat_solid { c0 = var_vec3({0.3, 1.0, 0.3}), },
}

local csg_test = {
	o1 = obj_sphere {
		name = "s1",
		pos = var_vec3({-3, 3.0, -10}),
		rad = var_float(5.0),
		--mat = mat_solid { c0 = {1.0, 0.5, 0.0}, },
		mat = mat_chequer {
			c0 = var_vec3({1.0, 0.5, 0.0}),
			c1 = var_vec3({0.0, 0.5, 1.0}),
			sq_size = var_float(0.5),
		},
	},

	o2 = obj_sphere {
		name = "s2",
		pos = var_vec3({3, 3.0, -10}),
		rad = var_float(5.0),
		--mat = mat_solid { c0 = {0.0, 0.5, 1.0}, },
		mat = mat_solid { c0 = var_vec3({0.5, 0.3, 0.0}), },
	},
}

tracer_generate {
	name = "test_isect",
	trace_scene = [=[

	Trace T1 = T;
	Trace T2 = T;
	obj_s1_trace(T1, true);
	obj_s2_trace(T2, true);

	// Ensure we hit both
	if(T1.hit_time < T1.zfar && T2.hit_time < T2.zfar)
	{
		if(T1.hit_time == T1.back_time && T2.hit_time == T2.back_time)
		{
			// Inside both
			obj_s1_trace(T, shadow_mode);
			obj_s2_trace(T, shadow_mode);

		} else if(T2.front_time < T1.front_time && T1.front_time < T2.back_time) {
			// Hit s1
			T.znear = T2.front_time;
			obj_s1_trace(T, shadow_mode);
			T.znear = T2.znear;

		} else if(T1.front_time < T2.front_time && T2.front_time < T1.back_time) {
			// Hit s2
			T.znear = T1.front_time;
			obj_s2_trace(T, shadow_mode);
			T.znear = T1.znear;
		}

	}

	obj_floor_trace(T, shadow_mode);
	obj_box1_trace(T, shadow_mode);

	]=],
}

tracer_generate {
	name = "test_sub",
	trace_scene = [=[

	Trace T1 = T;
	Trace T2 = T;
	obj_s1_trace(T1, true);
	obj_s2_trace(T2, true);

	if(T1.hit_time < T1.zfar && T1.hit_time == T1.back_time)
	{
		// Inside T1
		if(T2.hit_time >= T2.zfar)
		{
			obj_s1_trace(T, shadow_mode);
		} else if(T2.hit_time < T1.back_time) {
			obj_s2_trace(T, shadow_mode);

		}

	} else if(T1.hit_time < T1.zfar) {
		// Hit T1
		if(T2.hit_time >= T2.zfar || T1.front_time < T2.front_time || T1.front_time > T2.back_time)
		{
			obj_s1_trace(T, shadow_mode);

		} else if(T2.back_time > T1.back_time) {
			// PASS THROUGH

		} else {
			T.znear = T1.hit_time;
			obj_s2_trace(T, shadow_mode);
			T.znear = T1.znear;
		}

	}

	obj_floor_trace(T, shadow_mode);
	obj_box1_trace(T, shadow_mode);

	]=],
}

tracer_generate {
	name = "test_skip",
	trace_scene = [=[

	Trace T1 = T;
	obj_s1_trace(T1, true);
	obj_floor_trace(T1, true);
	obj_box1_trace(T1, true);

	Trace T2 = T;
	obj_s2_trace(T2, true);

	if(T2.hit_time < T2.zfar && T2.front_time < T1.hit_time)
	{
		T.znear = T2.back_time;
	}

	obj_s1_trace(T, shadow_mode);
	obj_floor_trace(T, shadow_mode);
	obj_box1_trace(T, shadow_mode);

	T.znear = T1.znear;

	]=],
}

tracer_generate {
	name = "test_union",
}

