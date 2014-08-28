" clockd.conf - Clockwork Policy Master configuration
au BufNew,BufRead clockd.conf set ft=clockd

" meshd.conf   - Clockwork Mesh Master configuration
au BufNew,BufRead meshd.conf set ft=meshd

" cogd.conf   - Clockwork Agent configuration
au BufNew,BufRead cogd.conf set ft=cogd

" ~/.cwrc     - Clockwork Multitool configuration
au BufNew,BufRead .cwrc set ft=cwrc
au BufNew,BufRead cw.conf set ft=cwrc

" *.pol       - Clockwork Policy Manifest files
au BufNew,BufRead *.pol set ft=clockwork
