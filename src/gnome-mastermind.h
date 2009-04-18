/*
 * GNOME Mastermind
 *
 * Authors:
 *  Filippo Argiolas <filippo.argiolas@gmail.com>  (2008)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Other copyright information shoud be found on copyright files
 * shipped with this package
 *
 */


#ifndef __GNOME_MASTERMIND_H__
#define __GNOME_MASTERMIND_H__

G_BEGIN_DECLS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

/* i18n stuff */

#ifdef ENABLE_NLS
#include <glib/gi18n.h>
#else /* !ENABLE_NLS */
#define _(String) (String)
#define N_(String) (String)
#endif /* ENABLE_NLS */

G_END_DECLS

#endif /* __GNOME_MASTERMIND_H__ */
