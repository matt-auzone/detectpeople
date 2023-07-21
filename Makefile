OBJS := detectpeople.o
LIBS := -lvaal

%.o : %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) 

detectpeople: $(OBJS)
	dpkg -L libvaal
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)


install: detectpeople
	mkdir -p $(WORKDIR)
	cp detectpeople $(WORKDIR)/


clean:
	rm -f *.o
	rm -f detectpeople
