cmake_minimum_required(VERSION 3.16)

project(cdx_cj_nppm_dev_cpp_pixel_2_ray LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(CTest)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Test OpenGL OpenGLWidgets)

add_library(pixel_2_ray_core
    src/FrameMath.cpp
    src/InteractionLogic.cpp
)

target_include_directories(pixel_2_ray_core PUBLIC src)
target_link_libraries(pixel_2_ray_core PUBLIC Qt6::Gui)

add_executable(pixel_2_ray_qt
    src/main.cpp
    src/MainWindow.cpp
    src/PixelToRayWidget.cpp
)

target_link_libraries(pixel_2_ray_qt PRIVATE
    pixel_2_ray_core
    Qt6::Widgets
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

if(BUILD_TESTING)
    add_executable(pixel_2_ray_frame_tests
        tests/FrameMathTests.cpp
    )

    target_link_libraries(pixel_2_ray_frame_tests PRIVATE
        pixel_2_ray_core
        Qt6::Test
        Qt6::Gui
    )

    add_test(NAME pixel_2_ray_frame_tests COMMAND pixel_2_ray_frame_tests)

    add_executable(pixel_2_ray_interaction_tests
        tests/InteractionLogicTests.cpp
    )

    target_link_libraries(pixel_2_ray_interaction_tests PRIVATE
        pixel_2_ray_core
        Qt6::Test
        Qt6::Gui
    )

    add_test(NAME pixel_2_ray_interaction_tests COMMAND pixel_2_ray_interaction_tests)
endif()
