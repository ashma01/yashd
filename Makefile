all: yashd yash

yashd: yashd.c
	gcc -o yashd yashd.c -lpthread

yash: yash.c
	gcc -o yash yash.c 	


clean:
	rm yashd
	rm yash
	
