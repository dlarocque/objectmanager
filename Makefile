Main: ObjectManager.o ObjectManager.h main.c
	clang++ -Wall -g main.c ObjectManager.o -o main

ObjectManager.o: ObjectManager.h ObjectManager.c
	clang++ -Wall -g -c ObjectManager.c


