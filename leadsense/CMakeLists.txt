find_package(EvoBinoSDK REQUIRED)

include_directories(${EvoBinoSDK_INCLUDE_DIRS})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/leadsense)

add_executable(leadsense leadsense.cpp)
target_link_libraries(leadsense ${STEREOCAMERA_LIBS} ${PROJECT_NAME})

add_executable(leadsense_imu leadsense_imu.cpp)
target_link_libraries(leadsense_imu ${STEREOCAMERA_LIBS} ${PROJECT_NAME})
