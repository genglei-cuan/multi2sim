/*
 *  Multi2Sim
 *  Copyright (C) 2012  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>
#include <unistd.h>
#include <stdio.h>

#include "buffer.h"
#include "context.h"
#include "debug.h"
#include "list.h"
#include "linked-list.h"
#include "mhandle.h"
#include "shader.h"
#include "program.h"
#include "vertex-array.h"

/* Global variables */
static unsigned int vao_id = 0;

/* Forward declaration */
static unsigned int opengl_vertex_array_obj_assign_id();

static struct opengl_vertex_attrib_t *opengl_vertex_attrib_create();
static void opengl_vertex_attrib_free(struct opengl_vertex_attrib_t *vattrib);
static unsigned int opengl_vertex_attrib_get_element_size(unsigned int num_elems, unsigned int data_type);

static struct opengl_vertex_array_obj_t *opengl_vertex_array_obj_create();
static void opengl_vertex_array_obj_free(struct opengl_vertex_array_obj_t *vao);
static void opengl_vertex_array_obj_ref_update(struct opengl_vertex_array_obj_t *vao, int change);

static void opengl_vertex_array_obj_repo_add(
	struct linked_list_t *vtx_array_obj_repo, struct opengl_vertex_array_obj_t *vao);
static struct opengl_vertex_array_obj_t *opengl_vertex_array_obj_repo_get(
	struct linked_list_t *vtx_array_obj_repo, unsigned int id);


/*
 * Private Functions
 */

static unsigned int opengl_vertex_array_obj_assign_id()
{
	vao_id += 1;
	return vao_id;
}

static struct opengl_vertex_attrib_t *opengl_vertex_attrib_create()
{
	struct opengl_vertex_attrib_t *vattrib;

	/* Allocate */
	vattrib = xcalloc(1, sizeof(struct opengl_vertex_attrib_t));

	/* Debug */
	opengl_debug("\t%s: OpenGL Vertex Attribute [%p] created\n", __FUNCTION__, vattrib);

	/* Return */
	return vattrib;
}

static void opengl_vertex_attrib_free(struct opengl_vertex_attrib_t *vattrib)
{
	/* Detach */
	opengl_vertex_attrib_detach_buffer_obj(vattrib, vattrib->vbo);

	/* Debug */
	opengl_debug("\t%s: OpenGL Vertex Attribute [%p] freed\n", __FUNCTION__, vattrib);

	/* Free */
	free(vattrib);
}


static unsigned int opengl_vertex_attrib_get_element_size(unsigned int num_elems, unsigned int data_type)
{
	if (data_type != GL_INT_2_10_10_10_REV && data_type != GL_UNSIGNED_INT_2_10_10_10_REV)
	{
		return num_elems*opengl_context_get_data_size(data_type);
	}
	else
		return opengl_context_get_data_size(data_type);
}

static struct opengl_vertex_array_obj_t *opengl_vertex_array_obj_create(unsigned int attribs_count)
{
	struct opengl_vertex_array_obj_t *vao;
	int i;

	/* Allocate */
	vao = xcalloc(1, sizeof(struct opengl_vertex_array_obj_t));

	/* Initialize */
	vao->id = opengl_vertex_array_obj_assign_id();
	vao->ref_count = 0;
	pthread_mutex_init(&vao->ref_mutex, NULL);
	vao->delete_pending = 0;

	vao->attribs_count = attribs_count;
	vao->attribs = xcalloc(attribs_count, sizeof(struct opengl_vertex_attrib_t *));
	for (i = 0; i < attribs_count; ++i)
		vao->attribs[i] = opengl_vertex_attrib_create();

	/* Debug */
	opengl_debug("\t%s: OpenGL VAO #%d [%p] created\n", __FUNCTION__, vao->id, vao);

	/* Return */	
	return vao;
}
static void opengl_vertex_array_obj_free(struct opengl_vertex_array_obj_t *vao)
{
	int i;

	/* Free */
	pthread_mutex_destroy(&vao->ref_mutex);
	for (i = 0; i < vao->attribs_count; ++i)
		opengl_vertex_attrib_free(vao->attribs[i]);
	free(vao->attribs);
	free(vao);

	/* Debug */
	opengl_debug("\t%s: OpenGL VAO [%p] free\n", __FUNCTION__, vao);
}

static void opengl_vertex_array_obj_delete(struct opengl_vertex_array_obj_t *vao)
{
	assert(vao);

	vao->delete_pending = 1;

	/* Delete checks conditions */
	if (!vao->ref_count)
		opengl_vertex_array_obj_free(vao);
	else
	{
		/* Debug */
		opengl_debug("\t%s: Cannot delete VAO #%d [%p], check if still in use\n", 
			__FUNCTION__, vao->id, vao);
	}

}
/* Update VAO reference count */
static void opengl_vertex_array_obj_ref_update(struct opengl_vertex_array_obj_t *vao, int change)
{
	int count;

	pthread_mutex_lock(&vao->ref_mutex);
	vao->ref_count += change;
	count = vao->ref_count;
	pthread_mutex_unlock(&vao->ref_mutex);

	if (count < 0)
		panic("\t%s: number of references is negative", __FUNCTION__);
}


static void opengl_vertex_array_obj_repo_add(struct linked_list_t *vtx_array_obj_repo, struct opengl_vertex_array_obj_t *vao)
{
	linked_list_add(vtx_array_obj_repo, vao);

	/* Debug */
	opengl_debug("\t%s: VAO #%d [%p] add to Vertex Array Repository [%p]\n", 
		__FUNCTION__, vao->id, vao, vtx_array_obj_repo);
}

static int opengl_vertex_array_obj_repo_remove(struct linked_list_t *vtx_array_obj_repo, 
	struct opengl_vertex_array_obj_t *vertex_array_obj)
{
	if (vertex_array_obj->ref_count != 0)
	{
		opengl_debug("\t%s: VAO #%d [%p] cannot be removed immediately, as reference counter = %d\n", 
			__FUNCTION__, vertex_array_obj->id, vertex_array_obj, vertex_array_obj->ref_count);
		return -1;
	}
	else 
	{
		/* Check if VAO exists */
		linked_list_find(vtx_array_obj_repo, vertex_array_obj);
		if (vtx_array_obj_repo->error_code)
			fatal("\t%s: VAO does not exist", __FUNCTION__);
		linked_list_remove(vtx_array_obj_repo);
		opengl_debug("\t%s: Remove VAO #%d [%p] from VAO Repository [%p]\n", 
			__FUNCTION__, vertex_array_obj->id, vertex_array_obj, vtx_array_obj_repo);
		return 1;
	}
}


static struct opengl_vertex_array_obj_t *opengl_vertex_array_obj_repo_get(struct linked_list_t *vtx_array_obj_repo, unsigned int id)
{
	struct opengl_vertex_array_obj_t *vao;

	/* ID 0 is reserved */
	if (id == 0)
		return NULL;

	/* Search VAO */
	LINKED_LIST_FOR_EACH(vtx_array_obj_repo)
	{
		vao = linked_list_get(vtx_array_obj_repo);
		assert(vao);
		if (vao->id == id && !vao->delete_pending)
			return vao;
	}

	/* Not found or being deleted */
	fatal("\t%s: requested VAO is not available (id=%d)",
		__FUNCTION__, id);
	return NULL;
}


/*
 * Public Functions
 */

void opengl_vertex_attrib_attach_buffer_obj(struct opengl_vertex_attrib_t *vattrib, struct opengl_buffer_obj_t *buffer_obj)
{
	/* Remove current buffer object bound to vertex attribute object */
	opengl_vertex_attrib_detach_buffer_obj(vattrib, vattrib->vbo);

	/* Attach new buffer object */
	vattrib->vbo = buffer_obj;
	opengl_buffer_obj_ref_update(buffer_obj, 1);
	list_add(buffer_obj->bound_vattribs, vattrib); /* To properly handle buffer object deleting */

	/* Debug */
	opengl_debug("\t%s: OpenGL Buffer Object #%d [%p] attach to Vertex Attribute [%p]\n", 
		__FUNCTION__, buffer_obj->id, buffer_obj, vattrib);
}

void opengl_vertex_attrib_detach_buffer_obj(struct opengl_vertex_attrib_t *vattrib, struct opengl_buffer_obj_t *buffer_obj)
{
	/* Remove buffer object bound to vertex attribute object */
	if (buffer_obj && vattrib->vbo && vattrib->vbo == buffer_obj)
	{
		list_remove(vattrib->vbo->bound_vattribs, vattrib);
		opengl_buffer_obj_ref_update(buffer_obj, -1);
		vattrib->vbo = NULL;

		/* Debug */
		opengl_debug("\t%s: OpenGL Buffer Object #%d [%p] detach from Vertex Attribute [%p]\n", 
			__FUNCTION__, buffer_obj->id, buffer_obj, vattrib);
	}
}

struct linked_list_t *opengl_vertex_array_obj_repo_create()
{
	struct linked_list_t *lst;

	/* Allocate */
	lst = linked_list_create();

	/* Debug */
	opengl_debug("\t%s: Vertex Array Repository [%p] created\n", __FUNCTION__, lst);

	/* Return */
	return lst;
}

void opengl_vertex_array_obj_repo_free(struct linked_list_t *vtx_array_obj_repo)
{
	struct opengl_vertex_array_obj_t *vao;

	LINKED_LIST_FOR_EACH(vtx_array_obj_repo)
	{
		vao = linked_list_get(vtx_array_obj_repo);
		opengl_vertex_array_obj_free(vao);
	}

	/* Debug */
	opengl_debug("\t%s: Vertex Array Repository [%p] freed\n", __FUNCTION__, vtx_array_obj_repo);
}


/* 
 * OpenGL API functions 
 */


/* Vertex Arrays [2.8] */


void glVertexPointer( GLint size, GLenum type,
	GLsizei stride, const GLvoid *ptr )
{
	__OPENGL_NOT_IMPL__
}


void glNormalPointer( GLenum type, GLsizei stride,
	const GLvoid *ptr )
{
	__OPENGL_NOT_IMPL__
}


void glColorPointer( GLint size, GLenum type,
	GLsizei stride, const GLvoid *ptr )
{
	__OPENGL_NOT_IMPL__
}

void glSecondaryColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	__OPENGL_NOT_IMPL__
}

void glIndexPointer( GLenum type, GLsizei stride,
	const GLvoid *ptr )
{
	__OPENGL_NOT_IMPL__
}

void glEdgeFlagPointer( GLsizei stride, const GLvoid *ptr )
{
	__OPENGL_NOT_IMPL__
}

void glFogCoordPointer (GLenum type, GLsizei stride, const GLvoid *pointer)
{
	__OPENGL_NOT_IMPL__
}

void glTexCoordPointer( GLint size, GLenum type,
	GLsizei stride, const GLvoid *ptr )
{
	__OPENGL_NOT_IMPL__
}

void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer)
{
	struct opengl_vertex_array_obj_t *vao;
	struct opengl_vertex_attrib_t *vattrib;
	struct opengl_buffer_binding_target_t *bbt;
	struct opengl_buffer_obj_t *buffer_obj;

	/* Debug */
	opengl_debug("API call %s(%d, %d, %x, %d, %d, %p)\n", 
		__FUNCTION__, index, size, type, normalized, stride, pointer);

	vao = opengl_ctx->vao_binding_point;
	bbt = opengl_buffer_binding_points_get_target(opengl_ctx->buffer_binding_points, GL_ARRAY_BUFFER);
	buffer_obj = opengl_buffer_obj_repo_get(buffer_repo, bbt->bound_buffer_id);
	opengl_buffer_obj_ref_update(buffer_obj, 1);

	if (vao)
	{
		vattrib = vao->attribs[index];
		vattrib->size = size;
		vattrib->type = type;
		vattrib->normalized = normalized;
		vattrib->stride = stride;
		vattrib->pointer = (unsigned int)pointer; /* FIXME: This is confusing, some sources say it's just an offset */
		vattrib->element_size = opengl_vertex_attrib_get_element_size(size, type);
		vattrib->vbo = buffer_obj;
	}
}

void glVertexAttribIPointer (GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	__OPENGL_NOT_IMPL__
}

void glVertexAttribLPointer (GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	__OPENGL_NOT_IMPL__
}

void glEnableClientState( GLenum cap )  /* 1.1 */
{
	__OPENGL_NOT_IMPL__
}

void glDisableClientState( GLenum cap )  /* 1.1 */
{
	__OPENGL_NOT_IMPL__
}

void glEnableVertexAttribArray (GLuint index)
{
	struct opengl_vertex_array_obj_t *vao;
	struct opengl_vertex_attrib_t *vattrib;

	/* Debug */
	opengl_debug("API call %s(%d)\n", __FUNCTION__, index);

	vao = opengl_ctx->vao_binding_point;
	if (vao)
	{
		vattrib = vao->attribs[index];
		vattrib->enabled = GL_TRUE;
	}
}

void glDisableVertexAttribArray (GLuint index)
{
	struct opengl_vertex_array_obj_t *vao;
	struct opengl_vertex_attrib_t *vattrib;

	/* Debug */
	opengl_debug("API call %s(%d)\n", __FUNCTION__, index);

	vao = opengl_ctx->vao_binding_point;
	if (vao)
	{
		vattrib = vao->attribs[index];
		vattrib->enabled = GL_FALSE;
	}

}

void glVertexAttribDivisor (GLuint index, GLuint divisor)
{
	__OPENGL_NOT_IMPL__
}

void glClientActiveTexture( GLenum texture )
{
	__OPENGL_NOT_IMPL__
}


void glArrayElement( GLint i )
{
	__OPENGL_NOT_IMPL__
}

/* Enable/Disable(PRIMITIVE_RESTART) */
void glPrimitiveRestartIndex (GLuint index)
{
	__OPENGL_NOT_IMPL__
}


/* Drawing Commands [2.8.3] [2.8.2] */

void glDrawArrays( GLenum mode, GLint first, GLsizei count )
{
	struct opengl_vertex_array_obj_t *vao;
	struct opengl_vertex_attrib_t *vattrib;
	struct opengl_buffer_obj_t *vbo;
	struct opengl_program_obj_t *program_obj;
	struct opengl_shader_obj_t *shader_obj;
	unsigned int vertex_shader_id;
	unsigned int pixel_shader_id;

	unsigned int num_workitems;
	unsigned int work_dim = 1; /* Currently always choose 1D */
	unsigned int global_work_offset[3];
	unsigned int global_work_size[3];
	unsigned int local_work_size[3];
	unsigned int group_count[3];
	unsigned int work_group_start[3];
	unsigned int work_group_count[3];
	unsigned int vs_ndrange_id;
	int max_work_groups_to_send;

	int i, j;

	/* Debug */
	opengl_debug("API call %s(%x, %d, %d)\n", 
		__FUNCTION__, mode, first, count);

	/* --------------------------- Vertex Shader Execution ----------------------------- */

	/* Find the ID of the vertex shader */
	program_obj = opengl_ctx->program_binding_point;
	LIST_FOR_EACH(program_obj->shaders, i)
	{
		shader_obj = list_get(program_obj->shaders, i);
		if (shader_obj)
		{
			switch(shader_obj->type)
			{
			case GL_VERTEX_SHADER:
			{
				vertex_shader_id = shader_obj->id;
				break;
			}
			case GL_FRAGMENT_SHADER:
			{
				pixel_shader_id = shader_obj->id;
				break;
			}
			default:
				break;
			}
		}
	}

	/* 
	 * VAO-	AttribArray[0]-VBO-Data 	Enabled
	 *	 |-	AttribArray[1]-VBO-Data 	Enabled
	 *	 |-	AttribArray[1]-VBO-Data 	Enabled
	 *	 |-	AttribArray[2]-VBO-Data 	Disabled
	 * 	......
	 * We need to pass the data in enabled attribute array to device memory
	 */
	vao = opengl_ctx->vao_binding_point;
	if (vao)
	{
		for (i = 0; i < MAX_VERTEX_ATTRIBS; ++i)
		{
			vattrib = vao->attribs[i];
			if (vattrib->enabled)
			{
				vbo = vattrib->vbo;
				if (vbo)
				{
					/* FIXME: data offset is not considered in current implementation */
					/* If already copied to device memory, free it */
					if (vbo->device_ptr)
						syscall(OPENGL_SYSCALL_CODE, opengl_abi_si_mem_free,
							vbo->device_ptr);
					/* Otherwise just allocate space in device memory */
					vbo->device_ptr = (unsigned int)syscall(OPENGL_SYSCALL_CODE, 
						opengl_abi_si_mem_alloc,
						vbo->size);
					/* Then send data to device memory */
					syscall(OPENGL_SYSCALL_CODE, opengl_abi_si_mem_write,
						vbo->device_ptr, vbo->data, vbo->size);
					/* Insert to input list */
					unsigned int sys_args[7];
					sys_args[0] = (unsigned int)vertex_shader_id;
					sys_args[1] = (unsigned int)vbo->device_ptr;
					sys_args[2] = (unsigned int)vattrib->size;
					sys_args[3] = (unsigned int)vattrib->type;
					sys_args[4] = (unsigned int)vbo->size;
					sys_args[5] = (unsigned int)i;
					sys_args[6] = (unsigned int)opengl_ctx->program_binding_point->id;
					syscall(OPENGL_SYSCALL_CODE, opengl_abi_si_shader_set_input,
						sys_args);

					/* Data is ready, now start processing NDrange information */
					/* Calculate # of workitems */
					num_workitems = vbo->size / vattrib->element_size;
					/* Work sizes */
					for (j = 0; j < work_dim; j++)
					{
						global_work_offset[j] = 0;
						global_work_size[j] = num_workitems;
						local_work_size[j] = num_workitems;
						assert(!(global_work_size[j] % 	local_work_size[j]));
						group_count[j] = global_work_size[j] / local_work_size[j];

					}

					/* Unused dimensions */
					for (j = work_dim; j < 3; j++)
					{
						global_work_offset[j] = 0;
						global_work_size[j] = 1;
						local_work_size[j] = 1;
						group_count[j] = global_work_size[j] / local_work_size[j];
					}

					for (j = 0; j < 3; j++)
					{
						work_group_start[j] = 0;
						work_group_count[j] = group_count[j];
					}

				
					/* Debug info */
					opengl_debug("\tVBO #%d data send to device memory, device_ptr = %x\n",
						vbo->id, vbo->device_ptr);
				}
				else
					fatal("Vertex Attribute at index %d is used with no data", i);
			}
		}

		/* Launch Vertex Shader */
		vs_ndrange_id = syscall(OPENGL_SYSCALL_CODE, 
			opengl_abi_si_ndrange_create,
			vertex_shader_id, work_dim, global_work_offset, global_work_size, local_work_size);
		opengl_debug("\tVertex Shader NDrange id = %d\n", vs_ndrange_id);

		syscall(OPENGL_SYSCALL_CODE, opengl_abi_si_ndrange_pass_mem_objs, 
			vertex_shader_id, vs_ndrange_id);

		syscall(OPENGL_SYSCALL_CODE,
			opengl_abi_si_ndrange_get_num_buffer_entries,
			&max_work_groups_to_send);

		syscall(OPENGL_SYSCALL_CODE, 
			opengl_abi_si_ndrange_send_work_groups, 
			&work_group_start[0], &work_group_count[0],
			group_count, vs_ndrange_id);

		syscall(OPENGL_SYSCALL_CODE, opengl_abi_si_ndrange_finish, vs_ndrange_id);

		/* Launch fragment shader */
		syscall(OPENGL_SYSCALL_CODE, opengl_abi_si_raster, mode, pixel_shader_id);
		opengl_debug("\tPixel Shader id = %d\n", pixel_shader_id);

		/* Finish fragment shader */
		syscall(OPENGL_SYSCALL_CODE, opengl_abi_si_raster_finish);

	}
	else
		opengl_debug("\tNo Vertex Array is available to render!\n");

	/*  */

}

void glDrawArraysInstanced (GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
	__OPENGL_NOT_IMPL__
}

void glDrawArraysInstancedBaseInstance (GLenum mode, GLint first, GLsizei count, GLsizei primcount, GLuint baseinstance)
{
	__OPENGL_NOT_IMPL__
}

void glDrawArraysIndirect (GLenum mode, const GLvoid *indirect)
{
	__OPENGL_NOT_IMPL__
}

void glMultiDrawArrays (GLenum mode, const GLint *first, const GLsizei *count, GLsizei primcount)
{
	__OPENGL_NOT_IMPL__
}

void glDrawElements( GLenum mode, GLsizei count,
                                      GLenum type, const GLvoid *indices )
{
	__OPENGL_NOT_IMPL__
}

void glDrawElementsInstanced (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount)
{
	__OPENGL_NOT_IMPL__
}

void glDrawElementsIndirect (GLenum mode, GLenum type, const GLvoid *indirect)
{
	__OPENGL_NOT_IMPL__
}

void glDrawElementsInstancedBaseInstance (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount, GLuint baseinstance)
{
	__OPENGL_NOT_IMPL__
}

void glDrawElementsInstancedBaseVertexBaseInstance (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount, GLint basevertex, GLuint baseinstance)
{
	__OPENGL_NOT_IMPL__
}

void glMultiDrawElements (GLenum mode, const GLsizei *count, GLenum type, const GLvoid* *indices, GLsizei primcount)
{
	__OPENGL_NOT_IMPL__
}

void glDrawRangeElements( GLenum mode, GLuint start,
	GLuint end, GLsizei count, GLenum type, const GLvoid *indices )
{
	__OPENGL_NOT_IMPL__
}

void glDrawElementsBaseVertex (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
	__OPENGL_NOT_IMPL__
}

void glDrawRangeElementsBaseVertex (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
	__OPENGL_NOT_IMPL__
}

void glDrawElementsInstancedBaseVertex (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex)
{
	__OPENGL_NOT_IMPL__
}

void glMultiDrawElementsBaseVertex (GLenum mode, const GLsizei *count, GLenum type, const GLvoid* *indices, GLsizei primcount, const GLint *basevertex)
{
	__OPENGL_NOT_IMPL__
}

void glInterleavedArrays( GLenum format, GLsizei stride,
                                           const GLvoid *pointer )
{
	__OPENGL_NOT_IMPL__
}


/* Vertex Array Objects [2.10] */


void glGenVertexArrays (GLsizei n, GLuint *arrays)
{
	struct opengl_vertex_array_obj_t *vao;
	int i;

	/* Debug */
	opengl_debug("API call %s(%d, %p)\n", __FUNCTION__, n, arrays);

	/* Generate and add to repository */
	for (i = 0; i < n; ++i)
	{
		vao = opengl_vertex_array_obj_create(MAX_VERTEX_ATTRIBS);
		opengl_vertex_array_obj_repo_add(opengl_ctx->vao_repo, vao);
		arrays[i] = vao->id;
	}
}

void glDeleteVertexArrays (GLsizei n, const GLuint *arrays)
{
	struct opengl_vertex_array_obj_t *vao;
	int i;

	/* Debug */
	opengl_debug("API call %s(%d, %p)\n", __FUNCTION__, n, arrays);

	/* Delete and remove from repository */
	for (i = 0; i < n; ++i)
	{
		vao = opengl_vertex_array_obj_repo_get(opengl_ctx->vao_repo, arrays[i]);
		opengl_vertex_array_obj_delete(vao);
		opengl_vertex_array_obj_repo_remove(opengl_ctx->vao_repo, vao);
	}	
}

void glBindVertexArray (GLuint array)
{
	struct opengl_vertex_array_obj_t *vao;

	/* Debug */
	opengl_debug("API call %s(%d)\n", __FUNCTION__, array);

	/* Get and Bind */
	if (array)
	{
		vao = opengl_vertex_array_obj_repo_get(opengl_ctx->vao_repo, array);
		opengl_vertex_array_obj_ref_update(vao, 1);
		opengl_ctx->vao_binding_point = vao;
	}
	else
		opengl_ctx->vao_binding_point = NULL;
}

GLboolean glIsVertexArray (GLuint array)
{
	__OPENGL_NOT_IMPL__
	return 0;
}
