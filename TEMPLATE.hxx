#ifndef 	__XBYTES_PARSER_HPP__
#define 	__XBYTES_PARSER_HPP__

#include <stack>
#include <string>
#include <map>
#include <list>
#include <vector>
%%DECLARE_INCLUDE_CODE%%

using std::string;
typedef std::map<string, int> string_int_map;

/*
SYMBOL_COUNT 宏定义了整个语法文件所包含的符号总个数。
TERMINAL_SYMBOL 枚举了所有的终结符，最后定义的TERMINAL_SYMBOL_COUNT
是终结符的总数。这里的定义的终结符枚举类型要与词法分析器分析所得
的类型要保持一致。
*/
%%DECLARE_SYMBOL%%

namespace xbytes {

%%DECLARE_TOKEN_CODE%%

struct rule
{
	string r;
	int left;//left symbol index
	int right_count;//right symbol count
};

enum LR_ACTION
{
	LR_ACTION_GOTO = 1,
	LR_ACTION_SHIFT,
	LR_ACTION_REDUCE,
	LR_ACTION_ACCEPT,
	LR_ACTION_ERROR,
};
struct action
{
	int type;//action type from LR_ACTION
	int target;//goto target state or reduce rule index
};

struct stack_state
{
	int state;//current state
	int symbol;//current symbol
	TOKEN_TYPE t;
};
typedef std::stack<stack_state> parser_stack;

typedef std::vector<TOKEN_TYPE> token_vector;

class parser
{
public: 
	parser();
	~parser();

	void eat(int tt, TOKEN_TYPE t);
	
private:
	void initialize();
	TOKEN_TYPE execute_reduce_code(int rule_index, token_vector& tv);

private:
	parser_stack xstack;
	string_int_map symbol_id_map;
};

}//namespace xbytes
#endif 		//__XBYTES_PARSER_HPP__