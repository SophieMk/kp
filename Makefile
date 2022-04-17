JSONS = $(wildcard *.json)
OUTS = $(patsubst %.json, %.log, $(JSONS))

all: $(OUTS)

json.hpp:
	wget https://github.com/nlohmann/json/releases/download/v3.10.5/json.hpp

main: main.cpp json.hpp
	g++ -Wall $< -o $@

%.log: %.json main
	( ./main <$< 2>&1 || echo "[exit $$?]" ) | tee $@

unix-dag.zip: Makefile main.cpp json.hpp $(JSONS) $(OUTS) docs
	rm -f $@
	zip -r --exclude='*/.*' $@ $^
