# Minimal, framework-free tests linked against the core library.
add_executable(validator_tests validator_tests.cpp)
target_link_libraries(validator_tests PRIVATE ecfdgii_core)
add_test(NAME validator_tests COMMAND validator_tests)

add_executable(hmac_tests hmac_tests.cpp)
target_link_libraries(hmac_tests PRIVATE ecfdgii_core)
add_test(NAME hmac_tests COMMAND hmac_tests)

add_executable(xsd_tests xsd_tests.cpp)
target_link_libraries(xsd_tests PRIVATE ecfdgii_core)
add_test(NAME xsd_tests COMMAND xsd_tests)

add_executable(parity_tests parity_tests.cpp)
target_link_libraries(parity_tests PRIVATE ecfdgii_core)
add_test(NAME parity_tests COMMAND parity_tests)

