cc = g++
prom = jobs
deps = p0_starter.h
obj = main.o
$(prom) : $(obj)
	$(cc) -o $(prom) $(obj)

%.o: %.c $(deps)
	$(cc) -c $< -o $(deps)
clean:
	rm *.o