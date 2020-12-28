CFLAGS += -DEXT_SIGINT
TARGET = libubox.a

OBJS = avl.o avl-cmp.o blob.o blobmsg.o uloop.o usock.o ustream.o ustream-fd.o vlist.o utils.o safe_list.o runqueue.o md5.o kvlist.o ulog.o base64.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) cr $@ $(OBJS)
	$(RANLIB) $@

.c.o::
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)

