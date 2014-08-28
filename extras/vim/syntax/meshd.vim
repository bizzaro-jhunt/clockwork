" Vim syntax file
" Language:    Clockwork Mesh Master Configuration
" Maintainer:  James Hunt <james@jameshunt.us>
" Last Change: 2014 Sep  4

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

syn keyword   meshdTodo       contained TODO FIXME XXX
syn match     meshdComment    "#.*" contains=meshdTodo

syn keyword   meshdDirective  control broadcast pidfile
syn match     meshdDirective  /client\.\(connections\|expiration\)/
syn match     meshdDirective  /syslog\.\(ident\|facility\|level\)/
syn match     meshdDirective  /auth\.service/
syn match     meshdDirective  /security\.cert/
syn match     meshdDirective  /acl\.global/

syn keyword   meshdLogLevel   emergency alert critical error warning notice info debug
syn keyword   meshdLogFacil   local0 local1 local2 local3 local4 local5 local6 local7 daemon
syn keyword   meshdACLDisp    allow deny

syn region    meshdString     start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+
syn match     meshdNumber     "[0-9][0-9]*"
syn match     meshdIpAddr     /[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+/
syn match     meshdEndpoint   /\*:[0-9]\+/

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""


hi def link   meshdComment    Comment
hi def link   meshdTodo       Todo

hi def link   meshdDirective  Keyword
hi def link   meshdLogLevel   Type
hi def link   meshdLogFacil   Type
hi def link   meshdACLDisp    Type

hi def link   meshdString     String
hi def link   meshdNumber     String
hi def link   meshdIpAddr     String
hi def link   meshdEndpoint   String

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "meshd"

"" vim: ts=8
