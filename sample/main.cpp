#include "PARSER.hxx"
#include <iostream>

int main()
{
    xbytes::parser p;

    p.eat(INT, 5);
    p.eat(TIMES, 0);
    p.eat(INT, 3);
    p.eat(PLUS, 0);
    p.eat(INT, 6);
    p.eat(DIV, 0);
    p.eat(INT, 2);
    p.eat(MINUS, 0);
    p.eat(INT, 8);
    p.eat(0, 0);

    return 0;
}

