/*
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling special schannel structures

   Copyright (C) Guenther Deschner 2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

void ndr_print_NL_AUTH_MESSAGE_BUFFER(struct ndr_print *ndr, const char *name, const union NL_AUTH_MESSAGE_BUFFER *r);
void ndr_print_NL_AUTH_MESSAGE_BUFFER_REPLY(struct ndr_print *ndr, const char *name, const union NL_AUTH_MESSAGE_BUFFER_REPLY *r);
void dump_NL_AUTH_SIGNATURE(TALLOC_CTX *mem_ctx,
			    const DATA_BLOB *blob);
