""""""""""""""""""""""""""""""""""""""""""""""""""""""
"
"" Clockwork Policy Master
"
"
"" policyd.conf
au BufNew,BufRead policyd.conf set ft=policyd
" *.pol - Policy Manifest
au BufNew,BufRead *.pol set ft=pol
