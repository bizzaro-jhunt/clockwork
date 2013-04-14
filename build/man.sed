/^Content-type:/d;
/<!DOCTYPE/d;
/<HTML/d;
/HEAD><BODY/d;
/^Section:/d;
/>Return to Main Contents</d;
/<HR>/,/HTML>/d;
s/<B>/<strong>/g;
s@</B>@</strong>@g;
s/<I>/<em>/g;
s@</I>@</em>@g;

/<A NAME=/d;
s/<A HREF=[^>]*>//g;
s@</A>@@g;
s/DL COMPACT/DL/g
s/<DT>\([0-9]\)/<dt class="num">\1/;
