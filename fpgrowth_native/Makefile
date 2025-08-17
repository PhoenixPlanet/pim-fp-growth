CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

# Source files
SRCS = main.cpp fpgrowth.cpp db.cpp
# Object files
OBJS = $(SRCS:.cpp=.o)

# Output executable
TARGET = fpgrowth

# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile .cpp to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(TARGET)