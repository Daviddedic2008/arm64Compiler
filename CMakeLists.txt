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
	src/helper/filereader.c
	src/tester/testGen.c
	src/backend/regAllocator/regAllocator.c
)
target_compile_options(${PROJECT_NAME} PRIVATE -O2)
target_link_options(${PROJECT_NAME} PRIVATE -s)
target_compile_options(${PROJECT_NAME} PRIVATE -fstack-usage)
target_compile_options(${PROJECT_NAME} PRIVATE -w)
