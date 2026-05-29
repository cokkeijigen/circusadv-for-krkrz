project(utils)

find_package(ZLIB REQUIRED)

add_library(utils OBJECT
	"crx/image_crx.cpp"
	"crx/image_crx2.cpp"	
	
	"tp_stub/tp_stub.cpp" 
	"ncbind/ncbind.cpp"
)

target_include_directories(utils PUBLIC
	${CMAKE_CURRENT_LIST_DIR}
	"crx"
	"tp_stub"
	"ncbind"
)

target_link_libraries(utils PRIVATE 
	ZLIB::ZLIB
)