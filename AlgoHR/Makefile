TARGET_EXEC ?= benchmarkHR

BUILD_DIR ?= build
SRC_DIRS ?= src

# Find all source files (.cpp, .c, .s)
SRCS := $(wildcard $(SRC_DIRS)/**/*.cpp $(SRC_DIRS)/**/*.c $(SRC_DIRS)/**/*.s $(SRC_DIRS)/*.c)
$(info Compiling source files: $(SRCS))

# Create corresponding object files
OBJS := $(SRCS:$(SRC_DIRS)/%.cpp=$(BUILD_DIR)/%.cpp.o)
OBJS := $(OBJS:$(SRC_DIRS)/%.c=$(BUILD_DIR)/%.c.o)
OBJS := $(OBJS:$(SRC_DIRS)/%.s=$(BUILD_DIR)/%.s.o)

# Include directories manually (update these to match your project structure)
INC_DIRS := src/dummy
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

# Compilation flags
CPPFLAGS ?= $(INC_FLAGS) -MMD -MP
CFLAGS ?= -Wall -g
LDFLAGS ?= -static -mconsole

# Main build rule
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	@powershell -Command "if (!(Test-Path '$(BUILD_DIR)')) { New-Item -ItemType Directory -Path '$(BUILD_DIR)' }"
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Rule for compiling .c files to object files
$(BUILD_DIR)/%.c.o: $(SRC_DIRS)/%.c
	@powershell -Command "if (!(Test-Path '$(dir $@)')) { New-Item -ItemType Directory -Path '$(dir $@)' }"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Rule for compiling .cpp files to object files
$(BUILD_DIR)/%.cpp.o: $(SRC_DIRS)/%.cpp
	@powershell -Command "if (!(Test-Path '$(dir $@)')) { New-Item -ItemType Directory -Path '$(dir $@)' }"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Rule for compiling .s (assembly) files to object files
$(BUILD_DIR)/%.s.o: $(SRC_DIRS)/%.s
	@powershell -Command "if (!(Test-Path '$(dir $@)')) { New-Item -ItemType Directory -Path '$(dir $@)' }"
	$(AS) $(ASFLAGS) -c $< -o $@

# Clean rule
.PHONY: clean
clean:
	$(RM) -r $(BUILD_DIR)

# Automatically include dependency files
-include $(OBJS:.o=.d)
