# set terminal pngcairo  transparent enhanced font "arial,10" fontscale 1.0 size 600, 400 
# set output 'contours.5.png'
set terminal tek40xx
set key at screen 1, 0.9 right top vertical Right noreverse enhanced autotitle nobox
set style textbox  opaque margins  0.5,  0.5 fc  bgnd noborder linewidth  1.0
set view 60, 30, 1, 1.1
set samples 20, 20
set isosamples 21, 21
set contour base
set cntrlabel  format '%8.3g' font ',7' start 5 interval 20
set cntrparam levels 10
set style data lines
set title "contours on base grid with labels" 
set xlabel "X axis" 
set xrange [ * : * ] noreverse writeback
set x2range [ * : * ] noreverse writeback
set ylabel "Y axis" 
set yrange [ * : * ] noreverse writeback
set y2range [ * : * ] noreverse writeback
set zlabel "Z " 
set zlabel  offset character 1, 0, 0 font "" textcolor lt -1 norotate
set zrange [ * : * ] noreverse writeback
set cbrange [ * : * ] noreverse writeback
set rrange [ * : * ] noreverse writeback
NO_ANIMATION = 1
splot x**2-y**2 with lines, x**2-y**2 with labels boxed notitle
