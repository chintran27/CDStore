all:
	gcc -c galois.c -lgf_complete;
	gcc -c jerasure.c galois.c -lgf_complete;
	gcc -c cauchy.c jerasure.c galois.c -lgf_complete;
clean:
	rm -rf *.o
