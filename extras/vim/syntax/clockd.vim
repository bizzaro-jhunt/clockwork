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

syn keyword   clockdDirective  listen timeout gatherers copydown interval pidfile manifest
syn match     clockdDirective  /syslog\.\(ident\|facility\|level\)/
syn match     clockdDirective  /security\.\(strict\|cert\|trusted\)/
syn match     clockdDirective  /ccache\.\(connections\|expiration\)/

syn keyword   clockdYesNo      yes no
syn keyword   clockdLogLevel   emergency alert critical error warning notice info debug
syn keyword   clockdLogFacil   local0 local1 local2 local3 local4 local5 local6 local7 daemon

syn region    clockdString     start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+
syn match     clockdNumber     "[0-9][0-9]*"
syn match     clockdIpAddr     /[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+/
syn match     clockdEndpoint   /\*:[0-9]\+/

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

hi def link   clockdComment    Comment
hi def link   clockdTodo       Todo

hi def link   clockdDirective  Keyword
hi def link   clockdYesNo      Type
hi def link   clockdLogLevel   Type
hi def link   clockdLogFacil   Type

hi def link   clockdString     String
hi def link   clockdNumber     String
hi def link   clockdIpAddr     String
hi def link   clockdEndpoint   String

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "clockd"

"" vim: ts=8
