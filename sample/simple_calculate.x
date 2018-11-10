%include { #include <iostream> }
%token { int }
%syntax_error { std::cout << "Error: Syntax error.\n" << std::endl;}
#left PLUS MINUS
#left TIMES DIV

program -> exp(A). { std::cout << "Result=" << A << std::endl; }

exp(A) -> exp(B) MINUS exp(C). { A = B - C; std::cout << A << "=" << B << "-" << C << std::endl; }
exp(A) -> exp(B) PLUS exp(C). { A = B + C; std::cout << A << "=" << B << "+" << C << std::endl; }
exp(A) -> exp(B) TIMES exp(C). { A = B * C; std::cout << A << "=" << B << "*" << C << std::endl; }
exp(A) -> exp(B) DIV exp(C). {
    if(C != 0)
    {
        A = B / C;
    }
    else
    {
        std::cout << "Divide by zero." << std::endl;
    }

    std::cout << A << "=" << B << "/" << C << std::endl;
}

exp(A) -> INT(B). { A = B; std::cout << A << "=" << B << std::endl;}

