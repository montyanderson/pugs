default:
	gcc main.c -o pugs -lGL -lGLU -lglut -lcurl -ljpeg -O3 -Wextra -Wall -Wunreachable-code

install:
	cp pugs /usr/local/bin
