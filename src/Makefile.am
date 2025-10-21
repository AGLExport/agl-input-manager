AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

bin_PROGRAMS = agl-input-manager

agl_input_manager_SOURCES = \
	device-udev.c \
	device-input.c \
	agl-input-manager.c

# options
# Additional library
agl_input_manager_LDADD = \
	-lrt -lpthread \
	@LIBSYSTEMD_LIBS@ \
	@LIBUDEV_LIBS@ \
	@LIBEVDEV_LIBS@

# C compiler options
agl_input_manager_CFLAGS = \
	-g -Wall -Wno-unused-but-set-variable \
	-I$(top_srcdir)/3rdparty/wpa-supplicant \
	-I$(top_srcdir)/include \
	-D_GNU_SOURCE \
	@LIBSYSTEMD_CFLAGS@ \
	@LIBUDEV_CFLAGS@ \
	@LIBEVDEV_CFLAGS@


# C++ compiler options
agl_input_manager_CXXFLAGS = \
	-g -Wall -Wno-unused-but-set-variable \
	-I$(top_srcdir)/3rdparty/wpa-supplicant \
	-I$(top_srcdir)/include \
	-D_GNU_SOURCE \
	@LIBSYSTEMD_CFLAGS@ \
	@LIBUDEV_CFLAGS@ \
	@LIBEVDEV_CFLAGS@


# Linker options
agl_input_manager_LDFLAGS =


# configure option
if ENABLE_ADDRESS_SANITIZER
CFLAGS   += -fsanitize=address
endif

if ENABLE_GCOV
CFLAGS   += -coverage
endif

agl_input_manager_CFLAGS   += -D_CRITICAL_ERROR_OUT_STDERROR
agl_input_manager_CXXFLAGS += -D_CRITICAL_ERROR_OUT_STDERROR

if ENABLE_PRINTFDEBUG
agl_input_manager_CFLAGS   += -D_PRINTF_DEBUG_
agl_input_manager_CXXFLAGS += -D_PRINTF_DEBUG_
endif

CLEANFILES = *.gcda *.gcno
