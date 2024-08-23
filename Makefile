
SRCS = $(wildcard src/*.cpp)
ASRCS = $(wildcard src/*.S)
OBJS = $(SRCS:.cpp=.o) $(ASRCS:.S=.o)
DEPS = $(SRCS:.cpp=.d) $(ASRCS:.S=.d)

all: mpv-gui

mpv-gui: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

.S.o:
	$(CXX) $(CXXFLAGS) -x assembler-with-cpp -c $< -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $@

clean:
	rm -f src/*.o src/*.d

-include $(DEPS)
