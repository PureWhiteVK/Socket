grammar Calculator;

@parser::header {
#include <string>
}

@parser::members {
int expression_value{};
}

// Parser rule names must start with a lowercase letter 
// and lexer rules must start with a capital letter.
s: e EOF { expression_value = $e.v; } ;
e returns [ int v ]
 : a=e '+' b=t { $v = $a.v + $b.v; } 
 | t { $v = $t.v; };
t returns [ int v ]
 : a=t '*' b=f { $v = $a.v * $b.v; }
 | f { $v = $f.v; };
f returns [ int v ]
 : '(' e ')' { $v = $e.v; }
 | DIGITS { $v = std::stoi($DIGITS.text); };
DIGITS: [0-9]+; 