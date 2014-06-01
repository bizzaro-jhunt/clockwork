" Vim syntax file
" Language:	Clockwork Policy Master Configuration
" Maintainer:	James Hunt <filefrog@gmail.com>
" Last Change:	2011 Mar 31

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

syn keyword   cogdTodo       contained TODO FIXME XXX
syn match     cogdComment    "#.*" contains=cogdTodo

syn keyword   cogdDirective  listen timeout gatherers copydown interval pidfile
syn keyword   cogdDirective  syslog.ident syslog.facility syslog.level
syn keyword   cogdDirective  master.1 master.2 master.3 master.4
syn keyword   cogdDirective  master.5 master.6 master.7 master.8

syn keyword   cogdLogLevel   emergency alert critical error warning notice info debug

syn region    cogdString     start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+
syn match     cogdNumber     "[0-9][0-9]*"

syn match     cogdInvalid    /\(\I\|\/\|\*\|\.\)\+/

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""


hi def link   cogdComment    Comment
hi def link   cogdTodo       Todo

hi def link   cogdDirective  Type
hi def link   cogdLogLevel   Identifier

hi def link   cogdString     String
hi def link   cogdNumber     String

hi def link   cogdInvalid    Error

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "cogd"

"" vim: ts=8
