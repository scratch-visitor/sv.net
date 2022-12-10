
set(SOURCE_FILES
  sv/net.cpp
  sv/net/core.cpp
)
set(HEADER_FILES
  sv/net.hpp
  sv/net/core.hpp
  sv/net/define.hpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES}
${HEADER_FILES})
target_compile_definitions(${PROJECT_NAME} PRIVATE SV_NET_EXPORTS)

