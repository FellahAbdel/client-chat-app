from SCons.Script import *

# Get the list of source files
sources = Glob("*.c")

# Common compiler flags
common_flags = ["-Wall", "-Wextra", "-Werror", "-g"]

# Environment with -DBIN flag
env_with_BIN = Environment(CCFLAGS=common_flags + ["-DBIN"])
obj_with_BIN = env_with_BIN.Object("client-chat_BIN.o", "client-chat.c")

#Environment with -DFILEIO flag
env_with_FILE = Environment(CCFLAGS=common_flags + ["-DFILEIO"])
obj_with_FILE = env_with_FILE.Object("client-chat_FILE.o", "client-chat.c")


# Environment without -DBIN flag
env_without_BIN = Environment(CCFLAGS=common_flags)
obj_without_BIN = env_without_BIN.Object("client-chat_without_BIN.o", "client-chat.c")

# Link object files to create executables
program_with_BIN = env_with_BIN.Program("client-chat-bin", [obj_with_BIN])
program_with_FILE = env_with_FILE.Program("client-chat-file", [obj_with_FILE])
program_without_BIN = env_without_BIN.Program("client-chat", [obj_without_BIN])

# Clean up *.o files after build
Clean(program_with_BIN, "*.o")
Clean(program_without_BIN, "*.o")
Clean(program_with_FILE, "*.o")

# Alias for both executables
Alias('all', [program_with_BIN, program_without_BIN, program_with_FILE])