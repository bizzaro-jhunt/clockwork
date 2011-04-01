" Vim syntax file
" Language:	Clockwork Policy Files
" Maintainer:	James Hunt <filefrog@gmail.com>
" Last Change:	2011 Mar 31

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

syn keyword   polTodo           contained TODO FIXME XXX
syn match     polComment        "#.*" contains=polTodo

syn match     polType           /host\|policy\|user\|file\|group\|package/
syn keyword   polKeyword        if unless else map is not
syn keyword   polPreProc        enforce extend include

syn match     polFact           /\(\i\+\)\(\.\i\+\)\+/
syn match     polAttribute      /\I\i*\s*\(:\)\@=/ display
syn region    polString         start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+
syn match     polNumber         "[0-9][0-9]*"

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

hi def link   polComment        Comment
hi def link   polTodo           Todo

hi def link   polType           Type
hi def link   polKeyword        Statement
hi def link   polPreProc        PreProc

hi def link   polFact           Special
hi def link   polAttribute      Statement
hi def link   polString         String
hi def link   polNumber         String

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "pol"

"" vim: ts=8
