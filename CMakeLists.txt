cmake_minimum_required(VERSION 3.8)
project(topsbot_yolo_det)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

# colcon 未传 CMAKE_BUILD_TYPE 时默认为 Release，避免热路径（YOLO post）以 -O0 编译
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release, Debug, RelWithDebInfo)")
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Og -g)
  else()
    # 对齐 model-zoo YOLO11 sample（-O3）
    add_compile_options(-O3 -DNDEBUG)
  endif()
endif()

message(STATUS "topsbot_yolo_det: CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(tb_img_msgs REQUIRED)
find_package(tb_det_msgs REQUIRED)
find_package(OpenCV REQUIRED)

if(CMAKE_SYSROOT)
  set(TOPSBOT_TACV_ROOT "${CMAKE_SYSROOT}/usr/local")
else()
  set(TOPSBOT_TACV_ROOT "/usr/local")
endif()

set(TOPSBOT_TACV_INCLUDE_DIR "${TOPSBOT_TACV_ROOT}/include")
set(TOPSBOT_TACV_LIB_DIR "${TOPSBOT_TACV_ROOT}/lib")
set(TOPSBOT_RUNTIME_INCLUDE_DIR "${TOPSBOT_TACV_ROOT}/include/ta-runtime")

add_library(topsbot_yolo SHARED
  topsbot_yolo/src/quant_utils.cpp
  topsbot_yolo/src/nms_utils.cpp
  topsbot_yolo/src/dmabuf_allocator.cpp
  topsbot_yolo/src/ta_image.cpp
  topsbot_yolo/src/image_source.cpp
  topsbot_yolo/src/ta_runtime_engine.cpp
  topsbot_yolo/src/nv12_phy_input_feeder.cpp
  topsbot_yolo/src/nv12_letterbox_preprocessor.cpp
  topsbot_yolo/src/yolo11_nv12_detector.cpp
  topsbot_yolo/src/yolo26_nv12_detector.cpp
  topsbot_yolo/src/yolo_detector_factory.cpp
  topsbot_yolo/src/viz_renderer.cpp
  topsbot_yolo/src/yolo_session.cpp
)

target_include_directories(topsbot_yolo PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/topsbot_yolo/include>
  $<INSTALL_INTERFACE:include>
  ${TOPSBOT_TACV_INCLUDE_DIR}
  ${TOPSBOT_RUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)

target_link_directories(topsbot_yolo PUBLIC ${TOPSBOT_TACV_LIB_DIR})

target_link_libraries(topsbot_yolo
  tacocv
  dmabufheap
  tacosys
  ta_runtime
  ${OpenCV_LIBS}
)

ament_target_dependencies(topsbot_yolo
  sensor_msgs
)

add_executable(yolo_det_node
  src/yolo_det_node.cpp
  src/yolo_det_main.cpp
)

target_include_directories(yolo_det_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(yolo_det_node topsbot_yolo)

add_executable(yolo_det_file_cli src/yolo_det_file_cli.cpp)
target_link_libraries(yolo_det_file_cli topsbot_yolo)

ament_target_dependencies(yolo_det_node
  rclcpp
  sensor_msgs
  tb_img_msgs
  tb_det_msgs
)

install(TARGETS topsbot_yolo yolo_det_node yolo_det_file_cli
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY topsbot_yolo/include/
  DESTINATION include
)
install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

install(PROGRAMS
  scripts/download_models.sh
  scripts/ensure_models.sh
  DESTINATION share/${PROJECT_NAME}/scripts
)
install(PROGRAMS scripts/lib/aliyun_download.sh scripts/lib/path_utils.sh
  DESTINATION share/${PROJECT_NAME}/scripts/lib
)

ament_export_include_directories(include topsbot_yolo/include)
ament_export_libraries(topsbot_yolo)
ament_export_dependencies(
  rclcpp
  sensor_msgs
  tb_img_msgs
  tb_det_msgs
)

ament_package()
