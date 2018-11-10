#ifndef 	__XBYTES_HPP__
#define		__XBYTES_HPP__

#include <vector>
#include <string>
#include <map>
#include <sstream>
using std::string;

#define	MAX_LINE_SIZE 1024

namespace xbytes {

#define	ACCEPT_TERMINAL	"$"
enum SYMBOL_TYPE
{
	ST_NONE = 0,
	ST_TERMINAL,
	ST_NONTERMINAL,
	ST_ACCEPT,
};

enum LEXICAL_STATE
{
	LS_START = 0,
	LS_LEFT_SYMBOL,
	LS_LEFT_ALIAS,
	LS_LEFT_WAIT_ARROW,
	LS_ARROW,
	LS_RIGHT_WARIT_SYMBOL,
	LS_RIGHT_SYMBOL,
	LS_RIGHT_ALIAS,
	LS_WAIT_RULE_CODE,
	LS_RULE_CODE,
	LS_CODE_DECLARE,
	LS_CODE,

	LS_ASSOCIATIVITY_DECLARE,
	LS_ASSOCIATIVITY,
};

struct rule;
struct symbol;
typedef std::vector<rule*> rule_array;
typedef std::vector<symbol*> symbol_array;
typedef std::vector<string> string_array;

enum ASSOCIATIVITY_TYPE
{
	AT_NONE = 0,
	AT_LEFT,
	AT_RIGHT
};

struct assoc_prec
{
	int associativity;
	string_array symbol_names;
};
typedef std::vector<assoc_prec> assoc_prec_array;

struct symbol
{
	int index;
	string name;
	string alias;
	int type;
	rule_array rules;

	bool is_nullable;
	symbol_array first_set;
	symbol_array follow_set;

	int precedence;
	int associativity;

	symbol() : type(ST_NONE) {}
};

struct rule
{
	int index;
	symbol* left;
	string left_alias;

	symbol_array rights;
	string_array right_alias;

	string code;
	string str_rule;

	symbol* precedence_symbol;

	rule() : left(NULL) {}
};

struct term
{
	rule* r;
	int dot;
	symbol_array forwards;
	bool is_completed;
};
typedef std::vector<term*> term_array;

enum LR_ACTION
{
	LR_ACTION_GOTO = 1,
	LR_ACTION_SHIFT,
	LR_ACTION_REDUCE,
	LR_ACTION_ACCEPT,
	LR_ACTION_ERROR,
	LR_ACTION_SSCONFLICT,
	LR_ACTION_SRCONFLICT,
	LR_ACTION_RRCONFLICT,
	LR_ACTION_RDRESOLVED,
	LR_ACTION_SHRESOLVED,
};
struct action
{
	int type;
	int next_state;

	rule* r;//规约动作的产生式

	symbol* lookahead;
};
typedef std::vector<action> action_array;
typedef std::vector<action_array> action_table;
typedef std::map<symbol*, int> goto_map;

struct lr_state
{
	int state;
	term_array terms;
	goto_map gotomap;
	action_table actions;
	bool is_completed;
};
typedef std::vector<lr_state*> state_array;

class xbytes
{
public:
	xbytes();
	~xbytes();

	void make_parser(const string& file_name);

private:
	void find_symbol_precedence();
	void find_rule_precedence();
	void find_nullable();
	void find_first_set();
	void find_follow_set();
	void find_lr_state();
	void make_action_table();

	//for lexical
	void lexical_rule(char cc);
	void make_symbol(rule& r, symbol& s);
	symbol* find_symbol(const string& name);
	void add_rule(rule& r);
	void make_string_rule(rule& r);
	void sort_symbol();
	void make_precedence_associativity();

	//for first_set follow_set
	int merge_symbol_set(symbol_array& s1, symbol_array& s2);
	bool is_symbol_in_set(symbol* s, symbol_array& sa);

	//for lr state
	lr_state make_first_state();
	int add_lr_state(lr_state& s);
	int add_lr_goto(lr_state& ls, symbol* s, int state);
	bool is_same_lr_state(lr_state& l, lr_state& r);
	bool is_merge_lr_state(lr_state& l, lr_state& r);
	int merge_lr_state(lr_state& l, lr_state& r);

	void closure(lr_state& s);
	lr_state lr_goto(lr_state& state, symbol* s);
	int add_term(lr_state& s, term& t);
	bool is_same_term(term& l, term& r);

	int resolve_conflict(action& x, action& y);

	//for error handle
	void error_on_line(const char* line, const string& error, int space);

	//for dump information
	void dump();
	void dump_symbol();
	void dump_rule();
	void dump_first_set();
	void dump_follow_set();
	void dump_lr_state();
	void dump_action_table();
	void dump_associativity();
	
	//output
	void output_declare_symbol(std::ofstream& ss);
	void output_symbol_table(std::ofstream& ss);
	void output_rule_table(std::ofstream& ss);
	void output_action_table(std::ofstream& ss);
	void output_reduce_code(std::ofstream& ss);

	//emit code
	void emit_hxx_file();
	void emit_cxx_file();

	void emit_action_table();
private:
	rule* start;
	symbol_array symbols;
	rule_array rules;
	state_array states;

	string include_code;
	string token_code;
	string syntax_error_code;

	//lex
	int state;
	symbol s;
	rule r;

	char line[MAX_LINE_SIZE];
	int char_index_in_line;
	int line_count;
	int bracket_count;//used in LS_RULE_CODE and LS_CODE state
	string declare_code_name;//内部定义代码名称
	string declare_code;//内部定义代码
	string declare_associativity_name;//结合方向以及优先级定义名称(left or right)
	string associativity_symbol_name;//定义结合方向的symbol

	assoc_prec_array assoc_precs;
};

}//namespace xbytes
#endif		//__XBYTES_HPP__
