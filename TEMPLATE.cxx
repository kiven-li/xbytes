#include "PARSER.hxx"
#include <iostream>
namespace xbytes {

const char* symbol_table[] =
{
%%DECLARE_SYMBOL_TABLE%%	
};

const rule rule_table[] =
{
%%DECLARE_RULE_TABLE%%	
};

const action action_table[] =
{
%%DECLARE_ACTION_TABLE%%	
};

parser::parser()
: xstack(),
  symbol_id_map()
{
	initialize();
}

parser::~parser()
{
}

void parser::initialize()
{
	//init symbol_id_map
	for(int i = 0; i < TERMINAL_SYMBOL_COUNT; ++i)
	{
		const string& s = symbol_table[i];
		symbol_id_map[s] = i;
	}

	//init stack
	stack_state ss;
	ss.state = 0;
	ss.symbol = $;//$

	xstack.push(ss);
}

TOKEN_TYPE parser::execute_reduce_code(int rule_index, token_vector& tv)
{
	TOKEN_TYPE tt;
	if(tv.size() > 0) tt = tv[0];
	switch(rule_index)
	{
%%DECLARE_REDUCE_CODE%%
	}

	return tt;
}

void parser::eat(int tt, TOKEN_TYPE t)
{
	while(xstack.size() > 0)
	{
		stack_state& ss = xstack.top();
		int action_index = SYMBOL_COUNT * ss.state + tt;
		if(action_index >= sizeof(action_table))
		{
%%DECLARE_SYNTAX_ERROR_CODE%%
			break;
		}

		const action& act = action_table[action_index];
		if(act.type == LR_ACTION_SHIFT)
		{
			stack_state nss;
			nss.state = act.target;
			nss.symbol = tt;
			nss.t = t;
			xstack.push(nss);
			break;
		}
		else if(act.type == LR_ACTION_REDUCE)
		{
			const rule& r = rule_table[act.target];
			token_vector tv(r.right_count);
			for(int i = 0; i < r.right_count; ++i)
			{
				stack_state& pss = xstack.top();
				tv[r.right_count - 1 - i] = pss.t;
				xstack.pop();				
			} 

			TOKEN_TYPE rct = execute_reduce_code(act.target, tv);

			ss = xstack.top();
			const action& goto_act = action_table[r.left + SYMBOL_COUNT * ss.state];
			if(goto_act.type != LR_ACTION_GOTO)
			{
%%DECLARE_SYNTAX_ERROR_CODE%%				
				break;
			}

			stack_state nss;
			nss.state = goto_act.target;
			nss.symbol = r.left;
			nss.t = rct;
			xstack.push(nss);
		}
		else if(act.type == LR_ACTION_ACCEPT)
		{
			const rule& r = rule_table[act.target];
			token_vector tv;
			tv.push_back(ss.t);
			xstack.pop();

			execute_reduce_code(act.target, tv);
			break;
		}
		else
		{
%%DECLARE_SYNTAX_ERROR_CODE%%
			break;
		}
	}	
}

}//namespace xbytes
