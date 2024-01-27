set(include_dirs
    "include"
)

set(srcs
)

if(CONFIG_BO_DFU_DEFAULT)
    list(APPEND srcs
        "src/bo_dfu.c"
    )
endif()

set(requires
    bootloader_support
    spi_flash
    hal
)

if(${IDF_VERSION_MAJOR} GREATER_EQUAL 5)
    list(APPEND requires
        esp_app_format
    )
endif()

idf_component_register(
    INCLUDE_DIRS ${include_dirs}
    SRCS ${srcs}
    REQUIRES ${requires}
)

if(CONFIG_BO_DFU_DEFAULT)
    idf_build_set_property(LINK_OPTIONS "-Wl,--require-defined=bootloader_after_init" APPEND)
endif()
