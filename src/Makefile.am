SUBDIRS = \
	arch \
	dram \
	driver \
	lib \
	memory \
	network \
	visual

bin_PROGRAMS = $(top_builddir)/bin/m2s

__top_builddir__bin_m2s_SOURCES = \
	\
	m2s.cc \
	m2s-old.c \
	\
	Wrapper.cc \
	Wrapper.h

AM_LIBTOOLFLAGS = --preserve-dup-deps
AM_CPPFLAGS = @M2S_INCLUDES@

LDADD = \
	$(top_builddir)/src/driver/cuda/libcuda.a \
	$(top_builddir)/src/driver/glu/libglu.a \
	$(top_builddir)/src/driver/glut/libglut.a \
	$(top_builddir)/src/driver/glew/libglew.a \
	$(top_builddir)/src/driver/opencl/libopencl.a \
	$(top_builddir)/src/driver/opencl-old/evergreen/libopencl.a \
	$(top_builddir)/src/driver/opengl/libopengl.a \
	$(top_builddir)/src/driver/opengl/southern-islands/libsi_gl.a \
	$(top_builddir)/src/driver/common/libcommon.a \
	\
	$(top_builddir)/src/arch/x86/timing/libtiming.a \
	$(top_builddir)/src/arch/x86/emu/libemu.a \
	$(top_builddir)/src/arch/x86/asm/libasm.a \
	\
	$(top_builddir)/src/arch/arm/timing/libtiming.a \
	$(top_builddir)/src/arch/arm/emu/libemu.a \
	$(top_builddir)/src/arch/arm/asm/libasm.a \
	\
	$(top_builddir)/src/arch/evergreen/timing/libtiming.a \
	$(top_builddir)/src/arch/evergreen/emu/libemu.a \
	$(top_builddir)/src/arch/evergreen/asm/libasm.a \
	\
	$(top_builddir)/src/arch/fermi/timing/libtiming.a \
	$(top_builddir)/src/arch/fermi/emu/libemu.a \
	$(top_builddir)/src/arch/fermi/asm/libasm.a \
	\
	$(top_builddir)/src/arch/hsa/asm/libasm.a \
	$(top_builddir)/src/arch/hsa/emu/libemu.a \
	$(top_builddir)/src/arch/hsa/driver/libdriver.a \
	\
	$(top_builddir)/src/arch/kepler/driver/libdriver.a \
	$(top_builddir)/src/arch/kepler/emu/libemu.a \
	$(top_builddir)/src/arch/kepler/asm/libasm.a \
	\
	$(top_builddir)/src/arch/mips/timing/libtiming.a \
	$(top_builddir)/src/arch/mips/emu/libemu.a \
	$(top_builddir)/src/arch/mips/asm/libasm.a \
	\
	$(top_builddir)/src/arch/southern-islands/timing/libtiming.a \
	$(top_builddir)/src/arch/southern-islands/driver/libdriver.a \
	$(top_builddir)/src/arch/southern-islands/emu/libemu.a \
	$(top_builddir)/src/driver/opengl/libopengl.a \
	$(top_builddir)/src/driver/opengl/southern-islands/libsi_gl.a \
	$(top_builddir)/src/arch/southern-islands/asm/libasm.a \
	\
	$(top_builddir)/src/arch/common/libcommon.a \
	\
	$(top_builddir)/src/dram/libdram.a \
	$(top_builddir)/src/memory/libmemory.a \
	$(top_builddir)/src/network/libnetwork.a \
	\
	$(top_builddir)/src/visual/common/libcommon.a \
	$(top_builddir)/src/visual/evergreen/libevergreen.a \
	$(top_builddir)/src/visual/memory/libmemory.a \
	$(top_builddir)/src/visual/network/libnetwork.a \
	$(top_builddir)/src/visual/southern-islands/libsouthernislands.a \
	$(top_builddir)/src/visual/x86/libx86.a \
	$(top_builddir)/src/visual/common/libcommon.a \
	\
	$(top_builddir)/src/lib/mhandle/libmhandle.a \
	$(top_builddir)/src/lib/esim/libesim.a \
	$(top_builddir)/src/lib/util/libutil.a \
	$(top_builddir)/src/lib/class/libclass.a \
	$(top_builddir)/src/lib/cpp/libcpp.a

if ENABLE_DEBUG
AM_LDFLAGS = -rdynamic
endif

if HAVE_GTK
AM_CFLAGS = @GTK_CFLAGS@
LDADD += @GTK_LIBS@
endif

if HAVE_OPENGL
LDADD += @OPENGL_LDFLAGS@
endif

LDADD += -lpthread -lz -lm -lstdc++

