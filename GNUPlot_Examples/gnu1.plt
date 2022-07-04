# set terminal pngcairo  transparent enhanced font "arial,10" fontscale 1.0 size 600, 400 
# set output 'surface2.1.png'
set terminal tek40xx
set dummy u, v
set key bmargin center horizontal Right noreverse enhanced autotitle nobox
set parametric
set view 45, 50, 1, 1
set isosamples 50, 10
set hidden3d back offset 1 trianglepattern 3 undefined 1 altdiagonal bentover
set style data lines
set ztics  norangelimit -1.00000,0.25,1.00000
set title "Parametric Sphere" 
set urange [ -1.57080 : 1.57080 ] noreverse nowriteback
set vrange [ 0.00000 : 6.28319 ] noreverse nowriteback
set xrange [ * : * ] noreverse writeback
set x2range [ * : * ] noreverse writeback
set yrange [ * : * ] noreverse writeback
set y2range [ * : * ] noreverse writeback
set zrange [ * : * ] noreverse writeback
set cbrange [ * : * ] noreverse writeback
set rrange [ * : * ] noreverse writeback
NO_ANIMATION = 1
splot cos(u)*cos(v),cos(u)*sin(v),sin(u)
