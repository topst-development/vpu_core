# config/common_version.cmake

# Function to extract version components from the header file
function(extract_define_value header_file define_name output_variable)
    file(READ "${header_file}" header_content)
    string(REGEX MATCH "${define_name}[ \t]+\"([^\"]*)\"" matched_line "${header_content}")
    if(matched_line)
        string(REGEX REPLACE ".*${define_name}[ \t]+\"([^\"]*)\".*" "\\1" define_value "${matched_line}")
        set(${output_variable} "${define_value}" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Could not find ${define_name} in ${header_file}")
    endif()
endfunction()
