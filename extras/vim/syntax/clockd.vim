" Vim syntax file
" Language:	Clockwork Policy Master Configuration
" Maintainer:	James Hunt <james@jameshunt.us>
" Last Change:	2014 Jun  1

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

syn keyword   clockdTodo       contained TODO FIXME XXX
syn match     clockdComment    "#.*" contains=clockdTodo

syn keyword   clockdDirective  listen timeout gatherers copydown interval pidfile
syn keyword   clockdDirective  syslog.ident syslog.facility syslog.level
syn keyword   clockdDirective  master.1 master.2 master.3 master.4
syn keyword   clockdDirective  master.5 master.6 master.7 master.8

syn keyword   clockdLogLevel   emergency alert critical error warning notice info debug

syn region    clockdString     start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+
syn match     clockdNumber     "[0-9][0-9]*"

syn match     clockdInvalid    /\(\I\|\/\)\+/

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

hi def link   clockdComment    Comment
hi def link   clockdTodo       Todo

hi def link   clockdDirective  Type
hi def link   clockdLogLevel   Identifier

hi def link   clockdString     String
hi def link   clockdNumber     String

hi def link   clockdInvalid    Error

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "clockd"

"" vim: ts=8
