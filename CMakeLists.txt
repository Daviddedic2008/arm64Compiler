cmake_minimum_required(VERSION 3.10)

project(GPC LANGUAGES C)

# c23 standard for certain features
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

add_executable(GPC
    src/main.c
	src/frontend/lexer/tokenizer.c
	src/frontend/parser/astGen.c
	src/frontend/parser/3opIrGen.c
	src/helper/arenaAlloc.c
	src/helper/algos.c
	src/helper/filereader.c
	src/tester/testGen.c
	src/backend/regAllocator/regAlloc.c
	src/backend/interPasses/rce.c
)

set_source_files_properties(src/tester/testGen.c PROPERTIES COMPILE_DEFINITIONS "main=testGen_main")

option(GPC_GDB_BUILD "unoptimized unstripped build for gdb (bld -g)" OFF)
if(GPC_GDB_BUILD)
	# -fgnu89-inline: plain `inline` funcs (compareFixed) get an out-of-line definition, otherwise -O0 fails to link
	target_compile_options(${PROJECT_NAME} PRIVATE -O0 -g3 -fno-omit-frame-pointer -fgnu89-inline)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -O2)
	target_link_options(${PROJECT_NAME} PRIVATE -s)
endif()
target_compile_options(${PROJECT_NAME} PRIVATE -fstack-usage)
target_compile_options(${PROJECT_NAME} PRIVATE -w)
