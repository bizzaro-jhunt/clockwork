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

syn keyword   pnasmOpcode1    noop push pop set swap acc
syn keyword   pnasmOpcode1    add sub mult div mod
syn keyword   pnasmOpcode1    eq lt lte gt gte streq jmp jz jnz
syn keyword   pnasmOpcode1    string print error perror syslog
syn keyword   pnasmOpcode1    flag unflag flagged?
syn match     pnasmOpCode1    /fs\.\(stat\|readlink\|dev\|inode\|mode\|nlink\|uid\|gid\|major\|minor\|size\|atime\|mtime\|ctime\)/
syn match     pnasmOpCode1    /fs\.\(touch\|mkdir\|link\|symlink\|unlink\|rmdir\|rename\|copy\|chown\|chgrp\|chmod\|sha1\|get\|put\)/
syn match     pnasmOpCode1    /fs\.\(file\|symlink\|dir\|chardev\|blockdev\|fifo\|socket\)\?/
syn match     pnasmOpCode1    /authdb\.\(open\|save\|close\|next\[ug\]id\)/
syn match     pnasmOpCode1    /\(user\|group\)\.\(find\|get\|set\|new\|delete\)/
syn match     pnasmOpCode1    /augeas\.\(init\|done\|perror\|write\|set\|find\|remove\)/
syn match     pnasmOpCode1    /env\.\(get\|set\|unset\)/
syn match     pnasmOpCode1    /show\.\(acl\|acls\)/

syn keyword   pnasmOpcode2    fn call try ret retv bail pragma halt

syn region    pnasmString     start=+L\="+ skip=+\\\\\|\\"\|\\$+ excludenl end=+"+ contains=pnasmFormat
syn match     pnasmFormat     display "%\[[a-p]\]\(\d\+\$\)\=[-+' #0*]*\(\d*\|\*\|\*\d\+\$\)\(\.\(\d*\|\*\|\*\d\+\$\)\)\=\([hlL]\|ll\)\=\([bdiuoxXDOUfeEgGcCsSpn]\|\[\^\=.[^]]*\]\)" contained
syn match     pnasmFormat     display "%%" contained

syn match     pnasmNumber     "[0-9][0-9]*"
syn match     pnasmOffset     "+[0-9][0-9]*"
syn match     pnasmRegister   "%[a-p]"
syn match     pnasmLabel      "[a-z].*:"

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

hi def link   pnasmComment    Comment
hi def link   pnasmTodo       Todo

hi def link   pnasmOpcode1    Keyword
hi def link   pnasmOpcode2    Special
hi def link   pnasmRegister   Type
hi def link   pnasmFormat     SpecialChar

hi def link   pnasmString     String
hi def link   pnasmNumber     String
hi def link   pnasmOffset     Identifier
hi def link   pnasmLabel      Identifier

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

let b:current_syntax = "pnasm"

"" vim: ts=8