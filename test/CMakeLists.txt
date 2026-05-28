
set(SRC_CXX_FILES
    "src/crx2bmp.cpp"
)

foreach(SRC_FILE IN LISTS SRC_CXX_FILES)
    get_filename_component(FILE_NAME ${SRC_FILE} NAME_WE)
    add_executable(${FILE_NAME} ${SRC_FILE})
    target_link_libraries(${FILE_NAME} utils)
endforeach()