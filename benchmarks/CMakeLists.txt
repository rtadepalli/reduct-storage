add_executable(benchmarks benschmarks.cc reduct/storage/entry_benchmarks.cc)

target_link_libraries(benchmarks PRIVATE reduct)
target_link_libraries(benchmarks PRIVATE ${CONAN_LIBS})