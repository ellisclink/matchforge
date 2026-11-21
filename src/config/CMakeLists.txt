# matchforge_config: loads and validates the server's JSON settings file.

add_library(matchforge_config STATIC ServerConfig.cpp)

target_include_directories(matchforge_config PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(matchforge_config
  PUBLIC
    matchforge_core              # MatchMode, MatchSettings, name rules
  PRIVATE
    nlohmann_json::nlohmann_json # JSON parser
    matchforge_warnings)
