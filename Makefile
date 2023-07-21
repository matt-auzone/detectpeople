OBJS := detectimg.o
LIBS := -lvaal

%.o : %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) 

detectimg: $(OBJS)
	dpkg -L libvaal
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)


install: detectimg
	mkdir -p $(WORKDIR)
	cp detectimg $(WORKDIR)/


clean:
	rm -f *.o
	rm -f detectimg
