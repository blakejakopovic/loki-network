
set(ABYSS src)

set(ABYSS_LIB abyss)

include_directories(include)

set(ABYSS_SRC
  ${ABYSS}/http.cpp
  ${ABYSS}/client.cpp
  ${ABYSS}/server.cpp
  ${ABYSS}/json.cpp
  )

add_library(${ABYSS_LIB} ${ABYSS_SRC})
