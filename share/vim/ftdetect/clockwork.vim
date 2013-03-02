" policyd.conf - Clockwork Policy Master configuration
au BufNew,BufRead policyd.conf set ft=policyd
" cwa.conf     - Clockwork Agent configuration
au BufNew,BufRead cwa.conf set ft=cwa
" *.pol        - Clockwork Policy Manifest files
au BufNew,BufRead *.pol set ft=pol
