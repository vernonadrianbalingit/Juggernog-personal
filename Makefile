CXX = clang++
CXXFLAGS = -Wall -Wextra -Werror -O2 -std=c++20
LDFLAGS = -pthread

# Common object files
COMMON_OBJS = frame.o crc32.o reliable.o dedupe.o
HEADERS = frame.h crc32.h reliable.h dedupe.h

all: server client test_frame

# Pattern rule: any .cpp -> .o
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

server: server.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

client: client.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test_frame: test_frame.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test: test_frame
	./test_frame

clean:
	rm -f *.o server client test_frame

.PHONY: all clean test
