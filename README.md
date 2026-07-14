# calc

```
st_ops stands for state operator, can be '+', '-', '*', '/', '^'
st_opd stands for state operand, can be 0-9 and '.'
st_lps stands for left parenthesis: '('
st_rps stands for right parentesis: ')'
st_flg stands for flags: '+' or '-'
```
![](https://raw.github.com/ForU/calculator-simple/master/state_transition.png "state transition procedure")

## Usage 

```bash
$ ./calc "(-5 + (-00.01 - 0.09) * 10 ^ 2) / 5"
-3
```
