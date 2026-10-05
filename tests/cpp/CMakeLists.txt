# One executable per module; each is its own CTest test.
set(SKIN_TESTS optimizer validation)

foreach(name IN LISTS SKIN_TESTS)
    add_executable(test_${name} test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE skin_core)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()
