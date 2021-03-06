#include "common.h"

lua_State *Lbase = NULL;

static GLenum texture_get_target(lua_State *L, const char *fmt)
{
	if(fmt == NULL) fmt = "*** INVALID";

	if(fmt[0] == '1')
	{
		if(fmt[1] == '\x00')
			return GL_TEXTURE_1D;
		else if(fmt[1] == 'a' && fmt[2] == '\x00')
			return GL_TEXTURE_1D_ARRAY;

	} else if(fmt[0] == '2') {
		if(fmt[1] == '\x00')
			return GL_TEXTURE_2D;
		else if(fmt[1] == 'a' && fmt[2] == '\x00')
			return GL_TEXTURE_2D_ARRAY;

	} else if(fmt[0] == '3') {
		if(fmt[1] == '\x00')
			return GL_TEXTURE_3D;
	}

	luaL_error(L, "unexpected GL target format \"%s\"", fmt);
	return GL_FALSE;
}

static GLenum texture_get_internal_fmt(lua_State *L, const char *fmt)
{
	if(fmt == NULL) fmt = "*** INVALID";

	static const struct {
		const char *str;
		GLenum val;
	} fmt_list[] = {
		{"1f", GL_R32F}, {"2f", GL_RG32F}, {"3f", GL_RGB32F}, {"4f", GL_RGBA32F},

		{"1i", GL_R32I}, {"2i", GL_RG32I}, {"3i", GL_RGB32I}, {"4i", GL_RGBA32I},
		{"1s", GL_R16I}, {"2s", GL_RG16I}, {"3s", GL_RGB16I}, {"4s", GL_RGBA16I},
		{"1b", GL_R8I}, {"2b", GL_RG8I}, {"3b", GL_RGB8I}, {"4b", GL_RGBA8I},

		{"1ui", GL_R32UI}, {"2ui", GL_RG32UI}, {"3ui", GL_RGB32UI}, {"4ui", GL_RGBA32UI},
		{"1us", GL_R16UI}, {"2us", GL_RG16UI}, {"3us", GL_RGB16UI}, {"4us", GL_RGBA16UI},
		{"1ub", GL_R8UI}, {"2ub", GL_RG8UI}, {"3ub", GL_RGB8UI}, {"4ub", GL_RGBA8UI},

		{"1ns", GL_R16}, {"2ns", GL_RG16}, {"3ns", GL_RGB16}, {"4ns", GL_RGBA16},
		{"1nb", GL_R8}, {"2nb", GL_RG8}, {"3nb", GL_RGB8}, {"4nb", GL_RGBA8},

		// THERE IS NO RGBA16_SNORM
		{"1Ns", GL_R16_SNORM}, {"2Ns", GL_RG16_SNORM}, {"3Ns", GL_RGB16_SNORM},
		{"1Nb", GL_R8_SNORM}, {"2Nb", GL_RG8_SNORM}, {"3Nb", GL_RGB8_SNORM}, {"4Nb", GL_RGBA8_SNORM},

		{NULL, 0}
	};

	int i;
	for(i = 0; fmt_list[i].str != NULL; i++)
		if(!strcmp(fmt_list[i].str, fmt))
			return fmt_list[i].val;

	luaL_error(L, "unexpected GL internal format \"%s\"", fmt);
	return GL_FALSE;

}

static void texture_get_data_fmt(lua_State *L, const char *fmt,
	GLenum *format, GLenum *typ, size_t *cmps, size_t *ebytes)
{
	if(fmt == NULL) fmt = "*** INVALID";

	if(fmt[0] == '1') *cmps = 1;
	else if(fmt[0] == '2') *cmps = 2;
	else if(fmt[0] == '3') *cmps = 3;
	else if(fmt[0] == '4') *cmps = 4;
	else luaL_error(L, "invalid component count for data format");

	*ebytes = 0;
	int is_float = (fmt[1] == 'f');
	int is_normalised = (fmt[1] == 'n' || fmt[1] == 'N');
	int is_unsigned = (fmt[1] == 'u' || fmt[1] == 'n');

	if(fmt[1] == 'f') *ebytes = 4;
	else if(fmt[1] == 'i') *ebytes = 4;
	else if(fmt[1] == 's') *ebytes = 2;
	else if(fmt[1] == 'b') *ebytes = 1;
	else if(fmt[1] == 'u') {
		if(fmt[2] == 'i') *ebytes = 4;
		else if(fmt[2] == 's') *ebytes = 2;
		else if(fmt[2] == 'b') *ebytes = 1;
	} else if( fmt[1] == 'n' || fmt[1] == 'N') {
		if(fmt[2] == 's') *ebytes = 2;
		else if(fmt[2] == 'b') *ebytes = 1;
	}

	if(*ebytes == 0)
		luaL_error(L, "invalid byte count for data format");

	if(is_float || is_normalised)
	{
		switch(*cmps)
		{
			case 1: *format = GL_RED; break;
			case 2: *format = GL_RG; break;
			case 3: *format = GL_RGB; break;
			case 4: *format = GL_RGBA; break;
			default: luaL_error(L, "EDOOFUS invalid component count"); break;
		}
	} else {
		switch(*cmps)
		{
			case 1: *format = GL_RED_INTEGER; break;
			case 2: *format = GL_RG_INTEGER; break;
			case 3: *format = GL_RGB_INTEGER; break;
			case 4: *format = GL_RGBA_INTEGER; break;
			default: luaL_error(L, "EDOOFUS invalid component count"); break;
		}
	}

	if(is_float)
	{
		switch(*ebytes)
		{
			case 4: *typ = GL_FLOAT; break;
			default: luaL_error(L, "EDOOFUS invalid elem byte count"); break;
		}

	} else if(is_unsigned) {
		switch(*ebytes)
		{
			case 1: *typ = GL_BYTE; break;
			case 2: *typ = GL_SHORT; break;
			case 4: *typ = GL_INT; break;
			default: luaL_error(L, "EDOOFUS invalid elem byte count"); break;
		}

	} else {
		switch(*ebytes)
		{
			case 1: *typ = GL_UNSIGNED_BYTE; break;
			case 2: *typ = GL_UNSIGNED_SHORT; break;
			case 4: *typ = GL_UNSIGNED_INT; break;
			default: luaL_error(L, "EDOOFUS invalid elem byte count"); break;
		}
	}
}


static int lbind_draw_cam_set_pa(lua_State *L)
{
	if(lua_gettop(L) < 5)
		return luaL_error(L, "expected 5 arguments to draw.cam_set_pa");

	cam_pos_x = lua_tonumber(L, 1);
	cam_pos_y = lua_tonumber(L, 2);
	cam_pos_z = lua_tonumber(L, 3);
	cam_rot_x = lua_tonumber(L, 4);
	cam_rot_y = lua_tonumber(L, 5);

	return 0;
}

static int lbind_draw_screen_size_get(lua_State *L)
{
	int wnd_w, wnd_h;
	SDL_GetWindowSize(window, &wnd_w, &wnd_h);
	
	lua_pushinteger(L, wnd_w);
	lua_pushinteger(L, wnd_h);
	return 2;
}

static int lbind_draw_viewport_set(lua_State *L)
{
	if(lua_gettop(L) < 4)
		return luaL_error(L, "expected 4 arguments to draw.viewport_set");

	GLint x = lua_tointeger(L, 1);
	GLint y = lua_tointeger(L, 2);
	GLsizei xlen = lua_tointeger(L, 3);
	GLsizei ylen = lua_tointeger(L, 4);
	glViewport(x, y, xlen, ylen);

	return 0;
}

static int lbind_draw_buffers_set(lua_State *L)
{
	int i;

	if(lua_gettop(L) < 1)
		return luaL_error(L, "expected 1 argument to draw.buffers_set");

	lua_len(L, 1);
	int len = lua_tointeger(L, -1);
	if(len <= 0 || len > 256) // arbitrary upper limit
		return luaL_error(L, "invalid length");
	lua_pop(L, 1);

	//
	int blen = len * sizeof(GLenum);
	if(blen < len)
		return luaL_error(L, "size overflow");

	GLenum *list = malloc(blen);
	for(i = 0; i < len; i++)
	{
		lua_geti(L, 1, i+1);
		int v = lua_tointeger(L, -1);
		lua_pop(L, 1);
		if(v >= 0 && v < 256)
			list[i] = GL_COLOR_ATTACHMENT0 + v;
		else {
			free(list);
			return luaL_error(L, "invalid output");
		}
	}

	assert(len > 0);
	glDrawBuffers(len, list);
	free(list);

	return 0;
}



static int lbind_draw_blit(lua_State *L)
{
	glBindVertexArray(va_ray_vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	return 0;
}

static int lbind_misc_mouse_grab_set(lua_State *L)
{
	if(lua_gettop(L) < 1)
		return luaL_error(L, "expected 1 argument to misc.mouse_grab_set");

	mouse_locked = lua_toboolean(L, 1);
	SDL_ShowCursor(!mouse_locked);
	SDL_SetWindowGrab(window, mouse_locked);
	SDL_SetRelativeMouseMode(mouse_locked);

	return 0;
}

static int lbind_misc_mouse_visible_set(lua_State *L)
{
	if(lua_gettop(L) < 1)
		return luaL_error(L, "expected 1 argument to misc.mouse_visible_set");

	SDL_ShowCursor(lua_toboolean(L, 1));

	return 0;
}

static int lbind_misc_exit(lua_State *L)
{
	do_exit = true;

	return 0;
}

static int lbind_misc_gl_error(lua_State *L)
{
	lua_pushinteger(L, glGetError());
	return 1;
}

static int lbind_fbo_new(lua_State *L)
{
	GLuint fbo;
	glGenFramebuffers(1, &fbo);

	lua_pushinteger(L, fbo);
	return 1;
}

static int lbind_fbo_bind_tex(lua_State *L)
{
	if(lua_gettop(L) < 5)
		return luaL_error(L, "expected 5 arguments to fbo.bind_tex");

	GLuint fbo = lua_tointeger(L, 1);
	int attachment = lua_tointeger(L, 2);
	const char *tex_fmt_str = lua_tostring(L, 3);
	GLuint tex = lua_tointeger(L, 4);
	int level = lua_tointeger(L, 5);
	GLenum tex_target = texture_get_target(L, tex_fmt_str);

	GLenum attachment_enum;
	if(attachment >= 0)
		attachment_enum = GL_COLOR_ATTACHMENT0 + attachment;
	else if(attachment == -1)
		attachment_enum = GL_DEPTH_ATTACHMENT;
	else if(attachment == -2)
		attachment_enum = GL_STENCIL_ATTACHMENT;
	else if(attachment == -3)
		attachment_enum = GL_DEPTH_STENCIL_ATTACHMENT;
	else
		return luaL_error(L, "invalid attachment number");

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_enum, tex_target, tex, level);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return 0;
}

static int lbind_fbo_target_set(lua_State *L)
{
	if(lua_gettop(L) < 1)
		return luaL_error(L, "expected 1 argument to fbo.target_set");

	GLuint fbo = lua_tointeger(L, 1);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	return 0;
}

static int lbind_fbo_validate(lua_State *L)
{
	if(lua_gettop(L) < 1)
		return luaL_error(L, "expected 1 argument to fbo.validate");

	GLuint fbo = lua_tointeger(L, 1);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	lua_pushboolean(L, glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	//printf("%04X\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return 1;
}

static int lbind_matrix_new(lua_State *L)
{
	mat4x4 *mat = lua_newuserdata(L, sizeof(mat4x4));

	mat4x4_identity(*mat);

	return 1;
}

static int lbind_matrix_identity(lua_State *L)
{
	if(lua_gettop(L) < 1)
		return luaL_error(L, "expected 1 argument to matrix.identity");

	mat4x4 *mat = lua_touserdata(L, 1);

	assert(mat != NULL);
	mat4x4_identity(*mat);

	return 0;
}

static int lbind_matrix_rotate_X(lua_State *L)
{
	if(lua_gettop(L) < 3)
		return luaL_error(L, "expected 3 arguments to matrix.rotate_X");

	mat4x4 *mat_A = lua_touserdata(L, 1);
	mat4x4 *mat_B = lua_touserdata(L, 2);
	double rot = lua_tonumber(L, 3);

	assert(mat_A != NULL);
	assert(mat_B != NULL);

	mat4x4_rotate_X(*mat_A, *mat_B, rot);

	return 0;
}

static int lbind_matrix_rotate_Y(lua_State *L)
{
	if(lua_gettop(L) < 3)
		return luaL_error(L, "expected 3 arguments to matrix.rotate_Y");

	mat4x4 *mat_A = lua_touserdata(L, 1);
	mat4x4 *mat_B = lua_touserdata(L, 2);
	double rot = lua_tonumber(L, 3);

	assert(mat_A != NULL);
	assert(mat_B != NULL);

	mat4x4_rotate_Y(*mat_A, *mat_B, rot);

	return 0;
}

static int lbind_matrix_translate_in_place(lua_State *L)
{
	if(lua_gettop(L) < 4)
		return luaL_error(L, "expected 4 arguments to matrix.translate_in_place");

	mat4x4 *mat_A = lua_touserdata(L, 1);
	double tx = lua_tonumber(L, 2);
	double ty = lua_tonumber(L, 3);
	double tz = lua_tonumber(L, 4);

	assert(mat_A != NULL);

	mat4x4_translate_in_place(*mat_A, tx, ty, tz);

	return 0;
}

static int lbind_matrix_invert(lua_State *L)
{
	if(lua_gettop(L) < 2)
		return luaL_error(L, "expected 2 arguments to matrix.invert");

	mat4x4 *mat_A = lua_touserdata(L, 1);
	mat4x4 *mat_B = lua_touserdata(L, 2);

	assert(mat_A != NULL);
	assert(mat_B != NULL);
	mat4x4_invert(*mat_A, *mat_B);

	return 0;
}

static int lbind_texture_unit_set(lua_State *L)
{
	if(lua_gettop(L) < 3)
		return luaL_error(L, "expected 3 arguments to texture.unit_set");

	int unit = lua_tointeger(L, 1);
	const char *tex_fmt_str = lua_tostring(L, 2);
	GLenum tex_target = texture_get_target(L, tex_fmt_str);
	int tex = lua_tointeger(L, 3);

	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(tex_target, tex);
	glActiveTexture(GL_TEXTURE0);

	return 0;
}

static int lbind_texture_load_sub(lua_State *L)
{
	int i;

	if(lua_gettop(L) < 7)
		return luaL_error(L, "expected at least 7 arguments to texture.load_sub");

	int tex = lua_tointeger(L, 1);
	const char *tex_fmt_str = lua_tostring(L, 2);
	GLenum tex_target = texture_get_target(L, tex_fmt_str);
	int dims = tex_fmt_str[0] - '0';
	int level = lua_tointeger(L, 3);
	assert(dims >= 1 && dims <= 3);

	if(lua_gettop(L) < 5 + 2*dims)
		return luaL_error(L, "expected %d arguments to texture.load_sub", 5 + 2*dims);

	// yeah yeah this is an arbitrary limit that doesn't necessarily hold
	if(level < 0 || level >= 16)
		return luaL_error(L, "invalid texture level");

	const char *data_fmt_str = lua_tostring(L, 4+dims*2);
	GLenum data_fmt_format;
	GLenum data_fmt_typ;
	size_t data_fmt_cmps;
	size_t data_fmt_ebytes;
	texture_get_data_fmt(L, data_fmt_str, &data_fmt_format, &data_fmt_typ,
		&data_fmt_cmps, &data_fmt_ebytes);
	int xoffs = (dims < 1 ? 0 : lua_tointeger(L, 4));
	int yoffs = (dims < 2 ? 0 : lua_tointeger(L, 5));
	int zoffs = (dims < 3 ? 0 : lua_tointeger(L, 6));
	int xlen = (dims < 1 ? 1 : lua_tointeger(L, 4+dims));
	int ylen = (dims < 2 ? 1 : lua_tointeger(L, 5+dims));
	int zlen = (dims < 3 ? 1 : lua_tointeger(L, 6+dims));

	if(xlen < 1 || ylen < 1 || zlen < 1)
		return luaL_error(L, "invalid texture size");
	if(xoffs < 0 || yoffs < 0 || zoffs < 0)
		return luaL_error(L, "invalid texture offset");

	// The overlooked source of buffer overflow exploits
	size_t belems = ((size_t)xlen) * ((size_t)ylen);
	if(belems < xlen || belems < ylen)
		return luaL_error(L, "texture size overflow");
	size_t old_belems = belems;
	belems *= ((size_t)zlen);
	if(belems < zlen || belems < old_belems)
		return luaL_error(L, "texture size overflow");

	if(dims == 2)
	{
		size_t becount = belems * data_fmt_cmps;
		if(becount < belems || becount < data_fmt_cmps)
			return luaL_error(L, "texture size overflow");

		size_t btotal = becount * data_fmt_ebytes;
		if(btotal < becount || btotal < data_fmt_ebytes)
			return luaL_error(L, "texture size overflow");

		// XXX: Do we use rawlen/rawgeti instead of len/geti?
		lua_len(L, 9);
		size_t len = lua_tointeger(L, -1);
		lua_pop(L, 1);
		if(len < becount)
			return luaL_error(L, "not enough elements in texture array to fill buffer");

		// TODO: use directly-mapped PBOs so we don't have to malloc
		void *data = malloc(btotal);
		if(data == NULL)
			return luaL_error(L, "could not allocate temp buffer for texture");

		switch(data_fmt_typ)
		{
			case GL_FLOAT:
				for(i = 0; i < becount; i++)
				{
					lua_geti(L, 9, i+1);
					float f = lua_tonumber(L, -1);
					lua_pop(L, 1);
					((float *)data)[i] = f;
				}
				break;

			default:
				return luaL_error(L, "TODO support data format \"%s\"", data_fmt_str);
		}


		//printf("%08X %08X %08X\n", tex_target, data_fmt_format, data_fmt_typ);
		glTexSubImage2D(tex_target, level, xoffs, yoffs, xlen, ylen,
			data_fmt_format, data_fmt_typ, data);

		free(data);

	} else {
		return luaL_error(L, "TODO: fill in other dimensions");

	}

	return 0;
}

static int lbind_texture_new(lua_State *L)
{
	int i;

	if(lua_gettop(L) < 6)
		return luaL_error(L, "expected at least 6 arguments to texture.new");

	// we need this for ARB_direct_state_access glCreateTextures!
	// (forward compat, currently not using right now)
	const char *tex_fmt_str = lua_tostring(L, 1);
	int levels = lua_tointeger(L, 2);
	const char *internal_fmt_str = lua_tostring(L, 3);
	GLenum tex_target = texture_get_target(L, tex_fmt_str);
	int dims = tex_fmt_str[0] - '0';
	assert(dims >= 1 && dims <= 3);

	if(lua_gettop(L) < 5+dims)
		return luaL_error(L, "expected %d arguments to texture.new", 5+dims);

	const char *filter_fmt_str = lua_tostring(L, dims+4);
	const char *data_fmt_str = lua_tostring(L, dims+5);

	if(filter_fmt_str == NULL) filter_fmt_str = "*** INVALID";

	GLenum internal_fmt = texture_get_internal_fmt(L, internal_fmt_str);

	// yeah yeah this is an arbitrary limit that doesn't necessarily hold
	if(levels-1 < 0 || levels-1 >= 16)
		return luaL_error(L, "invalid texture level");

	// fun thing, the last argument is only for glTexImage[123]D
	// if we use glTex\(ture\)\=Storage[123]D it's not necessary

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(tex_target, tex);
	glTexParameteri(tex_target, GL_TEXTURE_MAX_LEVEL, levels-1);
	//printf("%04X %u\n", tex_target, tex);

	if(filter_fmt_str[0] == 'n')
		glTexParameteri(tex_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	else if(filter_fmt_str[0] == 'l')
		glTexParameteri(tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	else
		return luaL_error(L, "invalid minification filter");

	if(filter_fmt_str[1] == 'n' && filter_fmt_str[2] == '\x00')
		glTexParameteri(tex_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	else if(filter_fmt_str[1] == 'l' && filter_fmt_str[2] == '\x00')
		glTexParameteri(tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	else if(filter_fmt_str[1] == 'n' && filter_fmt_str[2] == 'n' && filter_fmt_str[3] == '\x00')
		glTexParameteri(tex_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	else if(filter_fmt_str[1] == 'n' && filter_fmt_str[2] == 'l' && filter_fmt_str[3] == '\x00')
		glTexParameteri(tex_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	else if(filter_fmt_str[1] == 'l' && filter_fmt_str[2] == 'n' && filter_fmt_str[3] == '\x00')
		glTexParameteri(tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	else if(filter_fmt_str[1] == 'l' && filter_fmt_str[2] == 'l' && filter_fmt_str[3] == '\x00')
		glTexParameteri(tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	else
		return luaL_error(L, "invalid magnification filter");

	int xlen = (dims < 1 ? 1 : lua_tointeger(L, 4));
	int ylen = (dims < 2 ? 1 : lua_tointeger(L, 5));
	int zlen = (dims < 3 ? 1 : lua_tointeger(L, 6));
	if(xlen < 1 || ylen < 1 || zlen < 1)
		return luaL_error(L, "invalid texture size");

	if(epoxy_has_gl_extension("GL_ARB_texture_storage"))
	{
		if(dims == 2)
			glTexStorage2D(tex_target, levels, internal_fmt, xlen, ylen);
		else if(dims == 3)
			glTexStorage3D(tex_target, levels, internal_fmt, xlen, ylen, zlen);
		else
			return luaL_error(L, "TODO: fill in other dimensions");

	} else {
		GLenum data_fmt_format;
		GLenum data_fmt_typ;
		size_t data_fmt_cmps;
		size_t data_fmt_ebytes;
		texture_get_data_fmt(L, data_fmt_str, &data_fmt_format, &data_fmt_typ,
			&data_fmt_cmps, &data_fmt_ebytes);

		if(dims == 2)
			for(i = 0; i < levels; i++)
				glTexImage2D(tex_target, i, internal_fmt, xlen>>i, ylen>>i, 0,
					data_fmt_format, data_fmt_typ, NULL);
		else if(dims == 3)
			for(i = 0; i < levels; i++)
				glTexImage3D(tex_target, i, internal_fmt, xlen>>i, ylen>>i, zlen>>i, 0,
					data_fmt_format, data_fmt_typ, NULL);
		else
			return luaL_error(L, "TODO: fill in other dimensions");

	}

	lua_pushinteger(L, tex);
	return 1;
}

static int lbind_shader_new(lua_State *L)
{
	int i;

	if(lua_gettop(L) < 4)
		return luaL_error(L, "expected at least 4 arguments to shader.new");
	
	const char *vert_src = lua_tostring(L, 1);
	const char *frag_src = lua_tostring(L, 2);
	lua_len(L, 3); int len_input = lua_tointeger(L, -1); lua_pop(L, 1);
	lua_len(L, 4); int len_output = lua_tointeger(L, -1); lua_pop(L, 1);

	// TODO: actually use input/output lists
	GLuint ret = init_shader_str(vert_src, frag_src);

	lua_pushinteger(L, ret);
	return 1;
}

static int lbind_shader_use(lua_State *L)
{
	int i;

	if(lua_gettop(L) < 1)
		return luaL_error(L, "expected at least 1 argument to shader.use");
	
	if(lua_isnil(L, 1))
		glUseProgram(0);
	else
		glUseProgram(lua_tointeger(L, 1));

	return 0;
}

static int lbind_shader_uniform_location_get(lua_State *L)
{
	int i;

	if(lua_gettop(L) < 2)
		return luaL_error(L, "expected at least 2 arguments to shader.uniform_location_get");

	GLuint shader = lua_tointeger(L, 1);
	const char *name = lua_tostring(L, 2);
	if(name == NULL)
		return luaL_error(L, "expected string for arg 2");

	lua_pushinteger(L, glGetUniformLocation(shader, name));
	return 1;
}

static int lbind_shader_uniform_matrix_4f(lua_State *L)
{
	int i;

	if(lua_gettop(L) < 2)
		return luaL_error(L, "expected at least 2 arguments to shader.uniform_matrix_4f");

	GLuint idx = lua_tointeger(L, 1);
	mat4x4 *mat = lua_touserdata(L, 2);
	if(mat == NULL)
		return luaL_error(L, "expected matrix for arg 2");

	glUniformMatrix4fv(idx, 1, GL_FALSE, (GLfloat *)mat);

	return 0;
}

static int lbind_shader_uniform_f(lua_State *L)
{
	int top = lua_gettop(L);
	if(top < 2)
		return luaL_error(L, "expected at least 2 arguments to shader.uniform_f");

	int elems = top-1;
	if(elems < 1 || elems > 4)
		return luaL_error(L, "invalid element count");

	GLint idx = lua_tointeger(L, 1);
	switch(elems)
	{
		case 1:
			glUniform1f(idx
				, lua_tonumber(L, 2)
				);
			break;

		case 2:
			glUniform2f(idx
				, lua_tonumber(L, 2)
				, lua_tonumber(L, 3)
				);
			break;

		case 3:
			glUniform3f(idx
				, lua_tonumber(L, 2)
				, lua_tonumber(L, 3)
				, lua_tonumber(L, 4)
				);
			break;

		case 4:
			glUniform4f(idx
				, lua_tonumber(L, 2)
				, lua_tonumber(L, 3)
				, lua_tonumber(L, 4)
				, lua_tonumber(L, 5)
				);
			break;

		default:
			return luaL_error(L, "EDOOFUS: invalid element count");
	}

	return 0;
}

static int lbind_shader_uniform_i(lua_State *L)
{
	int top = lua_gettop(L);
	if(top < 2)
		return luaL_error(L, "expected at least 2 arguments to shader.uniform_i");

	int elems = top-1;
	if(elems < 1 || elems > 4)
		return luaL_error(L, "invalid element count");

	GLint idx = lua_tointeger(L, 1);
	switch(elems)
	{
		case 1:
			glUniform1i(idx
				, lua_tointeger(L, 2)
				);
			break;

		case 2:
			glUniform2i(idx
				, lua_tointeger(L, 2)
				, lua_tointeger(L, 3)
				);
			break;

		case 3:
			glUniform3i(idx
				, lua_tointeger(L, 2)
				, lua_tointeger(L, 3)
				, lua_tointeger(L, 4)
				);
			break;

		case 4:
			glUniform4i(idx
				, lua_tointeger(L, 2)
				, lua_tointeger(L, 3)
				, lua_tointeger(L, 4)
				, lua_tointeger(L, 5)
				);
			break;

		default:
			return luaL_error(L, "EDOOFUS: invalid element count");
	}

	return 0;
}

static int lbind_shader_uniform_ui(lua_State *L)
{
	int top = lua_gettop(L);
	if(top < 2)
		return luaL_error(L, "expected at least 2 arguments to shader.uniform_ui");

	int elems = top-1;
	if(elems < 1 || elems > 4)
		return luaL_error(L, "invalid element count");

	GLint idx = lua_tointeger(L, 1);
	switch(elems)
	{
		case 1:
			glUniform1ui(idx
				, lua_tointeger(L, 2)
				);
			break;

		case 2:
			glUniform2ui(idx
				, lua_tointeger(L, 2)
				, lua_tointeger(L, 3)
				);
			break;

		case 3:
			glUniform3ui(idx
				, lua_tointeger(L, 2)
				, lua_tointeger(L, 3)
				, lua_tointeger(L, 4)
				);
			break;

		case 4:
			glUniform4ui(idx
				, lua_tointeger(L, 2)
				, lua_tointeger(L, 3)
				, lua_tointeger(L, 4)
				, lua_tointeger(L, 5)
				);
			break;

		default:
			return luaL_error(L, "EDOOFUS: invalid element count");
	}

	return 0;
}

static int lbind_shader_uniform_fv(lua_State *L)
{
	int i;

	int top = lua_gettop(L);
	if(top < 4)
		return luaL_error(L, "expected at least 4 arguments to shader.uniform_fv");

	lua_len(L, 4);
	size_t size = lua_tointeger(L, -1);
	lua_pop(L, 1);

	size_t len = lua_tointeger(L, 2);
	size_t elems = lua_tointeger(L, 3);
	if(elems < 1 || elems > 4)
		return luaL_error(L, "invalid element count");
	if(len*elems > size || len*elems < elems || len*elems < len)
		return luaL_error(L, "invalid length");

	size_t bsize_c = len*elems;
	if(bsize_c < len || bsize_c < elems)
		return luaL_error(L, "size overflow");
	size_t bsize = bsize_c * sizeof(float);
	if(bsize < sizeof(float) || bsize < bsize_c)
		return luaL_error(L, "size overflow");

	float *data = malloc(bsize);

	for(i = 0; i < bsize_c; i++)
	{
		lua_geti(L, 4, i+1);
		float f = lua_tonumber(L, -1);
		lua_pop(L, 1);
		data[i] = f;
	}

	GLint idx = lua_tointeger(L, 1);
	//printf("%i %u %u %u\n", idx, len, elems, size);
	switch(elems)
	{
		case 1: glUniform1fv(idx, len, data); break;
		case 2: glUniform2fv(idx, len, data); break;
		case 3: glUniform3fv(idx, len, data); break;
		case 4: glUniform4fv(idx, len, data); break;

		default:
			return luaL_error(L, "EDOOFUS: invalid element count");
	}

	free(data);

	return 0;
}


void init_lua(void)
{
	// Create state
	Lbase = luaL_newstate();
	lua_State *L = Lbase;

	// Open builtin libraries
	// TODO cherry-pick once we make this into a game engine
	luaL_openlibs(L);

	//
	// Create tables to fill in
	//

	// --- draw
	lua_newtable(L);
	lua_pushcfunction(L, lbind_draw_cam_set_pa); lua_setfield(L, -2, "cam_set_pa");
	lua_pushcfunction(L, lbind_draw_screen_size_get); lua_setfield(L, -2, "screen_size_get");
	lua_pushcfunction(L, lbind_draw_blit); lua_setfield(L, -2, "blit");
	lua_pushcfunction(L, lbind_draw_viewport_set); lua_setfield(L, -2, "viewport_set");
	lua_pushcfunction(L, lbind_draw_buffers_set); lua_setfield(L, -2, "buffers_set");
	lua_setglobal(L, "draw");

	// --- fbo
	lua_newtable(L);
	lua_pushcfunction(L, lbind_fbo_new); lua_setfield(L, -2, "new");
	lua_pushcfunction(L, lbind_fbo_bind_tex); lua_setfield(L, -2, "bind_tex");
	lua_pushcfunction(L, lbind_fbo_target_set); lua_setfield(L, -2, "target_set");
	lua_pushcfunction(L, lbind_fbo_validate); lua_setfield(L, -2, "validate");
	lua_setglobal(L, "fbo");

	// --- matrix
	lua_newtable(L);
	lua_pushcfunction(L, lbind_matrix_new); lua_setfield(L, -2, "new");
	lua_pushcfunction(L, lbind_matrix_identity); lua_setfield(L, -2, "identity");
	lua_pushcfunction(L, lbind_matrix_rotate_X); lua_setfield(L, -2, "rotate_X");
	lua_pushcfunction(L, lbind_matrix_rotate_Y); lua_setfield(L, -2, "rotate_Y");
	lua_pushcfunction(L, lbind_matrix_translate_in_place); lua_setfield(L, -2, "translate_in_place");
	lua_pushcfunction(L, lbind_matrix_invert); lua_setfield(L, -2, "invert");
	lua_setglobal(L, "matrix");

	// --- texture
	lua_newtable(L);
	lua_pushcfunction(L, lbind_texture_new); lua_setfield(L, -2, "new");
	lua_pushcfunction(L, lbind_texture_unit_set); lua_setfield(L, -2, "unit_set");
	lua_pushcfunction(L, lbind_texture_load_sub); lua_setfield(L, -2, "load_sub");
	lua_setglobal(L, "texture");

	// --- shader
	lua_newtable(L);
	lua_pushcfunction(L, lbind_shader_new); lua_setfield(L, -2, "new");
	lua_pushcfunction(L, lbind_shader_use); lua_setfield(L, -2, "use");
	lua_pushcfunction(L, lbind_shader_uniform_location_get); lua_setfield(L, -2, "uniform_location_get");
	lua_pushcfunction(L, lbind_shader_uniform_matrix_4f); lua_setfield(L, -2, "uniform_matrix_4f");
	lua_pushcfunction(L, lbind_shader_uniform_f); lua_setfield(L, -2, "uniform_f");
	lua_pushcfunction(L, lbind_shader_uniform_i); lua_setfield(L, -2, "uniform_i");
	lua_pushcfunction(L, lbind_shader_uniform_ui); lua_setfield(L, -2, "uniform_ui");
	lua_pushcfunction(L, lbind_shader_uniform_fv); lua_setfield(L, -2, "uniform_fv");
	lua_setglobal(L, "shader");

	// --- voxel
	lua_newtable(L);
	lua_setglobal(L, "voxel");

	// --- misc
	lua_newtable(L);
	lua_pushcfunction(L, lbind_misc_exit); lua_setfield(L, -2, "exit");
	lua_pushcfunction(L, lbind_misc_gl_error); lua_setfield(L, -2, "gl_error");
	lua_pushcfunction(L, lbind_misc_mouse_grab_set); lua_setfield(L, -2, "mouse_grab_set");
	lua_pushcfunction(L, lbind_misc_mouse_visible_set); lua_setfield(L, -2, "mouse_visible_set");
	lua_setglobal(L, "misc");

	// Run main.lua
	printf("Running lua/main.lua\n");
	if(luaL_loadfile(L, "lua/main.lua") != LUA_OK)
	{
		printf("ERROR LOADING: %s\n", lua_tostring(L, 1));
		fflush(stdout);
		abort();
	}

	lua_call(L, 0, 0); // if it's broken, it needs to crash
}

