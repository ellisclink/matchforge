# matchforge_metrics: Prometheus metrics for the server (connections, logins, matches,
# queue wait). The HTTP endpoint that serves them is started in the server's main().

add_library(matchforge_metrics STATIC Metrics.cpp)

target_include_directories(matchforge_metrics PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(matchforge_metrics
  PUBLIC
    prometheus-cpp::core   # counters, gauges, histograms, the registry
  PRIVATE
    matchforge_warnings)
