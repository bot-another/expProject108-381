file (GLOB LIB_SOURCES "*.cc" "util/*.cc" "tcp_helpers/*.cc" "*.c")
add_library (sponge STATIC ${LIB_SOURCES})