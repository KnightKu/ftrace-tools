/* func-overview.c
 *
 * Copyright (C) 2010 Christian Hergert <chris@dronelabs.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ft-reader.h"

#define MAX_CPU 32

static GHashTable *calls = NULL;
static volatile gboolean need_disable = FALSE;

static void
set_tracer (const gchar *tracer)
{
	int fd;

	fd = open("/sys/kernel/debug/tracing/current_tracer", O_WRONLY, 0);
	if (fd < 0) {
		g_printerr("Cannot set tracer to %s. Exiting.\n", tracer);
		exit(1);
	}
	if (write(fd, tracer, strlen(tracer) + 1) != strlen(tracer) + 1) {
		g_printerr("Cannot set tracer to %s. Exiting.\n", tracer);
		exit(1);
	}
	close(fd);
}

static void
enable_tracer (void)
{
	int fd;

	fd = open("/sys/kernel/debug/tracing/tracing_enabled", O_WRONLY, 0);
	if (fd < 0) {
		g_printerr("Cannot enable tracer. Exiting.\n");
		exit(2);
	}
	if (write(fd, "1\n", 3) != 3) {
		g_printerr("Cannot enable tracer. Exiting.\n");
		exit(3);
	}
	close(fd);
	g_print("Tracing enabled.\n");
}

static void
disable_tracer (void)
{
	int fd;

	fd = open("/sys/kernel/debug/tracing/tracing_enabled", O_WRONLY, 0);
	if (fd < 0) {
		g_printerr("Cannot disable tracer. Exiting.\n");
		exit(4);
	}
	if (write(fd, "0\n", 3) != 3) {
		g_printerr("Cannot enable tracer. Exiting.\n");
		exit(5);
	}
	close(fd);
	g_print("Tracing disabled.\n");
}

static inline void
clear_trace (void)
{
	int fd;

	fd = open("/sys/kernel/debug/tracing/trace", O_WRONLY, 0);
	if (fd < 0) {
		g_printerr("Cannot clear trace. Exiting.\n");
		exit(6);
	}
	if (write(fd, "\n", 2) != 2) {
		g_printerr("Cannot clear trace. Exiting.\n");
		exit(7);
	}
	close(fd);
}

static void
sig_handler (gint sig)
{
	if (need_disable) {
		g_print("\nSIGINT caught; disabling tracing.\n");
		disable_tracer();
		need_disable = FALSE;
	} else {
		exit(255);
	}
}

static void
handle_call (FtEvent *begin, /* IN */
             FtEvent *end)   /* IN */
{
	GTimeVal *dur;
	GList *list = NULL;

	static gint count = 0;

	count++;
	if ((count % 1000) == 0) {
		g_print("call count=%d\n", count);
	}

	list = g_hash_table_lookup(calls, begin->call.name);
	dur = g_slice_new0(GTimeVal);
	*dur = end->call.duration;
	list = g_list_prepend(list, dur);
	g_hash_table_insert(calls, begin->call.name, list);

#if 0
	g_print("[%05ld.%06ld] ==> %s();\n",
	        end->call.duration.tv_sec, end->call.duration.tv_usec,
	        begin->call.name);
#endif
}

static gint
sort_time_val (GTimeVal *a,
               GTimeVal *b)
{
	if (a->tv_sec < b->tv_sec) {
		return -1;
	} else if (a->tv_sec > b->tv_sec) {
		return 1;
	}
	return a->tv_usec - b->tv_usec;
}

static void
calculate (void)
{
	GTimeVal cum = { 0 };
	GTimeVal *dur;
	GTimeVal *med;
	GHashTableIter iter;
	gchar *key;
	GList *val;
	GList *list;
	gfloat avg;
	gfloat medf;
	gint count;

	g_print("%40s | %14s | %14s | %14s | %5s\n",
	         "Function", "Cumulative", "Mean", "Median", "Count");
	g_print("====================================================================================================\n");
	g_hash_table_iter_init(&iter, calls);
	while (g_hash_table_iter_next(&iter, (gpointer *)&key, (gpointer *)&val)) {
		count = 0;
		cum.tv_sec = 0;
		cum.tv_usec = 0;
		for (list = val; list; list = list->next) {
			dur = list->data;
			cum.tv_sec += dur->tv_sec;
			g_time_val_add(&cum, dur->tv_usec);
			count++;
		}
		list = g_list_copy(val);
		list = g_list_sort(list, (GCompareFunc)sort_time_val);
		if (count == 1) {
			med = list->data;
			medf = ((gfloat)med->tv_sec + (gfloat)(med->tv_usec / (gfloat)G_USEC_PER_SEC));
		} else if ((count % 2) == 1) {
			med = g_list_nth_data(list, count / 2);
			medf = ((gfloat)med->tv_sec + (gfloat)(med->tv_usec / (gfloat)G_USEC_PER_SEC));
		} else {
			dur = g_list_nth_data(list, count / 2);
			med = g_list_nth_data(list, (count / 2) - 1);
			dur->tv_sec += med->tv_sec;
			g_time_val_add(dur, med->tv_usec);
			medf = ((gfloat)med->tv_sec + (gfloat)(med->tv_usec / (gfloat)G_USEC_PER_SEC)) / 2.;
		}
		g_list_free(list);
		avg = ((gfloat)cum.tv_sec + (gfloat)(cum.tv_usec / (gfloat)G_USEC_PER_SEC))
		    / (gfloat)count;
		g_print("%40s | %7ld.%06ld | %7ld.%06ld | %7ld.%06ld | %5d\n",
		        key, cum.tv_sec, cum.tv_usec,
		        (glong)avg, (glong)((avg - floor(avg)) * 1000000),
		        (glong)medf, (glong)((medf - floor(medf)) * 1000000),
		        count);
	}
}

gint
main (gint   argc,   /* IN */
      gchar *argv[]) /* IN */
{
	FtReader *reader;
	FtEvent event;
	FtEvent *ptr;
	GQueue *stack[MAX_CPU] = { 0 };
	GError *error = NULL;
	const gchar *filename = NULL;
	gint i;

	g_type_init();

	if (argc == 1) {
		if (getuid() != 0) {
			goto usage;
		}
		goto enable;
	} else if (argc != 2) {
		goto usage;
	} else {
		filename = argv[1];
		goto read_trace;
	}

  enable:
  	signal(SIGINT, sig_handler);
  	clear_trace();
	set_tracer("function_graph");
	g_print("Tracing for 10 seconds.\n");
	need_disable = TRUE;
	enable_tracer();
	sleep(10);
	if (need_disable) {
		need_disable = FALSE;
		disable_tracer();
	}
	filename = "/sys/kernel/debug/tracing/trace";

  read_trace:
	if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
		goto usage;
	}

	for (i = 0; i < MAX_CPU; i++) {
		stack[i] = g_queue_new();
	}
	calls = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

	/*
	 * Create reader and load trace file.
	 */
	reader = ft_reader_new();
	ft_reader_set_mode(reader, FT_READER_FUNCTION_GRAPH);
	if (!ft_reader_load_from_file(reader, filename, &error)) {
		g_printerr("Error loading file: %s\n", error->message);
		return EXIT_FAILURE;
	}

	g_print("Parsing trace output ...\n");

	/*
	 * Read events.
	 */
	while (ft_reader_read(reader, &event)) {
		switch (event.type) {
		case FT_EVENT_CALL_ENTRY:
			ptr = g_slice_new0(FtEvent);
			memcpy(ptr, &event, sizeof(event));
			g_queue_push_head(stack[event.call.cpu], ptr);
			break;
		case FT_EVENT_CALL_EXIT:
			if (!(ptr = g_queue_pop_head(stack[event.call.cpu]))) {
				/*
				 * Must be at beginning of file.  No matching call entry.
				 */
				continue;
			}
			handle_call(ptr, &event);
			g_slice_free(FtEvent, ptr);
			break;
		case FT_EVENT_CALL:
			handle_call(&event, &event);
			break;
		default:
			g_printerr("Unexpected event type in trace: %d.\n", event.type);
			return EXIT_FAILURE;
		}
	}

	g_print("Parsed trace file ...\n");
	g_print("Calculating overview ...\n");

	calculate();

	return EXIT_SUCCESS;

  usage:
  	g_printerr("usage: %s [FILENAME]\n", argv[0]);
  	g_printerr("\n");
  	g_printerr("If no filename is specified, a trace will be performed for 10 seconds.\n");
	g_printerr("Please run %s as root to perform trace.\n", argv[0]);
  	return EXIT_FAILURE;
}
