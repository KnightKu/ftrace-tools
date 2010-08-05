/* ft-reader.h
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

#ifndef __FT_READER_H__
#define __FT_READER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define FT_TYPE_READER            (ft_reader_get_type())
#define FT_READER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FT_TYPE_READER, FtReader))
#define FT_READER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), FT_TYPE_READER, FtReader const))
#define FT_READER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  FT_TYPE_READER, FtReaderClass))
#define FT_IS_READER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FT_TYPE_READER))
#define FT_IS_READER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  FT_TYPE_READER))
#define FT_READER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  FT_TYPE_READER, FtReaderClass))

typedef enum
{
	FT_READER_FUNCTION_GRAPH,
} FtReaderMode;

typedef enum
{
	FT_EVENT_CALL = 1,
	FT_EVENT_CALL_ENTRY,
	FT_EVENT_CALL_EXIT,
} FtEventType;

typedef struct _FtReader         FtReader;
typedef struct _FtReaderClass    FtReaderClass;
typedef struct _FtReaderPrivate  FtReaderPrivate;
typedef union  _FtEvent          FtEvent;
typedef struct _FtEventCall      FtEventCall;
typedef struct _FtEventCallEntry FtEventCallEntry;
typedef struct _FtEventCallExit  FtEventCallExit;

struct _FtReader
{
	GObject parent;

	/*< private >*/
	FtReaderPrivate *priv;
};

struct _FtReaderClass
{
	GObjectClass parent_class;
};

struct _FtEventCall
{
	FtEventType  type;
	gint         cpu;
	GTimeVal     duration;
	gchar        name[128];
};

struct _FtEventCallEntry
{
	FtEventType  type;
	gint         cpu;
	gchar        name[128];
};

struct _FtEventCallExit
{
	FtEventType  type;
	gint         cpu;
	GTimeVal     duration;
};

union _FtEvent
{
	FtEventType      type;
	FtEventCall      call;
	FtEventCallEntry call_entry;
	FtEventCallExit  call_exit;
};

GType        ft_reader_get_type       (void) G_GNUC_CONST;
FtReader*    ft_reader_new            (void);
FtReaderMode ft_reader_get_mode       (FtReader      *reader);
void         ft_reader_set_mode       (FtReader      *reader,
                                       FtReaderMode   mode);
gboolean     ft_reader_read           (FtReader      *reader,
                                       FtEvent       *event);
gboolean     ft_reader_load_from_file (FtReader      *reader,
                                       const gchar   *filename,
                                       GError       **error);

G_END_DECLS

#endif /* __FT_READER_H__ */
