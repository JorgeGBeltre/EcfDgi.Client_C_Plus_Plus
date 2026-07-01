# Minimal, framework-free tests linked against the core library.
add_executable(validator_tests validator_tests.cpp)
target_link_libraries(validator_tests PRIVATE ecfdgii_core)
add_test(NAME validator_tests COMMAND validator_tests)
