#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

PROJECT_VERSION := $(shell cd ${PROJECT_PATH} && git describe --always --tags --dirty 2> /dev/null)
CFLAGS += -DPROJECT_VERSION=\"${PROJECT_VERSION}\"
