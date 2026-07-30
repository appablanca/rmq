fast: build run plot
thick: build bench plot
    
build:  
	g++ -std=c++17 -O3 -march=native -funroll-loops rmq-cpp/*.cpp -o rmq

bench:
    ./benchmark.sh

run:
    ./rmq input > data.csv

plot:
    python3 plot.py




generate_input:
    cd input-generator && cargo run -- -n 1000,3000,10000,30000,100000,300000,1000000,3000000 --output ../input

