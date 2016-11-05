make: server.c client.c
	gcc server.c -o server379
	gcc client.c -o chat379

clean:
	rm -f *.o *.out *.log