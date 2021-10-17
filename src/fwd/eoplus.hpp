#ifndef FWD_EOPLUS_HPP_INCLUDED
#define FWD_EOPLUS_HPP_INCLUDED

namespace EOPlus
{
    #define UOP2(a, b) ((unsigned char)((((unsigned char)(a) - 32)) + (((unsigned char)(b)) << 2)))
    #define UOP1(a) UOP2(a, '\0')
    #define UOP_ALT '\2'

    enum OperatorAssoc
    {
        ASSOC_NONE,
        ASSOC_RIGHT,
        ASSOC_LEFT,
    };

    enum OperatorArgs
    {
        OP_UNARY  = 1,
        OP_BINARY = 2
    };

    enum class Operator : unsigned char
    {
        LeftParens       = UOP1('('),
        RightParens      = UOP1(')'),
        BitAnd           = UOP1('&'),
        LogicalAnd       = UOP2('&', '&'),
        BitOr            = UOP1('|'),
        LogicalOr        = UOP2('|', '|'),
        Assign           = UOP1('='),
        Equality         = UOP2('=', '='),
        Inequality       = UOP2('!', '='),
        LessThan         = UOP1('<'),
        LessThanEqual    = UOP2('<', '='),
        GreaterThan      = UOP1('>'),
        GreaterThanEqual = UOP2('>', '='),
        Add              = UOP1('+'),
        Subtract         = UOP1('-'),
        Multiply         = UOP1('*'),
        Divide           = UOP1('/'),
        Modulo           = UOP1('%'),
        BitNot           = UOP1('~'),
        LogicalNot       = UOP1('!'),
        Negate           = UOP2('-', UOP_ALT)
    };

    struct OperatorInfo;
    struct Token;
    struct Scope;
    struct Expression;
    struct Action;
    struct Rule;
    struct Info;
    struct State;
    struct Quest;

    struct Syntax_Error;
    struct Lexer_Error;
    struct Parser_Error;
    struct Runtime_Error;
}

#endif
