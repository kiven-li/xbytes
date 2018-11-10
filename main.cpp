#include "xbytes.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		std::cout << "x-bytes: need rule file name." << std::endl;
		return 1;
	}

    xbytes::xbytes x;
    x.make_parser(argv[1]);

    return 0;
}
