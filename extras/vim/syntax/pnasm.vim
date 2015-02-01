" Vim syntax file
" Language:    Clockwork Pendulum Assembly
" Maintainer:  James Hunt <james@jameshunt.us>
" Last Change: 2015 Jan 8

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

syn keyword   pnasmTodo       contained TODO FIXME XXX
syn match     pnasmComment    ";.*" contains=pnasmTodo

syn match     pnasmFunc       /\(fn\|call\|try\)[ \t]*[a-z][a-z0-9._?:-]*/ contains=pnasmOpFn,pnasmFuncName
syn keyword   pnasmOpFn       fn call try contained
syn match     pnasmFuncName   /[a-z][a-z0-9._?:-]*/ contained

syn match     pnasmLog        /syslog[ \t]*[^ \t]*/ contains=pnasmOpSyslog,pnasmLogLevel,pnasmLogBad
syn keyword   pnasmOpSyslog   syslog contained
syn keyword   pnasmLogLevel   emerg emergency alert critical err warning notice info debug contained
syn match     pnasmLogBad     /[^ \t]*/ contained

syn keyword   pnasmOpcode1    noop push pop set swap acc anno runtime
syn keyword   pnasmOpcode1    add sub mult div mod
syn keyword   pnasmOpcode1    eq lt lte gt gte streq jmp jz jnz
syn keyword   pnasmOpcode1    string print error perror flag unflag
syn match     pnasmOpcode1    /flagged?/
syn match     pnasmOpCode1    /fs\.\(stat\|type\|readlink\|dev\|inode\|mode\|nlink\|uid\|gid\|major\|minor\|size\|atime\|mtime\|ctime\)/
syn match     pnasmOpCode1    /fs\.\(touch\|mkdir\|link\|symlink\|unlink\|rmdir\|rename\|copy\|chown\|chgrp\|chmod\|sha1\|get\|put\|mkparent\)/
syn match     pnasmOpCode1    /fs\.\(open\|read\|close\)dir/
syn match     pnasmOpCode1    /fs\.\(file\|symlink\|dir\|chardev\|blockdev\|fifo\|socket\)?/
syn match     pnasmOpCode1    /authdb\.\(open\|save\|close\|next[ug]id\)/
syn match     pnasmOpCode1    /\(user\|group\)\.\(find\|get\|set\|new\|delete\)/
syn match     pnasmOpCode1    /group\.\(has?\|join\|kick\)/
syn match     pnasmOpCode1    /augeas\.\(init\|done\|perror\|write\|set\|find\|remove\)/
syn match     pnasmOpCode1    /env\.\(get\|set\|unset\)/
syn keyword   pnasmOpCode1    localsys exec dump acl umask loglevel geteuid getegid
syn match     pnasmOpCode1    /runas\.\(uid\|gid\)/
syn match     pnasmOpCode1    /show\.\(acl\|acls\)/
syn match     pnasmOpCode1    /remote\.\(live?\|sha1\|file\)/

syn keyword   pnasmOpcode2    ret retv bail pragma halt property topic

syn region    pnasmString     start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+ contains=pnasmFormat
syn region    pnasmString     start=+<<EOF$+ end=/^EOF$/ contains=pnasmFormat
syn match     pnasmFormat     display "%\[[a-p]\]\(\d\+\$\)\=[-+' #0*]*\(\d*\|\*\|\*\d\+\$\)\(\.\(\d*\|\*\|\*\d\+\$\)\)\=\([hlL]\|ll\)\=\([bdiuoxXDOUfeEgGcCsSpn]\|\[\^\=.[^]]*\]\)" contained
syn match     pnasmFormat     display /\\[nrt"]/ contained
syn match     pnasmFormat     display /%[%T]/ contained

syn match     pnasmNumber     /[0-9][0-9]*/
syn match     pnasmOffset     /+[0-9][0-9]*/
syn match     pnasmRegister   /%[a-p]/
syn match     pnasmLabel      /[a-z][^ \t]*:/

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

hi def link   pnasmComment    Comment
hi def link   pnasmTodo       Todo
hi def link   pnasmFuncName   Normal

hi def link   pnasmOpcode1    Keyword
hi def link   pnasmOpSyslog   Keyword

hi def link   pnasmOpcode2    Special
hi def link   pnasmOpFn       Special
hi def link   pnasmRegister   Type
hi def link   pnasmFormat     SpecialChar

hi def link   pnasmLogLevel   Keyword
hi def link   pnasmLogBad     Error

hi def link   pnasmString     String
hi def link   pnasmNumber     String
hi def link   pnasmOffset     Identifier
hi def link   pnasmLabel      Identifier

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "pnasm"

"" vim: ts=8
