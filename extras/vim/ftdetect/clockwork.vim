" clockd.conf - Clockwork Policy Master configuration
au BufNewFile,BufRead clockd.conf set ft=clockd

" meshd.conf   - Clockwork Mesh Master configuration
au BufNewFile,BufRead meshd.conf set ft=meshd

" cogd.conf   - Clockwork Agent configuration
au BufNewFile,BufRead cogd.conf set ft=cogd

" ~/.cwrc     - Clockwork Multitool configuration
au BufNewFile,BufRead .cwrc set ft=cwrc
au BufNewFile,BufRead cw.conf set ft=cwrc

" *.pol       - Clockwork Policy Manifest files
au BufNewFile,BufRead *.pol set ft=clockwork
