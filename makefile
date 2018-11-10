all:
	g++ -o x -g main.cpp xbytes.cpp 
	mv x sample/
	cp TEMPLATE.* sample/

clean:
	rm sample/x
