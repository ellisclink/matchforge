# Turns matchforge.proto into C++ source files during the build, then compiles
# them into the matchforge_proto library.

# Locates Protobuf from vcpkg. Afterwards the imported target protobuf::protoc
# (the compiler program) and protobuf::libprotobuf (the runtime library) exist.
find_package(Protobuf CONFIG REQUIRED)

# The input file, as an absolute path so it works from any build folder.
set(MATCHFORGE_PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/matchforge.proto")

# Generated files go into the build folder (build/<preset>/...), never into the
# source tree, so you never edit or commit them by accident.
set(MATCHFORGE_PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(MATCHFORGE_PROTO_SRCS "${MATCHFORGE_PROTO_GEN_DIR}/matchforge.pb.cc")
set(MATCHFORGE_PROTO_HDRS "${MATCHFORGE_PROTO_GEN_DIR}/matchforge.pb.h")

# protoc will not create the output folder itself, so create it here.
file(MAKE_DIRECTORY "${MATCHFORGE_PROTO_GEN_DIR}")

# A build rule: "to make these two output files, run protoc on the .proto file".
# CMake re-runs it only when matchforge.proto (or protoc itself) changes.
#   $<TARGET_FILE:protobuf::protoc>   the full path to the protoc program
#   --cpp_out=...                     write C++ files into the generated folder
#   --proto_path=...                  folder where imports are searched for
# CMake 3.22 has no protobuf_generate(TARGET ...) helper, so we call protoc directly.
# That works on any CMake 3.21 or newer.
add_custom_command(
  OUTPUT ${MATCHFORGE_PROTO_SRCS} ${MATCHFORGE_PROTO_HDRS}
  COMMAND $<TARGET_FILE:protobuf::protoc>
          --cpp_out=${MATCHFORGE_PROTO_GEN_DIR}
          --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
          ${MATCHFORGE_PROTO_FILE}
  DEPENDS ${MATCHFORGE_PROTO_FILE} protobuf::protoc
  COMMENT "Generating C++ from matchforge.proto"
  VERBATIM)  # Pass the arguments through exactly as written, with no shell surprises.

# Compiles the generated .pb.cc into a static library (a .a file that gets linked
# into programs). Listing the headers too makes IDEs show them in the project tree.
add_library(matchforge_proto STATIC ${MATCHFORGE_PROTO_SRCS} ${MATCHFORGE_PROTO_HDRS})

# PUBLIC means anything that links to matchforge_proto also gets this include
# folder. So code can write #include "matchforge.pb.h" without any extra setup.
target_include_directories(matchforge_proto PUBLIC "${MATCHFORGE_PROTO_GEN_DIR}")

# The generated code calls the Protobuf runtime, so it must link libprotobuf.
# PUBLIC passes that requirement on to anything that uses matchforge_proto.
target_link_libraries(matchforge_proto PUBLIC protobuf::libprotobuf)

# Generated code is not compiled with our strict warning flags, because we do
# not control its style. Only hand-written code gets -Werror.
