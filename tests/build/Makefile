CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -std=c11
CXXFLAGS = -Wall -Wextra -std=c++17
INCLUDES = -I../..

# Safety flags
ifdef ENABLE_ASAN
    CFLAGS += -fsanitize=address -fno-omit-frame-pointer
    CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address
endif

ifdef DEBUG
    CFLAGS += -g -DDEBUG
    CXXFLAGS += -g -DDEBUG
else
    CFLAGS += -O2 -DNDEBUG
    CXXFLAGS += -O2 -DNDEBUG
endif

# Kernel test objects
KERNEL_TEST_SRCS = $(wildcard ../unit/kernel/test_*.c)
KERNEL_TEST_OBJS = $(KERNEL_TEST_SRCS:../unit/kernel/%.c=kernel_%.o)

# FUSE test objects
FUSE_TEST_SRCS = $(wildcard ../unit/fuse/test_*.cpp)
FUSE_TEST_OBJS = $(FUSE_TEST_SRCS:../unit/fuse/%.cpp=fuse_%.o)

# Common test objects
COMMON_TEST_SRCS = $(wildcard ../unit/common/test_*.cpp)
COMMON_TEST_OBJS = $(COMMON_TEST_SRCS:../unit/common/%.cpp=common_%.o)

all: kernel_tests fuse_tests common_tests

kernel_tests: $(KERNEL_TEST_OBJS)

fuse_tests: $(FUSE_TEST_OBJS)

common_tests: $(COMMON_TEST_OBJS)

kernel_%.o: ../unit/kernel/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

fuse_%.o: ../unit/fuse/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

common_%.o: ../unit/common/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f *.o

.PHONY: all clean kernel_tests fuse_tests common_tests
