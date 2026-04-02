add_subdirectory(json)

if(USE_SOLUTION_FOLDERS)
    set_property(TARGET eap-sma-assemblyjson PROPERTY FOLDER "assembly")
endif()