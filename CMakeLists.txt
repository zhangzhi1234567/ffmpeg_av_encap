PROJECT(ffmpeg_av_encap)


SET(CMAKE_C_FLAGS "-O0 -g -ggdb -fPIC -std=gnu99")
SET(CMAKE_CXX_FLAGS "-O0 -g -ggdb -fPIC -std=c++11")

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src/audio_encap)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)
link_directories(${PROJECT_SOURCE_DIR}/lib)


SET(SRC_PATH ${PROJECT_SOURCE_DIR}/src/audio_encap)

SET(SRC_LIST ${SRC_PATH}/audio_engine.h  ${SRC_PATH}/audio_engine.cpp main.cpp)

ADD_EXECUTABLE(test ${SRC_LIST})

TARGET_LINK_LIBRARIES(test avcodec)
TARGET_LINK_LIBRARIES(test avformat)
TARGET_LINK_LIBRARIES(test avutil)
TARGET_LINK_LIBRARIES(test avdevice)
TARGET_LINK_LIBRARIES(test swresample)
TARGET_LINK_LIBRARIES(test swscale)


