" Vim syntax file
" Language:    Clockwork CLI run control
" Maintainer:  James Hunt <james@jameshunt.us>
" Last Change: 2014 Sep  4

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

syn keyword   cwrcTodo       contained TODO FIXME XXX
syn match     cwrcComment    "#.*" contains=cwrcTodo

syn match     cwrcDirective  /run\.\(master\|cert\)/

syn region    cwrcString     start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+
syn match     cwrcNumber     "[0-9][0-9]*"
syn match     cwrcIpAddr     /[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+/
syn match     cwrcEndpoint   /\*:[0-9]\+/

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""


hi def link   cwrcComment    Comment
hi def link   cwrcTodo       Todo

hi def link   cwrcDirective  Keyword

hi def link   cwrcString     String
hi def link   cwrcNumber     String
hi def link   cwrcIpAddr     String
hi def link   cwrcEndpoint   String

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "cwrc"

"" vim: ts=8
