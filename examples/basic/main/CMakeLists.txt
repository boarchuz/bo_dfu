set(srcs
    "main.c"
)

idf_component_register(
    SRCS ${srcs}
)

# Always rebuild main to ensure binary has updated __TIMESTAMP__
add_custom_target(file_toucher
    COMMAND ${CMAKE_COMMAND} -E touch_nocreate ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/main.c
)
