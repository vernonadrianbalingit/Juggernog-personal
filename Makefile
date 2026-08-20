CXX = clang++
CXXFLAGS = -Wall -Wextra -Werror -O2 -std=c++20

# Common object files
COMMON_OBJS = frame.o crc32.o

all: server client test_frame

# Pattern rule: any .cpp -> .o
%.o: %.cpp frame.h crc32.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

server: server.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

client: client.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

test_frame: test_frame.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: test_frame
	./test_frame

clean:
	rm -f *.o server client test_frame

.PHONY: all clean test
