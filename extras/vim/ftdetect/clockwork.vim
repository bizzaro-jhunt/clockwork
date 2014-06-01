" clockd.conf - Clockwork Policy Master configuration
au BufNew,BufRead clockd.conf set ft=clockd
" cogd.conf   - Clockwork Agent configuration
au BufNew,BufRead cogd.conf set ft=cogd
" *.pol       - Clockwork Policy Manifest files
au BufNew,BufRead *.pol set ft=clockwork
