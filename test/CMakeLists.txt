find_package(OpenCV REQUIRED COMPONENTS core imgproc)

add_executable(gaussian_filter_tests gaussian_filter_test.cpp)
target_include_directories(gaussian_filter_tests PRIVATE ../include)
target_link_libraries(gaussian_filter_tests PRIVATE filters_lib GTest::gtest_main ${OpenCV_LIBS})
add_test(NAME gaussian_filter_tests COMMAND gaussian_filter_tests)

add_executable(grayscale_tests grayscale_test.cpp)
target_include_directories(grayscale_tests PRIVATE ../include)
target_link_libraries(grayscale_tests PRIVATE filters_lib GTest::gtest_main ${OpenCV_LIBS})
add_test(NAME grayscale_tests COMMAND grayscale_tests)
