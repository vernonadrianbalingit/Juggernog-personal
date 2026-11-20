# Host tests: firmware logic with no hardware, runs on my mac and in CI
# Run with: make -C firmware/host_tests test
#
# Each test_*.c / test_*.cpp is its own little program that #includes the
# source file it tests, so adding a test never means touching this file

C = ../components
ROOT = ../..
INCLUDES = $(addprefix -I,$(wildcard $(C)/*/include)) -I$(ROOT)
CFLAGS = -Wall -Wextra -Werror -O1 -g -std=c11
CXXFLAGS = -Wall -Wextra -Werror -O1 -g -std=c++20

# Every test file -> build/<name>
TESTS = $(patsubst %.c,build/%,$(wildcard test_*.c)) \
	$(patsubst %.cpp,build/%,$(wildcard test_*.cpp))

# Run them all, stop at the first one that fails
test: $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done

# C tests
build/%: %.c check.h FORCE
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $<

# C++ tests
build/%: %.cpp check.h FORCE
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

clean:
	rm -rf build

# FORCE: always rebuild, since a test can #include any source file
FORCE:
.PHONY: test clean FORCE
