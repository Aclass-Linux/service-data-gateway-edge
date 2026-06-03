#ifndef FIFO_WRITER_H
#define FIFO_WRITER_H

int fifo_writer_init(const char *path);
void fifo_writer_write(double temperature);
void fifo_writer_cleanup(void);

#endif
