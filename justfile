fast: build run plot
thick: build bench plot
    
build:  
	g++ -std=c++17 -O3 -march=native rmq-cpp/*.cpp -o rmq

bench:
    ./benchmark.sh

run:
    ./rmq input > data.csv

plot:
    python3 plot.py




