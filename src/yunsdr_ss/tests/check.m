%& read samples from signal file
d = read_short_binary('~/work/gnuradio/libyunsdr/build/rx_signal_file');
%d = read_short_binary('~/work/gnuradio/libyunsdr/build/rf_txrx_tone');
subplot(2,1,1);
i = d(1:2:end);
q = d(2:2:end);
plot(i);hold on;
plot(q);hold off;
subplot(2,1,2);
plot(i,q);axis equal;

