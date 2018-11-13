# xbytes
LALR(1)语法分析生成器

### 0.概述：
看了编译器龙书和虎书后，自己手动写了一个LALR(1)语法分析生成器。　
程序里面很多的算法也都是摘录自虎书，龙书虽然讲的很详细，但是真正动手写的时候还是虎书上面的算法给力点。程序相对来说比较简单，没有做任何优化，如果看过虎书和龙书，看懂代码难度不大。代码文件bytes.hpp和bytes.cpp中是主要的代码，TEMPLATE.hxx和TEMPLATE.cxx是语法分析生成器的模板文件。首先直接用make进行编译，然后进入到sample目录中，运行生成器程序文件，参数是语法说明文件。执行成功后会生成会文件PARSER.hxx和PARSER.cxx，这两个文件就是你需要的语法分析器了。下面是个简单的实例说明下。

### 1.语法说明文件：
这里用一个简单的计算器来说明语法说明文件的用法。下面是计算器的语法说明文件。

```
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
```

终结符：终结符的名称只能由大写字母组成，在生成PARSER.hxx文件中会包括所有终结符的枚举定义。词法分析器的分析结果要和这里定义的枚举值一致。

　　非终结符：非终结符由小写字母、下划线组成，非终结符只存在于生成语法分析器的过程中。生成的语法分析器不会包括非终结符。

　　%include：这个说明符指定了生成的语法分析程序中要包含的头文件，这个指示符的格式是后面必须用大括号。如果有多个头文件可以用回车。

　　%token：这个是token结构的指示符，必须在大括号中指定，目前只支持内建的数据类型。

　　%syntax_error：语法分析过程中出现错误时，需要执行的代码。

　　#left：左结合指示符。同时会指定优先级，越往后面的优先级越高。

　　#right：右结合指示符。同left一样会指定优先级。

　　program：是语法开始指示符，语法说明文件必须指定program生成式，否则会报错。

　　BNF范式（产生式）：每个产生式必须以非终结符开始，以 . 符号结束。产生式中的每个非终结符都可以起别名，方便在语义代码中使用，别名必须紧跟在非终结符后面，而且要括在小括号中。需要注意的是xbytes不支持，一行多个产生式，因此每行只能写一个产生式。

　　语义代码：每个产生式的后面可以在大括号中指定产生式的语义代码。这个大括号要放到产生式最后的 . 点前面。语义代码只要是C++或者C代码就可以，没有其他限制。

　　语法说明文件名：因为我写的语法分析生成器的名字叫xbytes，所以我把语法说明文件的后缀名指定为.x。比如上面计算器的语法说明文件名：calculate.x 。当然这个文件的后缀名是可以随便起的，即使没有也没有关系。

　　ACTION.txt：在生成语法分析器的同时，会生成一个名为ACTION.txt的文件。文件中以很友好的方式将语法分析器的动作表打印出来了。可以帮助用户理解LALR(1)语法分析器的运作过程。

　　备注：在xbytes.cpp代码文件中，包含许多dump_开头的函数。这些函数可以输出很多生成分析器过程中的数据。包括Symbol集合、规则集合、First集、Follow集、状态集和动作表等。

### 2.语法分析器使用方式：

在根目录下直接输入 make 。编译xbytes，生成的可执行程序会被移入sample目录中，进入sample目录，然后执行./x calculate.x 就可以生成，简单计算器的语法分析程序了。使用这个程序的方式是自己写一个main.cpp文件，文件内容如下：

```
#include "PARSER.hxx"
#include <iostream>

int main()
{
    xbytes::parser p;

    //5 * 3 + 6 / 2 - 8
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
```

使用方式很简单，首先要自己写个词法分析器，来进行词法分析，然后将词法分析得到的token一个个的喂给parser就可以了。parser::eat函数的第一个参数是token的类型，第二个参数是token的值。读取结束后，最后写入0,就是结束分析。如果想简单看下运行效果，进入sample目录后直接make test就可以了。

### 3.运行结果：
这里计算的是算式 5 * 3 + 6 / 2 - 8 的值。打印的是规约的过程，具体要打印的信息可以自己在语法说明文件的语义代码中自己定制。
```
[kiven@localhost test]$ ./XP
5=5
3=3
15=5*3
6=6
2=2
3=6/2
18=15+3
8=8
10=18-8
Result=10
```

### 4.展望：
目前程序也仅仅只是能够生成语法分析器，但是性能不是很好，实用性也不是很高。后续要优化下程序性能，token要支持自定义结构。
