#
# This file is part of Project SkyFire https://www.projectskyfire.org.
# See COPYRIGHT file for Copyright information
#

add_executable(database_queue_tests
  ${CMAKE_SOURCE_DIR}/src/server/shared/Database/DatabaseQueue.cpp
  DatabaseQueueTests.cpp
)

target_include_directories(database_queue_tests PRIVATE
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src/server/shared
  ${CMAKE_SOURCE_DIR}/src/server/shared/Configuration
  ${CMAKE_SOURCE_DIR}/src/server/shared/Database
  ${CMAKE_SOURCE_DIR}/src/server/shared/DataStores
  ${CMAKE_SOURCE_DIR}/src/server/shared/Debugging
  ${CMAKE_SOURCE_DIR}/src/server/shared/Dynamic
  ${CMAKE_SOURCE_DIR}/src/server/shared/Logging
  ${CMAKE_SOURCE_DIR}/src/server/shared/Network
  ${CMAKE_SOURCE_DIR}/src/server/shared/Packets
  ${CMAKE_SOURCE_DIR}/src/server/shared/Platform
  ${CMAKE_SOURCE_DIR}/src/server/shared/Threading
  ${CMAKE_SOURCE_DIR}/src/server/shared/Utilities
  ${Boost_INCLUDE_DIRS}
  ${MYSQL_INCLUDE_DIR}
)

if(TARGET Boost::headers)
  target_link_libraries(database_queue_tests Boost::headers)
elseif(TARGET Boost::boost)
  target_link_libraries(database_queue_tests Boost::boost)
endif()

target_link_libraries(database_queue_tests Threads::Threads)

add_test(NAME database_queue_tests COMMAND database_queue_tests)
