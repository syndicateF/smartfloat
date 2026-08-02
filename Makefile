PLUGIN_NAME=smartfloat

CXX = g++
CXXFLAGS = -std=c++26 -shared -fPIC --no-gnu-unique -O2 -g $(shell pkg-config --cflags pixman-1 libdrm hyprland lua5.4)

all:
	$(CXX) $(CXXFLAGS) -o $(PLUGIN_NAME).so src/main.cpp

clean:
	rm -f $(PLUGIN_NAME).so
