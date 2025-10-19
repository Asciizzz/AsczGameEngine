# Simple Makefile for Lua Demo
# Note: You'll need Lua development libraries installed

CXX = g++
CXXFLAGS = -std=c++17 -Wall
LIBS = -llua
TARGET = lua_demo
SOURCE = main.cpp

# Default target
$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) $(LIBS) -o $(TARGET)

# Clean target
clean:
	rm -f $(TARGET)

# Run target
run: $(TARGET)
	./$(TARGET)

.PHONY: clean run