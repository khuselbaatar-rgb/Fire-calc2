CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra
TARGET   := calculator

all: $(TARGET)

$(TARGET): calculator.cpp
	$(CXX) $(CXXFLAGS) calculator.cpp -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
