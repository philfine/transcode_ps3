all: run

ps3_convert: ps3_convert.c
	gcc -O3 -o ps3_convert ps3_convert.c `pkg-config --cflags --libs gstreamer-0.10`

run: ps3_convert
	GST_DEBUG=ps3_convert:5 ./ps3_convert ~/Movies/movie.avi
