all:
	g++ -o XP -g main.cpp PARSER.cxx

test:
	./x simple_calculate.x
	make
	./XP
clean:
	rm ACTION.txt
	rm PARSER.*
	rm TEMPLATE.*
	rm x
	rm XP
