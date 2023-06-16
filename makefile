all: folders server client

server: bin/monitor

client: bin/tracer

folders:
	@mkdir -p src obj bin tmp PIDS-folder
        

bin/monitor: obj/monitor.o
	gcc -g obj/monitor.o `pkg-config --libs glib-2.0` -o bin/monitor
	
obj/monitor.o: src/monitor.c
	gcc -Wall -g -c src/monitor.c `pkg-config --cflags glib-2.0` -o obj/monitor.o
	
  
bin/tracer: obj/tracer.o
	gcc -g obj/tracer.o -o bin/tracer
         
-o obj/tracer.o: src/tracer.c
	gcc -Wall -g -c src/tracer.c -o obj/tracer.o
        
        
        
clean:
	rm -f obj/* tmp/* PIDS-folder/* bin/*
