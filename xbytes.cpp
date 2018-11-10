#include "xbytes.hpp"
#include <istream>
#include <fstream>
#include <cctype>
#include <cstdlib>
#include <cassert>
#include <set>

#include <iostream>
#include <iomanip>

namespace xbytes {

xbytes::xbytes()
: start(NULL),
  symbols(),
  rules(),
  states(),
  include_code(""),
  token_code(""),
  syntax_error_code(""),
  state(LS_START),
  char_index_in_line(0),
  line_count(0),
  bracket_count(0),
  declare_code_name(""),
  declare_code(""),
  declare_associativity_name(""),
  associativity_symbol_name(""),
  assoc_precs()
{
	for(int i = 0; i < MAX_LINE_SIZE; ++i) line[i] = 0;
}

xbytes::~xbytes()
{
	for(int i = 0; i < symbols.size(); ++i)
	{
		delete symbols[i];
	}
	for(int i = 0; i < rules.size(); ++i)
	{
		delete rules[i];
	}
	for(int i = 0; i < states.size(); ++i)
	{
		for(int j = 0; j < states[i]->terms.size(); ++j)
		{
			delete states[i]->terms[j];
		}
		delete states[i];
	}
}

void xbytes::make_parser(const string& file_name)
{
	int start_time = time(NULL);

	//open file
	std::ifstream ifs;
	ifs.open(file_name.c_str(), std::ifstream::in);

	//read file
	char cc;
	while(ifs.good())
	{
		ifs.get(cc);
		lexical_rule(cc);
	}
	
	//close file
	ifs.close();

	//check program file is complete
	if(bracket_count)
	{
		error_on_line(line, "expect }", char_index_in_line);
	}
	if(start == NULL)
	{
		std::cout << "ERROR: not found start symbol [program] ." << std::endl;
		exit(1);
	}
	if(token_code.size() == 0)
	{
		std::cout << "ERROR: not found token declare ." << std::endl;
		exit(1);		
	}

	//parse rule 
	sort_symbol();
	std::cout << "set precedence & associativity" << std::endl;
	find_symbol_precedence();
	find_rule_precedence();
	std::cout << "find_nullable" << std::endl;
	find_nullable();
	std::cout << "find_first_set" << std::endl;
	find_first_set();
	std::cout << "find_follow_set" << std::endl;
	find_follow_set();
	std::cout << "find_lr_state" << std::endl;
	find_lr_state();
	std::cout << "make_action_table" << std::endl;
	make_action_table();

	//dump
	//dump();

	//emit code
	emit_hxx_file();
	emit_cxx_file();

	emit_action_table();

	int eslape = time(NULL) - start_time;
	std::cout << "CONSULE:" << eslape << std::endl;
}

/*摘录自虎书
FIRST、FOLLOW、nullable计算算法：

将所有FIRST,FOLLOW,nullable设置为空
for 每一个终结符Z
	FIRST[Z] = {Z}

repeat
	for 每个产生式 X->Y1 Y2 ... Yk
		for 每个i从1到k 每个j从i+1到k
			if 所有Yi都可为空的 then
				nullable[X] = true
			if Y1 ... Yi-1都可为空的 then
				FIRST[X] = FIRST[X] U FIRST[Yi]
			if Yi+1 ... Yk 都可为空的 then
				FOLLOW[Yi] = FOLLOW[Yi] U FOLLOW[X]
			if Yi+1 .. Yj-1 都可为空的 then
				FOLLOW[Yi] = FOLLOW[Yi] U FIRST[Yj]
			if Yi+1是终结符并且Yi是非终结符
				FOLLOW[Yi] = FOLLOW[Yi] U Yi
			if Yi+1是非终结符并且Yi也是非终结符
				FOLLOW[Yi] = FOLLOW[Yi] U FIRST[Yi+1]
until FIRST,FOLLOW,nullable都没有变化
*/

void xbytes::find_nullable()
{
	for(int i = 0; i < symbols.size(); ++i)
	{
		symbols[i]->is_nullable = false;
	}

	while(true)
	{
		bool is_change = false;
		for(int i = 0; i < rules.size(); ++i)
		{
			rule* r = rules[i];
			if(r->left->is_nullable) continue;
			if(r->rights.size() == 0)
			{
				r->left->is_nullable = true;
				is_change = true;
				continue;
			}

			bool is_all_nullable = true;
			for(int j = 0; j < r->rights.size(); ++j)
			{
				if(!r->rights[j]->is_nullable)
				{
					is_all_nullable = false;
					break;
				}
			}

			if(is_all_nullable)
			{
				r->left->is_nullable = true;
				is_change = true;
			}
		}

		if(!is_change) break;
	}
}

void xbytes::find_first_set()
{
	for(int i = 0; i < symbols.size(); ++i)
	{
		if(symbols[i]->type == ST_TERMINAL)
		{
			symbols[i]->first_set.push_back(symbols[i]);
		}
	}

	while(true)
	{
		int change_count = 0;
		for(int i = 0; i < rules.size(); ++i)
		{
			rule* r = rules[i];
			if(r->rights.size() == 0) continue;

			change_count += merge_symbol_set(r->left->first_set, r->rights[0]->first_set);
			for(int j = 0; j < r->rights.size() - 1; ++j)
			{
				if(!r->rights[j]->is_nullable) break;
				change_count += merge_symbol_set(r->left->first_set, r->rights[j+1]->first_set);
			}
		}

		if(!change_count) break;
	}
}

int xbytes::merge_symbol_set(symbol_array& s1, symbol_array& s2)
{
	int insert_count = 0;
	for(int i = 0; i < s2.size(); ++i)
	{
		bool is_add = true;
		for(int j = 0; j < s1.size(); ++j)
		{
			if(s2[i] == s1[j])
			{
				is_add = false;
				break;
			}
		}
		if(is_add)
		{
			insert_count++;
			s1.push_back(s2[i]);
		}
	}

	return insert_count;
}

bool xbytes::is_symbol_in_set(symbol* s, symbol_array& sa)
{
	for(int i = 0; i < sa.size(); ++i)
	{
		if(sa[i] == s) return true;
	}

	return false;
}

void xbytes::find_follow_set()
{
	for(int i = 0; i < symbols.size(); ++i)
	{
		if(symbols[i]->type == ST_TERMINAL)
		{
			symbols[i]->follow_set.push_back(symbols[i]);
		}
	}

	while(true)
	{
		int change_count = 0;
		for(int x = 0; x < rules.size(); ++x)
		{
			rule* r = rules[x];
			int k = r->rights.size();
			if(k == 0) continue;

			for(int i = 0; i < k; ++i)
			{
				symbol* s = r->rights[i];
				if(s->type == ST_TERMINAL) 
				{
					//终结符前面是非终结符，则加入到前面的FOLLOW中
					if(i > 0 && r->rights[i-1]->type == ST_NONTERMINAL)
					{
						change_count += merge_symbol_set(r->rights[i-1]->follow_set, s->follow_set);
					}						
					continue;
				}

				//两个相邻的非终结符，后一个的FIRST是前一个的FOLLOW
				if(i > 0 && r->rights[i-1]->type == ST_NONTERMINAL)
				{
					change_count += merge_symbol_set(r->rights[i-1]->follow_set, s->first_set);
				}

				//if Yi+1 ... Yk 都可为空的 then
				//FOLLOW[Yi] = FOLLOW[Yi] U FOLLOW[X]
				bool is_all_follow_nullable = true;
				for(int j = i + 1; j < k; ++j)
				{
					if(!r->rights[j]->is_nullable)
					{
						is_all_follow_nullable = false;
						break;
					}
				}
				if(is_all_follow_nullable)
				{
					change_count += merge_symbol_set(s->follow_set, r->left->follow_set);
				}

				//if Yi+1 .. Yj-1 都可为空的 then
				//FOLLOW[Yi] = FOLLOW[Yi] U FIRST[Yj]
				for(int j = i + 1; j < k; ++j)
				{
					if(i+1 > j-1) continue;
					if(!r->rights[j-1]->is_nullable) break;
					change_count += merge_symbol_set(s->follow_set, r->rights[j]->first_set);
				}
			}
		}

		if(!change_count) break;
	}
}

/*摘录自虎书
初始化T为{Closure({S1 -> . S$})}
初始化E为空
repeat
    for T中的每一个状态I
        for I中的每一项A->a.Xb
            let J是Goto(I, X)
            T <- T U {J}
            E <- E U { I -X-> J}
until E和T在本轮中没有改变
*/
void xbytes::find_lr_state()
{
	//set first state
	make_first_state();

	//set follow state and goto
	while(true)
	{
		int change_count = 0;

		for(int i = 0; i < states.size(); ++i)
		{
			lr_state* ls = states[i];
			if(ls->is_completed) continue;
			for(int j = 0; j < ls->terms.size(); ++j)
			{
				term* t = ls->terms[j];
				if(t->dot >= t->r->rights.size()) continue;

				symbol* s = t->r->rights[t->dot];
				if(s->name == ACCEPT_TERMINAL) continue;

				lr_state ns = lr_goto(*ls, s);
				change_count += add_lr_state(ns);
				change_count += add_lr_goto(*ls, s, ns.state);		
			}
			if(!change_count) ls->is_completed = true;
		}

		if(!change_count) break;
	}
}

lr_state xbytes::make_first_state()
{
	lr_state start_state;
	start_state.state = 0;

	term* t = new term();
	t->r = start;
	t->dot = 0;

	start_state.terms.push_back(t);

	closure(start_state);

	add_lr_state(start_state);

	return start_state;
}

//只有向前看符号不一样，但是项集一样的状态进行合并。
int xbytes::add_lr_state(lr_state& s)
{
	for(int i = 0; i < states.size(); ++i)
	{
		lr_state& ls = *(states[i]);
		if(is_merge_lr_state(ls, s))
		{
			s.state = states[i]->state;
			if(is_same_lr_state(ls, s)) return 0;
			return merge_lr_state(ls, s);
		}
	}

	s.state = states.size();

	lr_state* ls = new lr_state();
	*ls = s;
	ls->is_completed = false;
	states.push_back(ls);

	return 1;
}

int xbytes::add_lr_goto(lr_state& ls, symbol* s, int state)
{
	if(ls.gotomap.count(s) > 0) return 0;

	ls.gotomap[s] = state;
	return state;
}

bool xbytes::is_same_lr_state(lr_state& l, lr_state& r)
{
	if(l.terms.size() != r.terms.size()) return false;
	
	for(int i = 0; i < l.terms.size(); ++i)
	{
		if(!is_same_term(*(l.terms[i]), *(r.terms[i]))) return false;
	}

	return true;
}

bool xbytes::is_merge_lr_state(lr_state& l, lr_state& r)
{
	if(l.terms.size() != r.terms.size()) return false;

	for(int i = 0; i < l.terms.size(); ++i)
	{
		term* lt = l.terms[i];
		term* rt = r.terms[i];

		if(lt->r != rt->r || lt->dot != rt->dot) return false;
	}

	return true;
}

int xbytes::merge_lr_state(lr_state& l, lr_state& r)
{
	int cc = 0;
	for(int i = 0; i < l.terms.size(); ++i)
	{
		term* lt = l.terms[i];
		term* rt = r.terms[i];

		cc += merge_symbol_set(lt->forwards, rt->forwards);
	}

	return cc;
}

/*
Closure(I) = 
repeat
    for I中的任意项(A->a.Xb, z)
        for 任意产生式X->y
            for 任意w in FIRST(bz)
                I <- I U {(X -> .y, w)}
until I 没有改变
return I
*/   
void xbytes::closure(lr_state& s)
{
	while(true)
	{
		int change_count = 0;
		for(int i = 0; i < s.terms.size(); ++i)
		{
			term* t = s.terms[i];
			if(t->is_completed) continue;
			if(t->dot >= t->r->rights.size()) continue;
			if(t->r->rights[t->dot]->type == ST_TERMINAL) continue;

			symbol* ss = t->r->rights[t->dot];
			for(int j = 0; j < ss->rules.size(); ++j)
			{				
				term tt;
				tt.r = ss->rules[j];
				tt.dot = 0;

				if(t->forwards.size() != 0)
				{
					for(int m = 0; m < t->forwards.size(); ++m)
					{
						for(int n = 0; n < t->forwards[m]->first_set.size(); ++n)
						{
							tt.forwards.push_back(t->forwards[m]->first_set[n]);
							change_count += add_term(s, tt);
							tt.forwards.clear();
						}
					}
				}
				if(t->dot < t->r->rights.size() - 1)
				{
					for(int m = 0; m < t->r->rights[t->dot+1]->first_set.size(); ++m)
					{
						tt.forwards.push_back(t->r->rights[t->dot+1]->first_set[m]);
						change_count += add_term(s, tt);
						tt.forwards.clear();
					}
				}
			}

			if(!change_count) t->is_completed = true;
		}

		if(!change_count) break;
	}
}

/*
Goto(I,X) =
    J <- {}
    for I中的任意项(A -> a.Xb, z)
        将(A->aX.b, z)加入到J中
    return Closure(J)
*/    
lr_state xbytes::lr_goto(lr_state& state, symbol* s)
{
	lr_state next_state;
	for(int i = 0; i < state.terms.size(); ++i)
	{
		term* t = state.terms[i];
		assert(t->r != NULL);
		if(t->dot >= t->r->rights.size()) continue;
		if(t->r->rights[t->dot] == s)
		{
			term tt;
			tt.r = t->r;
			tt.dot = t->dot + 1;
			tt.forwards = t->forwards;
			add_term(next_state, tt);
		}
	}

	closure(next_state);
	return next_state;
}

//添加LALR(1)项时，如果只有向前看符号不一样则合并到已经存在的项中。
int xbytes::add_term(lr_state& s, term& tt)
{	
	for(int i = 0; i < s.terms.size(); ++i)
	{
		//完全一样则返回
		term& t = *(s.terms[i]);
		if(is_same_term(t, tt)) return 0;

		//如果是向前看符号集不一样则合并向前看符号集
		if(t.r == tt.r && t.dot == tt.dot)
		{
			return merge_symbol_set(t.forwards, tt.forwards);
		}
	}

	term* t = new term();
	*t = tt;
	t->is_completed = false;
	s.terms.push_back(t);
	return 1;
}

bool xbytes::is_same_term(term& l, term& r)
{
	if(l.r != r.r || l.dot != r.dot || l.forwards.size() != r.forwards.size()) return false;
	for(int i = 0; i < l.forwards.size(); ++i)
	{
		if(l.forwards[i] != r.forwards[i]) return false;
	}
	return true;
}

/*
R <- {}
for T中的每一个状态I
    for I中的每一个项(A->a. , z)
        R <= R U {(I,z,A->a)}

对于每一条边I-X->J, 若X为终结符，则在位置(I,X)中放移进J(sJ)；
若X为非终结符，则将转换J(gJ)放在位置(I,X)中。
对于包含项S1->S.$的每个状态I,我们再位置(I,$)中放置接受(a).
最后，对于包含项A->y.(尾部有圆点的产生式n)的状态，
对每一个单词Y, 放置规约n(rn)于(I,Y)中。
*/
void xbytes::make_action_table()
{
	for(int i = 0; i < states.size(); ++i)
	{
		lr_state* s = states[i];
		s->actions.resize(symbols.size());

		//goto & shift action
		for(goto_map::iterator it = s->gotomap.begin();it != s->gotomap.end(); ++it)
		{
			symbol* ss = it->first;

			action act;
			act.next_state = it->second;
			act.r = NULL;
			act.lookahead = ss;

			if(ss->type == ST_TERMINAL)
			{
				act.type = LR_ACTION_SHIFT;				
			}
			else if(ss->type == ST_NONTERMINAL)
			{
				act.type = LR_ACTION_GOTO;
			}

			s->actions[ss->index].push_back(act);
		}

		//reduce action
		for(int m = 0; m < s->terms.size(); ++m)
		{
			term* t = s->terms[m];
			if(t->dot != t->r->rights.size()) continue;

			for(int n = 0; n < s->actions.size(); ++n)
			{
				if(symbols[n]->type != ST_TERMINAL) continue;
				if(!is_symbol_in_set(symbols[n], t->forwards)) continue;//FOR LALR
				//if(!is_symbol_in_set(symbols[m], t->r->left->follow_set)) continue; //for SLR

				action act;
				act.type = LR_ACTION_REDUCE;
				act.next_state = 0;
				act.r = t->r;
				act.lookahead = symbols[n];

				s->actions[n].push_back(act);
			}
		}

		//accept action
		for(int m = 0; m < s->terms.size(); ++m)
		{
			term* t = s->terms[m];
			if(t->dot != t->r->rights.size() - 1) continue;

			if(t->r->rights[t->dot]->name == ACCEPT_TERMINAL)
			{
				action act;
				act.r = t->r;
				act.type = LR_ACTION_ACCEPT;
				act.next_state = 0;
				act.lookahead = t->r->rights[t->dot];

				s->actions[t->r->rights[t->dot]->index].push_back(act);
			}
		}
	}

	//error action
	for(int i = 0; i < states.size(); ++i)
	{
		lr_state* s = states[i];
		for(int m = 0; m < s->actions.size(); ++m)
		{
			action_array& aa = s->actions[m];
			if(aa.size() == 0)
			{
				action act;
				act.r = NULL;
				act.type = LR_ACTION_ERROR;
				act.next_state = 0;
				act.lookahead = NULL;

				aa.push_back(act);
			}
		}
	}

	//resolve conflict
	int conflict = 0;
	for(int i = 0; i < states.size(); ++i)
	{
		lr_state* s = states[i];
		for(int m = 0; m < s->actions.size(); ++m)
		{
			action_array& aa = s->actions[m];
			if(aa.size() < 2) continue;

			for(int n = 1; n < aa.size(); ++n)
			{
				conflict += resolve_conflict(aa[n-1], aa[n]);
			}
		}
	}

	std::cout << "conflict: " << conflict << std::endl;
}

int xbytes::resolve_conflict(action& x, action& y)
{
	int error = 0;
	assert(x.lookahead == y.lookahead);

	if(x.type == LR_ACTION_SHIFT && y.type == LR_ACTION_SHIFT)
	{
		y.type = LR_ACTION_SSCONFLICT;
		error++;
	}

	if(x.type == LR_ACTION_SHIFT && y.type == LR_ACTION_REDUCE)
	{
		symbol* xs = x.lookahead;
		symbol* ys = y.r->precedence_symbol;

		if(ys == NULL || xs->precedence < 0 || ys->precedence < 0)
		{
			//Not enough precedence information
			y.type = LR_ACTION_SRCONFLICT;
			error++;
		}
		else if(xs->precedence > ys->precedence)
		{
			y.type = LR_ACTION_RDRESOLVED;
		}
		else if(xs->precedence < ys->precedence)
		{
			x.type = LR_ACTION_SHRESOLVED;
		}
		else if(xs->precedence == ys->precedence && xs->associativity == AT_RIGHT)//Use operator
		{
			y.type = LR_ACTION_RDRESOLVED;//associativity
		}
		else if(xs->precedence == ys->precedence && xs->associativity == AT_LEFT)//to break tie
		{
			x.type = LR_ACTION_SHRESOLVED;
		}
		else
		{
			assert(xs->precedence == ys->precedence && xs->associativity == AT_NONE);
			x.type = LR_ACTION_ERROR;
		}
	}
	else if(x.type = LR_ACTION_REDUCE && y.type == LR_ACTION_REDUCE)
	{
		symbol* xs = x.r->precedence_symbol;
		symbol* ys = y.r->precedence_symbol;

		if(xs == NULL || ys == NULL || xs->precedence < 0 || 
			ys->precedence < 0 || xs->precedence == ys->precedence)
		{
			y.type = LR_ACTION_RRCONFLICT;
			error++;
		}
		else if(xs->precedence > ys->precedence)
		{
			y.type = LR_ACTION_RDRESOLVED;
		}
		else if(xs->precedence < ys->precedence)
		{
			x.type = LR_ACTION_RDRESOLVED;
		}
	}
	else
	{
		assert(
			x.type == LR_ACTION_SHRESOLVED ||
			x.type == LR_ACTION_RDRESOLVED ||
			x.type == LR_ACTION_SSCONFLICT ||
			x.type == LR_ACTION_SRCONFLICT ||
			x.type == LR_ACTION_RRCONFLICT ||
			y.type == LR_ACTION_SHRESOLVED ||
			y.type == LR_ACTION_RDRESOLVED ||
			y.type == LR_ACTION_SSCONFLICT ||
			y.type == LR_ACTION_SRCONFLICT ||
			y.type == LR_ACTION_RRCONFLICT
			);
	}

	return error;
}

void xbytes::lexical_rule(char cc)
{
	line[char_index_in_line++] = cc;

	if(cc == '\0') return;

	switch(state)
	{
	case LS_START:
	{
		if(isalpha(cc))
		{
			s.name.push_back(cc);
			state = LS_LEFT_SYMBOL;
		}
		else if(cc == '%')
		{
			state = LS_CODE_DECLARE;
		}
		else if(cc == '#')
		{
			assoc_prec ap;
			ap.associativity = AT_NONE;
			assoc_precs.push_back(ap);
			state = LS_ASSOCIATIVITY_DECLARE;
		}
		else if(cc != ' ' && cc != '\n' && cc != '\r')
		{
			error_on_line(line, "expect char/SPACE", char_index_in_line);
		}	
		break;
	}
	case LS_CODE_DECLARE:
	{
		if(isalpha(cc) || cc == '_')
		{
			declare_code_name.push_back(cc);
		}
		else if(cc == '{')
		{
			++bracket_count;
			state = LS_CODE;
		}
		else if(cc != ' ' && cc != '\r' && cc != '\n')
		{
			error_on_line(line, "expect char OR { OR \\r OR \\n", char_index_in_line);
		}
		break;
	}
	case LS_CODE:
	{
		if(cc == '}')
		{
			--bracket_count;
			if(bracket_count == 0)
			{
				if(declare_code_name == "include")
				{
					include_code = declare_code;
				}
				else if(declare_code_name == "token")
				{
					token_code = declare_code;
				}
				else if(declare_code_name == "syntax_error")
				{
					syntax_error_code = declare_code;
				}

				declare_code.clear();
				declare_code_name.clear();

				state = LS_START;				
			}
			else
			{
				declare_code.push_back(cc);
			}
		}
		else if(cc == '{')
		{
			++bracket_count;
			declare_code.push_back(cc);
		}
		else
		{
			declare_code.push_back(cc);
		}
		break;
	}
	case LS_ASSOCIATIVITY_DECLARE:
	{
		if(isalpha(cc))
		{
			declare_associativity_name.push_back(cc);
		}
		else if(cc == ' ')
		{
			int ass = declare_associativity_name == "left" ? AT_LEFT : AT_RIGHT;
			assoc_precs[assoc_precs.size() - 1].associativity = ass;
			state = LS_ASSOCIATIVITY;
		}
		else 
		{
			error_on_line(line, "expect char OR space", char_index_in_line);
		}
		break;
	}
	case LS_ASSOCIATIVITY:
	{
		if(isalpha(cc))
		{
			associativity_symbol_name.push_back(cc);
		}
		else if(cc == ' ')
		{
			if(associativity_symbol_name.size() != 0)
			{
				assoc_precs[assoc_precs.size() - 1].symbol_names.push_back(associativity_symbol_name);
				associativity_symbol_name.clear();
			}
		}
		else if(cc == '\r' || cc == '\n')
		{
			if(associativity_symbol_name.size() != 0)
			{
				assoc_precs[assoc_precs.size() - 1].symbol_names.push_back(associativity_symbol_name); 
				associativity_symbol_name.clear();
			}
			declare_associativity_name.clear();
			state = LS_START;
		}
		else
		{
			error_on_line(line, "expect char, space, \\r, OR \\n", char_index_in_line);
		}
		break;
	}
	case LS_LEFT_SYMBOL:
	{
		if(isalpha(cc) || cc == '_' || isdigit(cc) || cc == '-')
		{
			s.name.push_back(cc);
		}
		else if(cc == '(')
		{
			state = LS_LEFT_ALIAS;
		}
		else if(cc == '-')
		{
			state = LS_ARROW;
		}
		else if(cc == ' ')
		{
			state = LS_LEFT_WAIT_ARROW;
		}
		else
		{
			error_on_line(line, "expect char/(/-/SPACE", char_index_in_line);
		}

		break;
	}
	case LS_LEFT_ALIAS:
	{
		if(isalpha(cc))
		{
			s.alias.push_back(cc);
		}
		else if(cc == ')')
		{
			state = LS_LEFT_WAIT_ARROW;
		}
		else
		{
			error_on_line(line, "expect char or )", char_index_in_line);
		}

		break;
	}
	case LS_LEFT_WAIT_ARROW:
	{
		if(cc == '-')
		{
			state = LS_ARROW;				
		}
		else if(cc != ' ')
		{
			error_on_line(line, "expect -", char_index_in_line);
		}

		break;
	}
	case LS_ARROW:
	{
		if(cc == '>')
		{
			state = LS_RIGHT_WARIT_SYMBOL;
			make_symbol(r, s);			
		}
		else
		{
			error_on_line(line, "expect >", char_index_in_line);
		}

		break;
	}
	case LS_RIGHT_WARIT_SYMBOL:
	{
		if(cc == '.')
		{
			state = LS_WAIT_RULE_CODE;
		}
		else if(isalpha(cc))
		{
			state = LS_RIGHT_SYMBOL;
			s.name.push_back(cc);
		}
		else if(cc != ' ')
		{
			error_on_line(line, "expect char/./SPACE", char_index_in_line);
		}

		break;
	}
	case LS_RIGHT_SYMBOL:
	{
		if(cc == '.')
		{
			state = LS_WAIT_RULE_CODE;
			make_symbol(r, s);
		}
		else if(isalpha(cc) || cc == '_' || isdigit(cc) || cc == '-')
		{
			s.name.push_back(cc);
		}
		else if(cc == '(')
		{
			state = LS_RIGHT_ALIAS;
		}
		else if(cc == ' ')
		{
			state = LS_RIGHT_WARIT_SYMBOL;
			make_symbol(r, s);
		}
		else
		{
			error_on_line(line, "expect char/./(/SPACE", char_index_in_line);
		}
		break;
	}
	case LS_RIGHT_ALIAS:
	{
		if(isalpha(cc))
		{
			s.alias.push_back(cc);
		}
		else if(cc == ')')
		{
			state = LS_RIGHT_WARIT_SYMBOL;
			make_symbol(r, s);
		}
		else
		{
			error_on_line(line, "expect ) or char", char_index_in_line);
		}
		break;
	}
	case LS_WAIT_RULE_CODE:
	{
		if(cc == '{')
		{
			bracket_count++;
			r.code.push_back(cc);
			state = LS_RULE_CODE;
		}
		else if(isalpha(cc))
		{
			add_rule(r);
			s.name.push_back(cc);
			state = LS_LEFT_SYMBOL;
		}
		else if(cc != ' ' && cc != '\r' && cc != '\n')
		{
			error_on_line(line, "expect \r\n or { or SPACE", char_index_in_line);
		}
		break;
	}
	case LS_RULE_CODE:
	{
		if(cc == '}')
		{
			r.code.push_back(cc);
			
			if(--bracket_count == 0) 
			{
				add_rule(r);
				state = LS_START;
			}
		}
		else
		{
			r.code.push_back(cc);
			if(cc == '{') bracket_count++;
		}
		break;
	}
	}

	if(cc == '\n')
	{
		for(int m = 0; m < MAX_LINE_SIZE; ++m) line[m] = 0;
		if(char_index_in_line) line_count++;
		char_index_in_line = 0;
	}
}

void xbytes::make_symbol(rule& r, symbol& s)
{
	if(s.name.size() == 0) return;

	//add to symbol table
	symbol* ss = find_symbol(s.name);
	if(ss == NULL)
	{
		ss = new symbol();
		ss->index = symbols.size();
		ss->name = s.name;
		ss->alias = s.alias;
		ss->type = ss->name[0] >= 'a' && ss->name[0] <= 'z' ? ST_NONTERMINAL : ST_TERMINAL;
		ss->precedence = -1;
		ss->associativity = AT_NONE;
		symbols.push_back(ss);
	}
	assert(ss != NULL);

	//attach to rule
	if(r.left == NULL)
	{
		r.left = ss;
		r.left_alias = s.alias;
	}
	else
	{
		r.rights.push_back(ss);
		r.right_alias.push_back(s.alias);
	}

	//clear symbol
	s.name.clear();
	s.alias.clear();
}

void xbytes::add_rule(rule& r)
{
	//create new rule
	rule* rr = new rule();
	rr->index = rules.size();
	rr->left = r.left;
	rr->left_alias = r.left_alias;
	rr->rights = r.rights;
	rr->right_alias = r.right_alias;
	rr->code = r.code;
	make_string_rule(*rr);
	rules.push_back(rr);

	//attach rule with left symbol
	rr->left->rules.push_back(rr);

	//set start rule
	if(rr->left->name == "program")
	{
		start = rr;

		symbol* s = new symbol();
		s->index = symbols.size();
		s->name = ACCEPT_TERMINAL;
		s->type = ST_TERMINAL;
		s->associativity = AT_NONE;
		s->precedence = -1;
		symbols.push_back(s);
		rr->rights.push_back(s);
		rr->right_alias.push_back("");
	}
	
	//clear global rule
	r.left = NULL;
	r.left_alias.clear();
	r.rights.clear();	
	r.right_alias.clear();
	r.code.clear();
}

void xbytes::make_string_rule(rule& r)
{
	r.str_rule.clear();

	r.str_rule = r.left->name;
	if(r.left_alias.size() > 0)
	{
		r.str_rule += "(" + r.left_alias + ")";
	}
	r.str_rule += " ->";
	
	assert(r.rights.size() == r.right_alias.size());

	for(int j = 0; j < r.rights.size(); ++j)
	{
		r.str_rule += " " + r.rights[j]->name;
		if(r.right_alias[j].size() > 0)
		{
			r.str_rule += "(" + r.right_alias[j] + ")";
		}
	}
	r.str_rule += ".";
}

symbol* xbytes::find_symbol(const string& name)
{
	for(int i = 0; i < symbols.size(); ++i)
	{
		if(symbols[i]->name == name)
		{
			return symbols[i];
		}
	}

	return NULL;
}

void xbytes::sort_symbol()
{
	symbol_array terminals;
	symbol_array nonterminals;

	for(int i = 0; i < symbols.size(); ++i)
	{
		symbol* s = symbols[i];
		if(s->type == ST_TERMINAL) 
			terminals.push_back(s);
		else
			nonterminals.push_back(s);
	}
	for(int i = 0; i < terminals.size(); ++i)
	{
		symbols[i] = terminals[i];
		symbols[i]->index = i;
	}
	for(int i = 0; i < nonterminals.size(); ++i)
	{
		int index = terminals.size() + i;
		symbols[index] = nonterminals[i];
		symbols[index]->index = index;
	}
}

void xbytes::find_symbol_precedence()
{
	//make symbol's precedence & associativity
	for(int i = 0; i < assoc_precs.size(); ++i)
	{
		int ass = assoc_precs[i].associativity;
		string_array& sa = assoc_precs[i].symbol_names;
		for(int m = 0; m < sa.size(); ++m)
		{
			symbol* s = find_symbol(sa[m]);
			if(s)
			{
				s->associativity = ass;
				s->precedence = i;
			}
		}
	}	
}
void xbytes::find_rule_precedence()
{


	//make rule's precedence
	for(int i = 0; i < rules.size(); ++i)
	{
		rule* r = rules[i];
		if(r->precedence_symbol != NULL) continue;

		for(int m = 0; m < r->rights.size() && r->precedence_symbol == NULL; ++m)
		{
			symbol* s = r->rights[m];
			if(s->type != ST_TERMINAL) continue;
			
			if(s->precedence >= 0)
			{
				r->precedence_symbol = s;
				break;
			}
		}
	}
}

void xbytes::error_on_line(const char* line, const string& error, int space)
{
	std::cout << "ERROR:" << std::endl;
	std::cout << "  line: " << line_count+1 << std::endl;
	std::cout << "  info: Don't parse program file, There is an error:[[" << std::endl;
	std::cout << line << std::endl;
	for(int i = 0; i < space; ++i)
	{
		std::cout << ' ';
	}
	std::cout << "^" << std::endl;
	std::cout << "  " << error << "]]" << std::endl;

	exit(1);
}

void xbytes::dump()
{
	dump_symbol();
	dump_rule();
	dump_first_set();
	dump_follow_set();
	dump_lr_state();
	dump_action_table();
	dump_associativity();
}

void xbytes::dump_symbol()
{
	std::cout << "==============SYMBOL============" << std::endl;
	std::cout << "COUNT	SYMBOL 		TYPE 	RCOUNT	NULL 	ASSOC 	PRECE" << std::endl;
	std::cout << "----------------------------" << std::endl;
	for(int i = 0; i < symbols.size(); ++i)
	{
		string type = symbols[i]->type == ST_TERMINAL ? "T" : "NT";
		string assoc = symbols[i]->associativity == AT_LEFT ? "L" : symbols[i]->associativity == AT_RIGHT ? "R" : "N";
		std::cout << i << "	" << symbols[i]->name << "		" << type << "	" << symbols[i]->rules.size() << "	" << symbols[i]->is_nullable << "	" << assoc << "	" << symbols[i]->precedence << std::endl;
	}
	std::cout << std::endl << std::endl;
}

void xbytes::dump_rule()
{
	std::cout << "==============RULE============" << std::endl;
	for(int i = 0; i < rules.size(); ++i)
	{	
		std::cout << i << ": " << rules[i]->str_rule;

		if(rules[i]->code.size() > 0)
		{
			std::cout << "	[" << rules[i]->code << "].";
		}
		std::cout << std::endl;
	}
}

void xbytes::dump_first_set()
{
	std::cout << "==============FIRST SET============" << std::endl;
	for(int i = 0; i < symbols.size(); ++i)
	{
		symbol* s = symbols[i];
		if(s->type == ST_TERMINAL) continue;

		std::cout << s->name << "[";
		for(int j = 0; j < s->first_set.size(); ++j)
		{
			std::cout << s->first_set[j]->name << ",";
		}
		std::cout << "]" << std::endl;
	}
	std::cout << std::endl << std::endl;	
}

void xbytes::dump_follow_set()
{
	std::cout << "==============FOLLOW SET============" << std::endl;
	for(int i = 0; i < symbols.size(); ++i)
	{
		symbol* s = symbols[i];
		if(s->type == ST_TERMINAL) continue;

		std::cout << s->name << "[";
		for(int j = 0; j < s->follow_set.size(); ++j)
		{
			std::cout << s->follow_set[j]->name << ",";
		}
		std::cout << "]" << std::endl;
	}
	std::cout << std::endl << std::endl;	
}

void xbytes::dump_lr_state()
{
	std::cout << "==============LR STATE============" << std::endl;
	for(int i = 0; i < states.size(); ++i)
	{
		lr_state* ls = states[i];
		std::cout << "STATE " << i << ":" << std::endl;
		for(int j = 0; j < ls->terms.size(); ++j)
		{
			rule* r = ls->terms[j]->r;
			int dot = ls->terms[j]->dot;
			string forwards = "";
			for(int m = 0; m < ls->terms[j]->forwards.size(); ++m)
			{
				forwards += ls->terms[j]->forwards[m]->name + ",";
			}
			std::cout << "	" << r->left->name << " -> ";
			int m = 0;
			for(m = 0; m < r->rights.size(); ++m)
			{
				if(m == dot) std::cout << "*";
				std::cout << " " << r->rights[m]->name; 
			}
			if(m == dot) std::cout << "*";
			std::cout << "	[" << forwards << "]" << std::endl;
		}
		std::cout << std::endl;
		for(goto_map::iterator it = ls->gotomap.begin(); it != ls->gotomap.end(); ++it)
		{
			std::cout << "	" << it->first->name << " GOTO " << it->second << std::endl;
		}
		std::cout << std::endl;		
	}
}

void xbytes::dump_action_table()
{
	std::cout << "==============ACTION TABLE============" << std::endl;
	std::cout << "STATE 	";
	for(int i = 0; i < symbols.size(); ++i)
	{
		std::cout << symbols[i]->name << "	";
	}
	std::cout << std::endl;

	for(int i = 0; i < states.size(); ++i)
	{
		std::cout << i << "	";
		for(int j = 0; j < states[i]->actions.size(); ++j)
		{
			int valid_action_count = 0;
			action_array& aa = states[i]->actions[j];
			for(int m = 0; m < aa.size(); ++m)
			{
				assert(valid_action_count <= 1);
				action& act = aa[m];
				std::stringstream ss;
				switch(act.type)
				{
				case LR_ACTION_REDUCE:
					ss << "R" << act.r->index << "	";
					valid_action_count++;
					break;
				case LR_ACTION_SHIFT:
					ss << "S" << act.next_state << "	";
					valid_action_count++;
					break;
				case LR_ACTION_GOTO:
					ss << "G" << act.next_state << "	";
					valid_action_count++;
					break;
				case LR_ACTION_ACCEPT:
					ss << "accept" << "	";
					valid_action_count++;
					break;
				case LR_ACTION_ERROR:
					ss << "-	";
					valid_action_count++;
				}

				std::cout << ss.str();
			}
		}
		std::cout << std::endl;
	}
}

void xbytes::dump_associativity()
{
	std::cout << "==============ASSOCIATIVITY============" << std::endl;
	for(int i = 0; i < assoc_precs.size(); ++i)
	{
		std::cout << "precedence=" << i;
		if(assoc_precs[i].associativity == AT_LEFT)
		{
			std::cout << " LEFT<< ";
		}
		else if(assoc_precs[i].associativity == AT_RIGHT)
		{
			std::cout << " RIGHT<< ";
		}
		else
		{
			std::cout << " ERROR<< ";
		}
		for(int m = 0; m < assoc_precs[i].symbol_names.size(); ++m)
		{
			std::cout << assoc_precs[i].symbol_names[m] << " ";
		}
		std::cout << ">>" << std::endl;
	}
}

void xbytes::output_declare_symbol(std::ofstream& ss)
{
	ss << "#define SYMBOL_COUNT " << symbols.size() << std::endl;
	ss << "enum TERMINAL_SYMBOL" << std::endl;
	ss << "{\n";
	int i = 0;
	for(i = 0; i < symbols.size(); ++i)
	{
		symbol* s = symbols[i];
		if(s->type == ST_NONTERMINAL) break;

		ss << "	" << s->name << " = " << s->index << "," << std::endl;
	}
	ss << "	TERMINAL_SYMBOL_COUNT = " << i << std::endl;
	ss << "};\n";
	ss << std::endl;	
}

void xbytes::output_symbol_table(std::ofstream& ss)
{
	for(int i = 0; i < symbols.size(); ++i)
	{
		ss << "/*"<< std::setw(4)  << i << "*/";
		ss << "\"" << symbols[i]->name << "\"," << std::endl;
	}
}

void xbytes::output_rule_table(std::ofstream& ss)
{
	for(int i = 0; i < rules.size(); ++i)
	{	
		ss << "/*"<< std::setw(4)  << i << "*/{\"";

		ss << (rules[i]->left)->name;
		if(rules[i]->left_alias.size() > 0)
		{
			ss << "(" << rules[i]->left_alias << ")";
		}
		ss <<" ->";
		for(int j = 0; j < rules[i]->rights.size(); ++j)
		{
			ss << " " << rules[i]->rights[j]->name;
			if(rules[i]->right_alias[j].size() > 0)
			{
				ss << "(" << rules[i]->right_alias[j] << ")";
			}
		}

		ss << "\", " << rules[i]->left->index << ", ";
		ss << rules[i]->rights.size() << "}," << std::endl;
	}
}

void xbytes::output_action_table(std::ofstream& ss)
{
	for(int i = 0; i < states.size(); ++i)
	{
		ss << "/*" << std::setw(4) << i << "*/";
		for(int j = 0; j < states[i]->actions.size(); ++j)
		{
			int valid_action_count = 0;
			action_array& aa = states[i]->actions[j];
			for(int m = 0; m < aa.size(); ++m)
			{
				assert(valid_action_count <= 1);
				action& act = aa[m];
				switch(act.type)
				{
				case LR_ACTION_ACCEPT:
				case LR_ACTION_REDUCE:
					ss << "{" << act.type << "," << act.r->index << "},";
					valid_action_count++;
					break;
				case LR_ACTION_SHIFT:
				case LR_ACTION_GOTO:
					ss << "{" << act.type << "," << act.next_state << "},";
					valid_action_count++;
					break;
				case LR_ACTION_ERROR:
					ss << "{" << act.type << ",0},";
					valid_action_count++;
					break;
				}
			}			
		}
		ss << std::endl;
	}
}

void xbytes::output_reduce_code(std::ofstream& ss)
{
	for(int i = 0; i < rules.size(); ++i)
	{
		rule* r = rules[i];
		if(r->code.size() == 0) continue;
		ss << "	case " << i << ":/* "  << r->str_rule << " */" << std::endl;

		string& code = r->code;

		//生成左边符号代码
		string& la = r->left_alias;
		if(la.size() > 0)
		{
			std::size_t spos = 0;
			while(spos < code.size())
			{
				std::size_t pos = code.find(la, spos);
				
				if(pos == std::string::npos) break;
				if(spos == pos) break;
				if(pos > 0 && isalpha(code[pos-1]))
				{
					spos = pos + la.size() - 1;
					continue;
				}
				if(pos < code.size() - 1 && isalpha(code[pos + la.size()]))
				{
					spos = pos + la.size() - 1;
					continue;
				}

				code.replace(pos, la.size(), "tt");
			}
		}

		//生成右边符号代码
		for(int m = 0; m < r->right_alias.size(); ++m)
		{
			string& ra = r->right_alias[m];
			if(ra.size() == 0) continue;

			std::stringstream ss;
			ss << "tv[" << m << "]";

			std::size_t spos = 0;
			while(spos < code.size())
			{
				std::size_t pos = code.find(ra, spos);

				if(pos == std::string::npos) break;
				if(spos == pos) break;
				if(pos > 0 && isalpha(code[pos-1]))
				{
					spos = pos + ra.size() - 1;
					continue;
				}
				if(pos < code.size() - 1 && isalpha(code[pos + ra.size()]))
				{
					spos = pos + ra.size() - 1;
					continue;
				}

				code.replace(pos, ra.size(), ss.str());
			}
		}

		ss << r->code << std::endl;
		ss << "	break;\n" << std::endl;
	}	
}

void xbytes::emit_hxx_file()
{
	//open output file
	std::ofstream ofs;
	ofs.open("PARSER.hxx", std::ofstream::out);

	//open template file
	std::ifstream ifs;
	ifs.open("TEMPLATE.hxx", std::ifstream::in);

	//copy data
	string line;
	while(ifs.good())
	{
		line.clear();
		getline(ifs, line);

		if(line.size() == 0)
		{
			ofs << std::endl;
			continue;
		}			

		if(line[0] == '%' && line[1] == '%')
		{
			string keyword;
			for(int i = 2; i < line.size(); ++i)
			{
				if(line[i] == '%') break;
				keyword.push_back(line[i]);
			}

			if(keyword == "DECLARE_SYMBOL")
			{
				output_declare_symbol(ofs);
			}
			else if(keyword == "DECLARE_INCLUDE_CODE")
			{
				if(include_code.size() > 0)
				{
					ofs << "//include file from .x file";
					ofs << include_code << std::endl;
				}					
			}
			else if(keyword == "DECLARE_TOKEN_CODE")
			{
				if(token_code.size() > 0)
				{
					ofs << "//declare token from .x file\n";
					ofs << "typedef " << token_code  << "TOKEN_TYPE;" << std::endl;					
				}
			}
		}
		else
		{
			ofs << line << std::endl;
		}
	}

	ifs.close();
	ofs.close();
}

void xbytes::emit_cxx_file()
{
	//open output file
	std::ofstream ofs;
	ofs.open("PARSER.cxx", std::ofstream::out);

	//open template file
	std::ifstream ifs;
	ifs.open("TEMPLATE.cxx", std::ifstream::in);

	//copy data
	string line;
	while(ifs.good())
	{
		line.clear();
		getline(ifs, line);

		if(line.size() == 0)
		{
			ofs << std::endl;
			continue;
		}
		
		if(line[0] == '%' && line[1] == '%')
		{
			string keyword;
			for(int i = 2; i < line.size(); ++i)
			{
				if(line[i] == '%') break;
				keyword.push_back(line[i]);
			}

			if(keyword == "DECLARE_SYMBOL_TABLE")
			{
				output_symbol_table(ofs);
			}
			else if(keyword == "DECLARE_RULE_TABLE")
			{
				output_rule_table(ofs);
			}
			else if(keyword == "DECLARE_ACTION_TABLE")
			{
				output_action_table(ofs);
			}
			else if(keyword == "DECLARE_REDUCE_CODE")
			{
				output_reduce_code(ofs);
			}
			else if(keyword == "DECLARE_SYNTAX_ERROR_CODE")
			{
				if(syntax_error_code.size() > 0)
					ofs << syntax_error_code << std::endl;
			}

			continue;
		}

		ofs << line << std::endl;	
	}

	ifs.close();
	ofs.close();
}

void xbytes::emit_action_table()
{
	std::ofstream ofs;
	ofs.open("ACTION.txt", std::ofstream::out);

	for(int i = 0; i < states.size(); ++i)
	{
		lr_state* ls = states[i];
		ofs << "STATE " << i << ":" << std::endl;

		//output all terms
		for(int j = 0; j < ls->terms.size(); ++j)
		{
			rule* r = ls->terms[j]->r;
			int dot = ls->terms[j]->dot;

			ofs << "	" << r->left->name << " -> ";
			int m = 0;
			for(m = 0; m < r->rights.size(); ++m)
			{
				if(m == dot) ofs << "*";
				ofs << " " << r->rights[m]->name; 
			}
			if(m == dot) ofs << "*";
			ofs << std::endl;
		}
		ofs << std::endl;

		//output all action
		for(int j = 0; j < states[i]->actions.size(); ++j)
		{
			action_array& aa = states[i]->actions[j];
			for(int m = 0; m < aa.size(); ++m)
			{
				action& act = aa[m];
				switch(act.type)
				{
				case LR_ACTION_REDUCE:
					ofs << "	" << symbols[j]->name << " REDUCE " << act.r->index << std::endl;
					break;
				case LR_ACTION_SHIFT:
					ofs << "	" << symbols[j]->name << " SHIFT " << act.next_state << std::endl;
					break;
				case LR_ACTION_GOTO:
					ofs << "	" << symbols[j]->name << " GOTO " << act.next_state << std::endl;
					break;
				case LR_ACTION_ACCEPT:
					ofs << "	" << symbols[j]->name << " ACCEPT" << std::endl;
					break;
				case LR_ACTION_SSCONFLICT:
					ofs << "	" << symbols[j]->name << " SSCONFLICT" << std::endl;
					break;
				case LR_ACTION_SRCONFLICT:
					ofs << "	" << symbols[j]->name << " SRCONFLICT" << std::endl;
					break;
				case LR_ACTION_RRCONFLICT:
					ofs << "	" << symbols[j]->name << " RRCONFLICT" << std::endl;
					break;
				case LR_ACTION_RDRESOLVED:
					ofs << "	" << symbols[j]->name << " RDRESOLVED" << std::endl;
					break;
				case LR_ACTION_SHRESOLVED:
					ofs << "	" << symbols[j]->name << " SHRESOLVED" << std::endl;
					break;
				}
			}
		}
		ofs << std::endl;	
	}

	ofs.close();
}

}//namespace xbytes