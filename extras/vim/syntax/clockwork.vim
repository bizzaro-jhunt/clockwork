" Vim syntax file
" Language:	Clockwork Policy Files
" Maintainer:	James Hunt <james@jameshunt.us>
" Last Change:	2014 Jun  1

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

syn keyword   clockworkTodo           contained TODO FIXME XXX
syn match     clockworkComment        "#.*" contains=clockworkTodo

syn match     clockworkType           /\<\(host\|policy\|user\|file\|group\|package\|service\|dir\|exec\|symlink\) /
syn keyword   clockworkKeyword        if unless else map is not like and or not
syn match     clockworkOperator       /==\|!=\|=\~\|!\~\|!/
syn match     clockworkDepKeyword     /depends  *on\|affects/
syn keyword   clockworkPreProc        enforce extend include defaults fallback

syn match     clockworkFact           /\(\i\+\)\(\.\i\+\)\+/
syn match     clockworkRegex          /m?\/(\\\/|[^\/])*\/i?/
syn match     clockworkRegex          /m|(\\||[^|])*|i?/
syn match     clockworkAttribute      /\I\i*\s*\(:\)\@=/ display
syn region    clockworkString         start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+
syn match     clockworkNumber         "[0-9]\+"

syn region    clockworkACL            start="\(allow\|deny\)" end="}\|\n"
syn match     clockworkACLUser        "[a-zA-Z_][a-zA-Z0-9_]*" contained containedin=clockworkACL
syn match     clockworkACLGroup       "%[a-zA-Z_][a-zA-Z0-9_]*" contained containedin=clockworkACL
syn region    clockworkACLCommand     start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+ contained containedin=clockworkACL
syn keyword   clockworkACLKeyword     allow deny final ALL contained containedin=clockworkACL

syn region    clockworkRegex matchgroup=clockworkRegexDelim  start=+\</+ end=+/[i]*+ contains=@clockworkRegexInt keepend extend
syn region    clockworkRegex matchgroup=clockworkRegexDelim start=+\<m/+ end=+/[i]*+ contains=@clockworkRegexInt keepend extend
syn region    clockworkRegex matchgroup=clockworkRegexDelim start=+\<m|+ end=+|[i]*+ contains=@clockworkRegexInt keepend extend

syn match     clockworkRegexMeta "\\\%(\o\{1,3}\|x\%({\x\+}\|\x\{1,2}\)\|c.\|[^cx]\)" contained extend
syn match     clockworkRegexMeta "\\." extend contained contains=NONE
syn match     clockworkRegexMeta "\\\\" contained
syn match     clockworkRegexMeta "\\[1-9]" contained extend
syn match     clockworkRegexMeta "\\g\%(\d\+\|{\%(-\=\d\+\|\h\w*\)}\)" contained
syn match     clockworkRegexMeta "\\k\%(<\h\w*>\|'\h\w*'\)" contained
syn match     clockworkRegexMeta "{\d\+\%(,\%(\d\+\)\=\)\=}" contained
syn match     clockworkRegexMeta "\[[]-]\=[^\[\]]*[]-]\=\]" contained extend
syn match     clockworkRegexMeta "[+*()?.]" contained
syn match     clockworkRegexMeta "(?[#:=!]" contained
syn match     clockworkRegexMeta "(?[impsx]*\%(-[imsx]\+\)\=)" contained
syn match     clockworkRegexMeta "(?\%([-+]\=\d\+\|R\))" contained
syn match     clockworkRegexMeta "(?\%(&\|P[>=]\)\h\w*)" contained
syn match     clockworkRegexMeta "(\*\%(\%(PRUNE\|SKIP\|THEN\)\%(:[^)]*\)\=\|\%(MARK\|\):[^)]*\|COMMIT\|F\%(AIL\)\=\|ACCEPT\))" contained
syn cluster   clockworkRegexInt contains=clockworkRegexMeta

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

hi def link   clockworkComment        Comment
hi def link   clockworkTodo           Todo

hi def link   clockworkType           Type
hi def link   clockworkKeyword        Statement
hi def link   clockworkOperator       Normal
hi def link   clockworkDepKeyword     Statement
hi def link   clockworkPreProc        PreProc

hi def link   clockworkFact           Special
hi def link   clockworkAttribute      Statement
hi def link   clockworkString         String
hi def link   clockworkNumber         String

hi def link   clockworkACLUser        Identifier
hi def link   clockworkACLGroup       Identifier
hi def link   clockworkACLCommand     String
hi def link   clockworkACLKeyword     Statement

hi def link   clockworkRegex          String
hi def link   clockworkRegexDelim     Statement
hi def link   clockworkRegexMeta      Special

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "clockwork"
set autoindent

"" vim: ts=8
