/* ft-reader.c
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

#include <string.h>
#include <stdlib.h>

#include "ft-reader.h"

/**
 * SECTION:ft-reader.h
 * @title: FtReader
 * @short_description: A parser for ftrace logs.
 *
 * #FtReader can read ftrace log files.
 */

G_DEFINE_TYPE(FtReader, ft_reader, G_TYPE_OBJECT)

struct _FtReaderPrivate
{
	FtReaderMode  mode;
	GIOChannel   *channel;
};

/**
 * ft_reader_new:
 *
 * Creates a new instance of #FtReader.
 *
 * Returns: the newly created instance of #FtReader.
 * Side effects: None.
 */
FtReader*
ft_reader_new (void)
{
	FtReader *reader;

	reader = g_object_new(FT_TYPE_READER, NULL);
	return reader;
}

/**
 * ft_reader_set_mode:
 * @reader: A #FtReader.
 *
 * XXX
 *
 * Returns: None.
 * Side effects: None.
 */
void
ft_reader_set_mode (FtReader     *reader, /* IN */
                    FtReaderMode  mode)   /* IN */
{
	FtReaderPrivate *priv;

	g_return_if_fail(FT_IS_READER(reader));

	priv = reader->priv;
	priv->mode = mode;
}

/**
 * ft_reader_func_name:
 * @reader: A #FtReader.
 *
 * XXX
 *
 * Returns: None.
 * Side effects: None.
 */
FtReaderMode
ft_reader_get_mode (FtReader *reader) /* IN */
{
	FtReaderPrivate *priv;

	g_return_val_if_fail(FT_IS_READER(reader), 0);

	priv = reader->priv;
	return priv->mode;
}

gboolean
ft_reader_read_function_graph (FtReader *reader, /* IN */
                               FtEvent  *event)  /* OUT */
{
	static GString *str = NULL;
	FtReaderPrivate *priv;
	GIOStatus status;
	gboolean ret = FALSE;
	gsize len;
	gint pos;
	gint i;

	g_return_val_if_fail(FT_IS_READER(reader), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);
	g_return_val_if_fail(reader->priv->channel != NULL, FALSE);

	priv = reader->priv;
	if (!str) {
		str = g_string_new(NULL);
	}

  next:
	memset(event, 0, sizeof(*event));

  	/*
  	 * Read the next line from the file.
  	 */
	status = g_io_channel_read_line_string(priv->channel, str, &len, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		goto cleanup;
	}
	g_strchomp(str->str);
	if (!(str->len = strlen(str->str))) {
		goto next;
	}

	/*
	 * Skip comment lines.
	 */
	if (str->str[0] == '#') {
		goto next;
	}

	/*
	 * Look for the CPU id.
	 */
	pos = -1;
	for (i = 0; str->str[i]; i++) {
		switch (str->str[i]) {
		case '-':
			if (pos < 0) {
				goto next;
			}
			break;
		case ' ':
			break;
		case '<':
			goto next;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (pos < 0) {
				pos = i;
			}
			continue;
		case ')':
			str->str[i] = ' ';
			if (pos < 0) {
				goto cleanup;
			}
			event->call.cpu = atoi(str->str + pos);
			goto tags;
		default:
			/*
			 * Failed to get CPU id.
			 */
			goto cleanup;
		}
	}

	/*
	 * Collect tags up to time.
	 */
  tags:
	pos = -1;
	for (i++; str->str[i]; i++) {
		switch (str->str[i]) {
		case '|':
			event->type = FT_EVENT_CALL_ENTRY;
			goto name;
		case '+':
		case '!':
			continue;
		case '.':
			if (pos < 0) {
				goto cleanup;
			}
			str->str[i] = ' ';
			event->call.duration.tv_sec = atoi(str->str + pos);
			pos = -1;
			continue;
		case ' ':
			if (str->str[i + 1] == 'u') {
				if (pos < 0) {
					goto cleanup;
				}
				event->call.duration.tv_usec = atoi(str->str + pos);
				goto type;
			}
			continue;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (pos < 0) {
				pos = i;
			}
			continue;
		default:
			/*
			 * Must be a line of switching process. Skip.
			 */
			goto next;
		}
	}

  type:
  	switch (str->str[str->len - 1]) {
  	case ';':
		event->type = FT_EVENT_CALL;
		goto name;
	case '}':
		event->type = FT_EVENT_CALL_EXIT;
		ret = TRUE;
		goto cleanup;
	default:
		g_warning("Invalid trailing character: (%c) %s", str->str[str->len - 1], str->str);
		g_assert_not_reached();
	}

  name:
  	for (; str->str[i]; i++) {
  		switch (str->str[i]) {
  		case '|':
  			memcpy(event->call.name, &str->str[i + 2], str->len - i - 2);
  			goto scrub;
  		default:
  			break;
		}
	}
	goto cleanup;

  scrub:
  	g_strstrip(event->call.name);
  	for (i = 0; event->call.name[i]; i++) {
  		switch (event->call.name[i]) {
  		case '(':
  		case ')':
  		case ';':
  		case '{':
  		case ' ':
  			event->call.name[i] = '\0';
  			goto done;
  		default:
  			break;
		}
	}

  done:
  	ret = TRUE;

	/*
	 * Get the lines time.
	 */

  cleanup:
  	/*
  	 * Leave string around for next line.
  	 * g_string_free(str, TRUE);
  	 */
  	return ret;
}

gboolean
ft_reader_read (FtReader *reader, /* IN */
                FtEvent  *event)  /* OUT */
{
	gboolean ret = FALSE;

	g_return_val_if_fail(FT_IS_READER(reader), FALSE);

	switch (reader->priv->mode) {
	case FT_READER_FUNCTION_GRAPH:
		ret = ft_reader_read_function_graph(reader, event);
		break;
	default:
		g_assert_not_reached();
	}
	return ret;
}

GQuark
ft_reader_error_quark (void)
{
	return g_quark_from_static_string("ft-reader-error-quark");
}

gboolean
ft_reader_load_from_file (FtReader     *reader,
                          const gchar  *filename,
                          GError      **error)
{
	FtReaderPrivate *priv;

	g_return_val_if_fail(FT_IS_READER(reader), FALSE);
	g_return_val_if_fail(filename != NULL, FALSE);

	priv = reader->priv;
	/*
	 * TODO: Support loading new files.
	 */
	if (priv->channel) {
		g_set_error(error, ft_reader_error_quark(), 0,
		            "File already loaded in reader");
		return FALSE;
	}
	/*
	 * Load file as GIOChannel.
	 */
	if (!(priv->channel = g_io_channel_new_file(filename, "r", error))) {
		return FALSE;
	}
	return TRUE;
}

/**
 * ft_reader_finalize:
 * @object: A #FtReader.
 *
 * Finalizer for a #FtReader instance.  Frees any resources held by
 * the instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
ft_reader_finalize (GObject *object) /* IN */
{
	G_OBJECT_CLASS(ft_reader_parent_class)->finalize(object);
}

/**
 * ft_reader_class_init:
 * @klass: A #FtReaderClass.
 *
 * Initializes the #FtReaderClass and prepares the vtable.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
ft_reader_class_init (FtReaderClass *klass) /* IN */
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = ft_reader_finalize;
	g_type_class_add_private(object_class, sizeof(FtReaderPrivate));
}

/**
 * ft_reader_init:
 * @reader: A #FtReader.
 *
 * Initializes the newly created #FtReader instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
ft_reader_init (FtReader *reader) /* IN */
{
	FtReaderPrivate *priv;

	reader->priv = G_TYPE_INSTANCE_GET_PRIVATE(reader,
	                                           FT_TYPE_READER,
	                                           FtReaderPrivate);
	priv = reader->priv;
	priv->mode = FT_READER_FUNCTION_GRAPH;
}
