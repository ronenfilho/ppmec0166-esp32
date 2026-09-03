# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS
        "main.c"
        "connect.c"
        "clock.c"

    INCLUDE_DIRS
        "."
)
