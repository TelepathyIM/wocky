/*
 * wocky-utils-test.c - Tests for utility code
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <wocky/wocky.h>

static void
test_compose_jid (void)
{
  gchar *s = NULL;

  g_assert_cmpstr ((s = wocky_compose_jid ("juliet", "example.com", "Balcony")),
      ==, "juliet@example.com/Balcony");
  g_free (s);

  g_assert_cmpstr ((s = wocky_compose_jid ("juliet", "example.com", NULL)),
      ==, "juliet@example.com");
  g_free (s);

  g_assert_cmpstr ((s = wocky_compose_jid (NULL, "example.com", NULL)),
      ==, "example.com");
  g_free (s);

  g_assert_cmpstr ((s = wocky_compose_jid (NULL, "example.com", "Server")),
      ==, "example.com/Server");
  g_free (s);
}

int
main (int argc,
    char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/utils/compose-jid", test_compose_jid);

  return g_test_run ();
}
