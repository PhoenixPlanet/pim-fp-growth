CXX = g++
CXXFLAGS = -O2 -std=c++17 -pthread
INCLUDES = -I.

SRCS = main.cpp db.cpp fpgrowth.cpp db_count_item_cpu.cpp mine_candidates_cpu.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = fpgrowth_cpu

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
