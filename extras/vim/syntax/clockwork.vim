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

syn match     clockworkType           /host\|policy\|user\|file\|group\|package\|service\|sysctl\|dir\|exec/
syn keyword   clockworkKeyword        if unless else map is not
syn match     clockworkDepKeyword     /depends  *on\|affects/
syn keyword   clockworkPreProc        enforce extend include defaults fallback

syn match     clockworkFact           /\(\i\+\)\(\.\i\+\)\+/
syn match     clockworkAttribute      /\I\i*\s*\(:\)\@=/ display
syn region    clockworkString         start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+
syn match     clockworkNumber         "[0-9][0-9]*"

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

hi def link   clockworkComment        Comment
hi def link   clockworkTodo           Todo

hi def link   clockworkType           Type
hi def link   clockworkKeyword        Statement
hi def link   clockworkDepKeyword     Statement
hi def link   clockworkPreProc        PreProc

hi def link   clockworkFact           Special
hi def link   clockworkAttribute      Statement
hi def link   clockworkString         String
hi def link   clockworkNumber         String

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "clockwork"
set autoindent

"" vim: ts=8
