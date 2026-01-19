CXX = g++
CXXFLAGS = -std=c++17 -Iinclude
TARGET = aurorakv

SRC = \
src/main.cpp \
src/KVStore.cpp \
src/MemTable.cpp \
src/SSTable.cpp \
src/BloomFilter.cpp \
src/Compaction.cpp \
src/ConfigManager.cpp \
src/ManifestManager.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
