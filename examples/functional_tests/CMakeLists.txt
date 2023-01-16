set(client_functional_tests_SRCS
    client_functional_tests.c
)

IF(MSVC)
set_source_files_properties(${client_functional_tests_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF(MSVC)

add_executable(client_functional_tests
  ${client_functional_tests_SRCS}
)

target_link_libraries(client_functional_tests
    iec61850
)