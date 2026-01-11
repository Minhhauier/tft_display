# Gather all C source files in this component
file(GLOB SRCS "*.c")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS include
    REQUIRES driver led_strip
)