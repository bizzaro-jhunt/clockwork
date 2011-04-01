" Vim syntax file
" Language:	Clockwork Policy Master Configuration
" Maintainer:	James Hunt <filefrog@gmail.com>
" Last Change:	2011 Mar 31

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

syn keyword   cwaTodo       contained TODO FIXME XXX
syn match     cwaComment    "#.*" contains=cwaTodo

syn keyword   cwaDirective  ca_cert_file cert_file key_file request_file server port gatherers

syn region    cwaString     start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+
syn match     cwaNumber     "[0-9][0-9]*"

syn match     cwaInvalid    /\(\I\|\/\|\*\|\.\)\+/

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""


hi def link   cwaComment    Comment
hi def link   cwaTodo       Todo

hi def link   cwaDirective  Type

hi def link   cwaString     String
hi def link   cwaNumber     String

hi def link   cwaInvalid    Error

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "cwa"

"" vim: ts=8
