function(rfp_set_project_options target_name)
    target_compile_features(${target_name} PUBLIC cxx_std_20)

    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endfunction()
