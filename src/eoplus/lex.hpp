#ifndef EOPLUS_LEX_HPP_INCLUDED
#define EOPLUS_LEX_HPP_INCLUDED

#include "extras/lex.hpp"

#include <functional>
#include <queue>

#include "../eoplus.hpp"

namespace EOPlus
{
	class Lexer
	{
		private:
			int row;
			int col;
			std::basic_istream<char>& is;
			std::queue<Token> token_buffer;

			bool PeekChar(char& c);
			bool GetChar(char& c);
			bool GetCharIf(char& c, std::function<bool(char)> f);

			Token ReadNumber();
			Token ReadString();
			Token ReadIdentifier();
			Token ReadSymbol();
			Token ReadOperator();

		public:
			Lexer(std::basic_istream<char>& is);

			Token ReadToken();

			template <class IT> IT Lex(IT it)
			{
				Token t;

				for (t = ReadToken(); t && t.type != Token::EndOfFile; t = ReadToken())
					*it++ = t;

				*it++ = t;

				return it;
			}
	};
}

#endif
