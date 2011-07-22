                                        
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>

#include "memory.h"
#include "tqueue.h"
#include "util.h"

typedef struct __batch {
    struct __batch* next; // Link to next batch in chain.
    int write_pos;        // Position for next write.
    void* elems[];        // Here *follows* an array of size batch_size.
} batch;

struct __hlt_thread_queue {
    pthread_spinlock_t lock; // Protects accesses to shared data.

    // These are safe to *read* from any thread. They won't be changed after
    // initialization.
    int writers;
    int batch_size;
    int max_batches;

    // These are safe to access from the writers without locking.
    batch**   writer_batches;     // Array of batches, one for each writer.
    uint64_t* writer_num_written; // Array of total number of elements written so far per writer.

    // These are safe to access without locking from the reader only.
    batch*   reader_head;                 // First batch the reader is working on.
    int      reader_pos;                  // Position for next read in reader_head.
    uint64_t reader_num_read;             // Total number of elements read so far.
    int      reader_num_terminated;       // Number of writers the reader has found to have terminated.
    hlt_thread_queue_stats* reader_stats; // Stats fo reader.

    // These must use the lock for access.
    int     lock_num_pending;             // Number of batches waiting for reader to pick up.
    batch*  lock_pending_head;            // First batch waiting for the reader to pick up.
    batch*  lock_pending_tail;            // Last batch waiting for the reader to pick up.
    int     lock_block;                   // Max capacity reached, writes will block/fail.
    int*    lock_writers_terminated;      // Array indicating which writers have terminated.
    hlt_thread_queue_stats* writer_stats; // Array with stats for writer.
};


static void _fatal_error(const char* msg)
{
    fprintf(stderr, "hlt_thread_queue: %s\n", msg);
    exit(1);
}

// Note: When using _acquire/_release, it is important that there's no
// cancelation point between the two calls as otherwise a lock main remain
// acquired after a thread has already terminated.
inline static void _acquire_lock(hlt_thread_queue* queue, int* i, int reader, int thread)
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, i);

    if ( pthread_spin_lock(&queue->lock) != 0 )
        _fatal_error("cannot acquire lock");

    //fprintf(stderr, "acquired %p %d %d\n", queue, reader, thread);

}

inline static void _release_lock(hlt_thread_queue* queue, int i, int reader, int thread)
{
    if ( pthread_spin_unlock(&queue->lock) != 0 )
        _fatal_error("cannot release lock");

    //fprintf(stderr, "released %p %d %d\n", queue, reader, thread);

    pthread_setcancelstate(i, NULL);
}

#if 0

static void _debug_print_batch(batch* b)
{
    while ( b ) {
        fprintf(stderr, "-> %p[%d]", b, b->write_pos);
        b = b->next;
    }
    fprintf(stderr, "\n");
}

static void _debug_print_queue(const char *prefix, hlt_thread_queue* q, int reader, int thread)
{
    int s;
    _acquire_lock(q, &s, reader, thread);

    fprintf(stderr, "%s: %p\n", prefix, q);
    for ( int i = 0; i < q->writers; i++ ) {
        fprintf(stderr, "  writer %d (elems %lu, terminated %d) * ", i, q->writer_num_written[i], q->lock_writers_terminated[i]);
        _debug_print_batch(q->writer_batches[i]);
    }

    fprintf(stderr, "  reader at %d (elems %lu, found terminated %d) * ", q->reader_pos, q->reader_num_read, q->reader_num_terminated);
    _debug_print_batch(q->reader_head);

    fprintf(stderr, "  pending (num %d / block %d) * ", q->lock_num_pending, q->lock_block);
    _debug_print_batch(q->lock_pending_head);

    _release_lock(q, s, reader, thread);
}

#endif

hlt_thread_queue* hlt_thread_queue_new(int writers, int batch_size, int max_batches)
{
    hlt_thread_queue *queue = (hlt_thread_queue *) hlt_gc_malloc_non_atomic(sizeof(hlt_thread_queue));
    if ( ! queue )
        _fatal_error("out of memory");

    queue->writers = writers;
    queue->batch_size = batch_size;
    queue->max_batches = max_batches;

    queue->reader_head = 0;
    queue->reader_pos = 0;
    queue->reader_num_read = 0;
    queue->reader_num_terminated = 0;
    queue->reader_stats = (hlt_thread_queue_stats*) hlt_gc_malloc_non_atomic(sizeof(hlt_thread_queue_stats));
    memset(queue->reader_stats, 0, sizeof(hlt_thread_queue_stats));

    queue->writer_batches = (batch**) hlt_gc_malloc_non_atomic(sizeof(batch*) * writers);
    queue->writer_num_written = (uint64_t*) hlt_gc_malloc_non_atomic(sizeof(uint64_t) * writers);
    queue->writer_stats = (hlt_thread_queue_stats*) hlt_gc_malloc_non_atomic(sizeof(hlt_thread_queue_stats) * writers);

    queue->lock_num_pending = 0;
    queue->lock_pending_head = 0;
    queue->lock_pending_tail = 0;
    queue->lock_block = 0;
    queue->lock_writers_terminated = (int*) hlt_gc_malloc_non_atomic(sizeof(int) * writers);
    memset(queue->writer_stats, 0, sizeof(hlt_thread_queue_stats) * writers);

    if ( ! (queue->writer_batches && queue->writer_num_written && queue->lock_writers_terminated && queue->reader_stats && queue->writer_stats) )
        _fatal_error("out of memory");

    for ( int i = 0; i < writers; ++i ) {
        queue->writer_batches[i] = 0;
        queue->writer_num_written[i] = 0;
        queue->lock_writers_terminated[i] = 0;
    }

    if ( pthread_spin_init(&queue->lock, PTHREAD_PROCESS_PRIVATE) != 0 )
        _fatal_error("cannot init lock");

    return queue;
}

void hlt_thread_queue_delete(hlt_thread_queue* queue)
{
    if ( pthread_spin_destroy(&queue->lock) != 0 )
        _fatal_error("cannot destroy lock");
}

void hlt_thread_queue_write(hlt_thread_queue* queue, int writer, void *elem)
{
    if ( queue->lock_writers_terminated[writer] )
        // Ignore when we have already terminated. We can read this without
        // locking as we're the only thread ever going to write to it.
        return;

    do {
        batch* b = queue->writer_batches[writer];

        // If we don't have a batch, get us one.
        if ( ! b ) {
            b = (batch*) hlt_gc_malloc_non_atomic(sizeof(batch) * queue->batch_size * sizeof(void*));
            if ( ! b )
                _fatal_error("out of memory");

            b->write_pos = 0;
            b->next = 0;

            queue->writer_batches[writer] = b;
        }

        // If there isn't enough space in our batch, flush.
        if ( b->write_pos >= queue->batch_size )
            hlt_thread_queue_flush(queue, writer);

    } while ( ! queue->writer_batches[writer] );

    // Write the element.
    batch* b = queue->writer_batches[writer];
    assert(b && b->write_pos < queue->batch_size);
    b->elems[b->write_pos++] = elem;
    ++queue->writer_num_written[writer];
    ++queue->writer_stats[writer].elems;
}

void hlt_thread_queue_flush(hlt_thread_queue* queue, int writer)
{
    int block;

    batch* b = queue->writer_batches[writer];

    if ( ! ( b && b->write_pos ) )
        // Nothing to do.
        return;

    do {
        int i;

        _acquire_lock(queue, &i, 0, writer);
        ++queue->writer_stats[writer].locked;

        if ( ! queue->lock_block ) {
            block = 0;

            // If we have space in the queue, move our batch to the end of
            // the reader's pending queue.
            if ( queue->lock_num_pending < queue->max_batches || ! queue->max_batches ) {
                if ( queue->lock_pending_tail )
                    queue->lock_pending_tail->next = b;
                else
                    queue->lock_pending_head = b;

                queue->lock_pending_tail = b;

                ++queue->lock_num_pending;
                ++queue->writer_stats[writer].batches;

                // Can't write to this batch any longer.
                queue->writer_batches[writer] = 0;
            }
            else
                // Max number of pending batches reached, can't do anything right now.
                queue->lock_block = block = 1;
        }

        else
            // Oops, pending queue is completely filled up already. Need to
            // block.
            block = 1;

        _release_lock(queue, i, 0, writer);

        if ( block ) {
            ++queue->writer_stats[writer].blocked;
            // Sleep a tiny bit.
            // pthread_yield();
            hlt_util_nanosleep(1000);
        }

        pthread_testcancel();

    } while ( block );

    assert(! queue->writer_batches[writer]);
}

void* hlt_thread_queue_read(hlt_thread_queue* queue, int timeout)
{
    int block = (timeout == 0);

    timeout *= 1000; // Turn it into nanoseconds.

    while ( 1 ) {
        while ( queue->reader_head ) {
            // We still have stuff to do, so do it.
            batch* b = queue->reader_head;

            if ( queue->reader_pos < b->write_pos ) {
                // Still something in our current batch.
                ++queue->reader_stats->elems;
                ++queue->reader_num_read;
                return b->elems[queue->reader_pos++];
            }

            // Switch to next batch.
            queue->reader_head = queue->reader_head->next;
            queue->reader_pos = 0;
            ++queue->reader_stats->batches;
        }

        pthread_testcancel();

        // Nothing left anymore, get a currently pending batch.
        int s;
        _acquire_lock(queue, &s, 1, 0);
        ++queue->reader_stats->locked;

        queue->reader_head = queue->lock_pending_head;
        queue->reader_pos = 0;
        queue->lock_pending_head = queue->lock_pending_tail = 0;
        queue->lock_num_pending = 0;
        queue->lock_block = 0;

        // Take the opportunity to check who has terminated.
        queue->reader_num_terminated = 0;

        for ( int i = 0; i < queue->writers; ++i ) {
            if ( queue->lock_writers_terminated[i] )
                ++queue->reader_num_terminated;
        }

        _release_lock(queue, s, 1, 0);

        if ( ! queue->reader_head ) {
            // Nothing was pending actually ...
            ++queue->reader_stats->blocked;

            if ( timeout <= 0 && ! block )
                return 0;

            if ( hlt_thread_queue_terminated(queue) )
                return 0;

            // Sleep a tiny bit.
            // pthread_yield();
            hlt_util_nanosleep(1000);
            timeout -= 1000;
        }

    }

    // Can't be reached.
    assert(0);
}

int8_t hlt_thread_can_read(hlt_thread_queue* queue)
{
    return (queue->reader_head != 0);
}

uint64_t hlt_thread_queue_size(hlt_thread_queue* queue)
{
    // We're accessing the counters here without locking, which may get us
    // wrong results occasionally. That's fine, we're just gueesing.
    uint64_t size = 0;
    for ( int i = 0; i < queue->writers; i++ )
        size += queue->writer_num_written[i];

    return size - queue->reader_num_read;
}

uint64_t hlt_thread_queue_pending(hlt_thread_queue* queue)
{
    return queue->lock_num_pending;
}

void hlt_thread_queue_terminate_writer(hlt_thread_queue* queue, int writer)
{
    int s;
    _acquire_lock(queue, &s, 0, writer);
    queue->lock_writers_terminated[writer] = 1;
    _release_lock(queue, s, 0, writer);

    hlt_thread_queue_flush(queue, writer);

}

int8_t hlt_thread_queue_terminated(hlt_thread_queue* queue)
{
    // We can rely on size() here because if all writers have terminated
    // already, we'll get a useful result (eventually).
    return (queue->reader_num_terminated == queue->writers) && (hlt_thread_queue_size(queue) == 0);
}

const hlt_thread_queue_stats* hlt_thread_queue_stats_reader(hlt_thread_queue* queue)
{
    return queue->reader_stats;
}

const hlt_thread_queue_stats* hlt_thread_queue_stats_writer(hlt_thread_queue* queue, int writer)
{
    return &queue->writer_stats[writer];
}

