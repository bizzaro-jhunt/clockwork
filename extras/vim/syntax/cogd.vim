" Vim syntax file
" Language:	Clockwork Policy Master Configuration
" Maintainer:	James Hunt <james@jameshunt.us>
" Last Change:	2014 Jun  1

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

syn keyword   cogdTodo       contained TODO FIXME XXX
syn match     cogdComment    "#.*" contains=cogdTodo

syn keyword   cogdDirective  listen timeout gatherers copydown interval pidfile difftool
syn match     cogdDirective  /master\.[1-8]/
syn match     cogdDirective  /syslog\.\(ident\|facility\|level\)/

syn keyword   cogdLogLevel   emergency alert critical error warning notice info debug
syn keyword   cogdLogFacil   local0 local1 local2 local3 local4 local5 local6 local7 daemon

syn region    cogdString     start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+
syn match     cogdNumber     "[0-9][0-9]*"
syn match     cogdIpAddr     /[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+/
syn match     cogdEndpoint   /\*:[0-9]\+/

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""


hi def link   cogdComment    Comment
hi def link   cogdTodo       Todo

hi def link   cogdDirective  Keyword
hi def link   cogdLogLevel   Type
hi def link   cogdLogFacil   Type

hi def link   cogdString     String
hi def link   cogdNumber     String
hi def link   cogdIpAddr     String
hi def link   cogdEndpoint   String

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "cogd"

"" vim: ts=8
