" Vim syntax file
" Language:	Clockwork Policy Master Configuration
" Maintainer:	James Hunt <filefrog@gmail.com>
" Last Change:	2011 Mar 31

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

syn keyword   policydTodo       contained TODO FIXME XXX
syn match     policydComment    "#.*" contains=policydTodo

syn keyword   policydDirective  ca_cert_file crl_file cert_file key_file manifest_file log_level certs_dir requests_dir
syn keyword   policydLogLevel   none emergency alert critical error warning notice info all debug

syn region    policydString     start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+
syn match     policydNumber     "[0-9][0-9]*"

syn match     policydInvalid    /\(\I\|\/\)\+/

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""


hi def link   policydComment    Comment
hi def link   policydTodo       Todo

hi def link   policydDirective  Type
hi def link   policydLogLevel   Identifier

hi def link   policydString     String
hi def link   policydNumber     String

hi def link   policydInvalid    Error

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "policyd"

"" vim: ts=8
