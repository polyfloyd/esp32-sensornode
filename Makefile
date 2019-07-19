PROJECT_NAME := sensornode

space :=
space +=

include $(IDF_PATH)/make/project.mk

compile_flags.txt:
	echo ${subst ${space},\\n,${CFLAGS}} > $@
	echo ${subst ${space},\\n,${CPPFLAGS}} >> $@
	echo ${addprefix \\n-I,${COMPONENT_INCLUDES}} >> $@
	sed -i 's/ $$//g' $@
