project_home       = ..
build_dir          = $(project_home)
bin_dir            = $(project_home)/bin/linux
object_dir         = $(project_home)/.objs
stupid_home        = $(project_home)/../../stupid
gnu_home           = $(stupid_home)/gnu



# includes of stupid headers
stupid_inc_path    = $(stupid_home)/inc
stupid_includes    = -I$(stupid_inc_path)

# includes of daemon headers
daemon_inc_path    = $(project_home)/inc
daemon_includes    = -I$(daemon_inc_path)

# all includes that daemon solution needs
includes           = $(stupid_includes)
includes          += $(daemon_includes)



# source files of daemon solution
daemon_src_path    = $(project_home)/src
daemon_source      = $(filter %.cpp, $(shell find $(daemon_src_path) -depth -name "*.cpp"))



# objects of daemon solution
daemon_objects     = $(daemon_source:$(project_home)%.cpp=$(object_dir)%.o)



# system librarys
system_libs        = -lpthread -ldl

# open source librarys
cmarkup_lib_inc    = $(gnu_home)/cmarkup/lib/linux
gnu_libs           = -L$(cmarkup_lib_inc) -lcmarkup

# stupid librarys
stupid_lib_inc     = $(stupid_home)/lib/linux
stupid_libs        = -L$(stupid_lib_inc) -lstupid_net -lstupid_base

# daemon depends librarys
depend_libs        = $(stupid_libs)
depend_libs       += $(gnu_libs)
depend_libs       += $(system_libs)



# output execution
output_exec        = $(bin_dir)/daemon



# my g++ not support nullptr and 64bits
# so no -m64, and add a macro -Dnullptr=0

# build flags for objects
build_objs_flags   = -std=c++0x -g -Wall -O1 -pipe -fPIC -Dnullptr=0

# build flags for execution
build_exec_flags   = $(build_objs_flags)



# build targets

# let 'build' be default target, build all targets
build   : $(daemon_objects)
	@echo
	@echo "@@@@@  start making daemon  @@@@@"
	g++ $(build_exec_flags) -o $(output_exec) $^ $(depend_libs)
	@echo "@@@@@  make daemon success  @@@@@"
	@echo

cpfile  :
	@cp $(stupid_lib_inc)/* $(bin_dir)/
	@cp $(cmarkup_lib_inc)/* $(bin_dir)/

run     : cpfile
	@cp $(bin_dir)/* $(project_home)/
	@export LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(project_home) && $(project_home)/daemon
# it same as: @env LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$(project_home) $(project_home)/daemon

# build all objects
$(object_dir)/%.o:$(project_home)/%.cpp
	@dir=`dirname $@`;      \
    if [ ! -d $$dir ]; then \
        mkdir -p $$dir;     \
    fi
	g++ -c $(build_objs_flags) $(includes) -o $@ $<

clean    :
	rm -rf $(object_dir) $(bin_dir)/*

rebuild  : clean build
