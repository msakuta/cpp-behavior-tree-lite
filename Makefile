main: main.cc behavior_tree_lite.h
	g++ -lstdc++ -std=c++17 main.cc -o main

catchball: examples/catchball.cc behavior_tree_lite.h
	g++ -lstdc++ -std=c++17 $< -o $@
