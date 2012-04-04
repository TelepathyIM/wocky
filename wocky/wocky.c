/*
 * wocky.c - General functions
 * Copyright (C) 2009 Collabora Ltd.
 * @author Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
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

#include <libxml/parser.h>

#include "wocky.h"
#include "wocky-node.h"
#include "wocky-xmpp-error.h"

/**
 * wocky_init:
 *
 * Initializes the Wocky library.
 *
 * This function should be called before calling any other Wocky functions.
 */
void
wocky_init (void)
{
  xmlInitParser ();
  wocky_node_init ();
  wocky_xmpp_error_init ();
}

/**
 * wocky_deinit:
 *
 * Clean up any resources created by Wocky in wocky_init().
 *
 * It is normally not needed to call this function in a normal application
 * as the resources will automatically be freed when the program terminates.
 * This function is therefore mostly used by testsuites and other memory
 * profiling tools.
 *
 * After this call Wocky (including this method) should not be used anymore.
 */
void
wocky_deinit (void)
{
  xmlCleanupParser ();
  wocky_node_deinit ();
  wocky_xmpp_error_deinit ();
}
